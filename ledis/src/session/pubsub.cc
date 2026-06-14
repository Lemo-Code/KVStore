#include "ledis/session/pubsub.h"

#include "ledis/command/registry.h"
#include "ledis/session/reply_router.h"

#include <cctype>

namespace ledis {
namespace {

bool matchHere(const char* p, const char* pe, const char* t, const char* te) {
  while (p < pe && t < te) {
    if (*p == '\\' && p + 1 < pe) {
      if (*++p != *t) {
        return false;
      }
      ++p;
      ++t;
      continue;
    }
    if (*p == '*') {
      if (p + 1 == pe) {
        return true;
      }
      for (const char* s = t; s <= te; ++s) {
        if (matchHere(p + 1, pe, s, te)) {
          return true;
        }
      }
      return false;
    }
    if (*p == '?') {
      ++p;
      ++t;
      continue;
    }
    if (*p == '[') {
      ++p;
      bool negate = false;
      if (p < pe && *p == '^') {
        negate = true;
        ++p;
      }
      bool matched = false;
      while (p < pe && *p != ']') {
        char lo = *p;
        char hi = lo;
        if (p + 2 < pe && p[1] == '-' && p[2] != ']') {
          hi = p[2];
          p += 2;
        }
        if (*t >= lo && *t <= hi) {
          matched = true;
        }
        ++p;
      }
      if (p < pe && *p == ']') {
        ++p;
      }
      if (negate ? matched : !matched) {
        return false;
      }
      ++t;
      continue;
    }
    if (*p != *t) {
      return false;
    }
    ++p;
    ++t;
  }
  while (p < pe && *p == '*') {
    ++p;
  }
  return p == pe && t == te;
}

}  // namespace

bool pubsubPatternMatch(const String& pattern, const String& text) {
  if (pattern.empty()) {
    return text.empty();
  }
  return matchHere(pattern.data(), pattern.data() + pattern.size(), text.data(),
                    text.data() + text.size());
}

CommandResult PubSubHub::metaReply(const char* kind, const String& channel,
                                   size_t count) {
  RespArray elems;
  elems.push_back(RespValue::makeBulk(Sds(kind)));
  elems.push_back(RespValue::makeBulk(Sds(channel)));
  elems.push_back(RespValue::makeInteger(static_cast<int64_t>(count)));
  return CommandResult::fromValue(RespValue::makeArray(Move(elems)));
}

CommandResult PubSubHub::messageReply(const String& channel,
                                      const String& message) {
  RespArray elems;
  elems.push_back(RespValue::makeBulk(Sds("message")));
  elems.push_back(RespValue::makeBulk(Sds(channel)));
  elems.push_back(RespValue::makeBulk(Sds(message)));
  return CommandResult::fromValue(RespValue::makeArray(Move(elems)));
}

CommandResult PubSubHub::pmessageReply(const String& pattern, const String& channel,
                                       const String& message) {
  RespArray elems;
  elems.push_back(RespValue::makeBulk(Sds("pmessage")));
  elems.push_back(RespValue::makeBulk(Sds(pattern)));
  elems.push_back(RespValue::makeBulk(Sds(channel)));
  elems.push_back(RespValue::makeBulk(Sds(message)));
  return CommandResult::fromValue(RespValue::makeArray(Move(elems)));
}

void PubSubHub::addSubscriber(uint64_t conn_id, uint32_t io_thread_id,
                              const String& channel) {
  conn_channels_[conn_id].insert(channel);
  StdVector<Subscriber>& subs = channel_subs_[channel];
  for (size_t i = 0; i < subs.size(); ++i) {
    if (subs[i].conn_id == conn_id) {
      subs[i].io_thread_id = io_thread_id;
      return;
    }
  }
  Subscriber sub;
  sub.conn_id = conn_id;
  sub.io_thread_id = io_thread_id;
  subs.push_back(sub);
}

void PubSubHub::removeSubscriber(uint64_t conn_id, const String& channel) {
  conn_channels_[conn_id].erase(channel);
  if (conn_channels_[conn_id].empty()) {
    conn_channels_.erase(conn_id);
  }
  const auto it = channel_subs_.find(channel);
  if (it == channel_subs_.end()) {
    return;
  }
  StdVector<Subscriber>& subs = it->second;
  for (size_t i = 0; i < subs.size(); ++i) {
    if (subs[i].conn_id == conn_id) {
      subs.erase(subs.begin() + static_cast<ptrdiff_t>(i));
      break;
    }
  }
  if (subs.empty()) {
    channel_subs_.erase(it);
  }
}

void PubSubHub::addPatternSubscriber(uint64_t conn_id, uint32_t io_thread_id,
                                     const String& pattern) {
  conn_patterns_[conn_id].insert(pattern);
  StdVector<Subscriber>& subs = pattern_subs_[pattern];
  for (size_t i = 0; i < subs.size(); ++i) {
    if (subs[i].conn_id == conn_id) {
      subs[i].io_thread_id = io_thread_id;
      return;
    }
  }
  Subscriber sub;
  sub.conn_id = conn_id;
  sub.io_thread_id = io_thread_id;
  subs.push_back(sub);
}

void PubSubHub::removePatternSubscriber(uint64_t conn_id, const String& pattern) {
  conn_patterns_[conn_id].erase(pattern);
  if (conn_patterns_[conn_id].empty()) {
    conn_patterns_.erase(conn_id);
  }
  const auto it = pattern_subs_.find(pattern);
  if (it == pattern_subs_.end()) {
    return;
  }
  StdVector<Subscriber>& subs = it->second;
  for (size_t i = 0; i < subs.size(); ++i) {
    if (subs[i].conn_id == conn_id) {
      subs.erase(subs.begin() + static_cast<ptrdiff_t>(i));
      break;
    }
  }
  if (subs.empty()) {
    pattern_subs_.erase(it);
  }
}

size_t PubSubHub::pushToChannel(const String& channel, const String& message) {
  const auto it = channel_subs_.find(channel);
  if (it == channel_subs_.end()) {
    return 0;
  }
  if (!router_) {
    return it->second.size();
  }
  const CommandResult msg = messageReply(channel, message);
  size_t delivered = 0;
  for (size_t i = 0; i < it->second.size(); ++i) {
    ReplyEnvelope reply;
    reply.conn_id = it->second[i].conn_id;
    reply.io_thread_id = it->second[i].io_thread_id;
    reply.result = msg;
    reply.unsolicited = true;
    if (router_->tryPush(Move(reply))) {
      ++delivered;
    }
  }
  return delivered;
}

size_t PubSubHub::pushToPatterns(const String& channel, const String& message) {
  if (pattern_subs_.empty()) {
    return 0;
  }
  size_t delivered = 0;
  for (StdUnorderedMap<String, StdVector<Subscriber>>::const_iterator pit =
           pattern_subs_.begin();
       pit != pattern_subs_.end(); ++pit) {
    if (!pubsubPatternMatch(pit->first, channel)) {
      continue;
    }
    if (!router_) {
      delivered += pit->second.size();
      continue;
    }
    const CommandResult msg = pmessageReply(pit->first, channel, message);
    for (size_t i = 0; i < pit->second.size(); ++i) {
      ReplyEnvelope reply;
      reply.conn_id = pit->second[i].conn_id;
      reply.io_thread_id = pit->second[i].io_thread_id;
      reply.result = msg;
      reply.unsolicited = true;
      if (router_->tryPush(Move(reply))) {
        ++delivered;
      }
    }
  }
  return delivered;
}

CommandResult PubSubHub::subscribe(SessionContext& ctx, const Command& cmd) {
  if (cmd.args.empty()) {
    return CommandResult::error("ERR wrong number of arguments for 'subscribe' command");
  }
  CommandResult first;
  bool has_first = false;
  for (size_t i = 0; i < cmd.args.size(); ++i) {
    const String channel = cmd.args[i].str();
    ctx.pubsub_channels.insert(channel);
    addSubscriber(ctx.conn_id, ctx.io_thread_id, channel);
    const size_t count = ctx.pubsub_channels.size() + ctx.pubsub_patterns.size();
    CommandResult one = metaReply("subscribe", channel, count);
    if (!has_first) {
      first = one;
      has_first = true;
    } else {
      first.trailing.push_back(one.value);
    }
  }
  return first;
}

CommandResult PubSubHub::unsubscribe(SessionContext& ctx, const Command& cmd) {
  if (cmd.args.empty()) {
    if (ctx.pubsub_channels.empty()) {
      return metaReply("unsubscribe", String(), ctx.pubsub_patterns.size());
    }
    CommandResult first;
    bool has_first = false;
    StdVector<String> channels;
    for (StdUnorderedSet<String>::const_iterator it = ctx.pubsub_channels.begin();
         it != ctx.pubsub_channels.end(); ++it) {
      channels.push_back(*it);
    }
    for (size_t i = 0; i < channels.size(); ++i) {
      ctx.pubsub_channels.erase(channels[i]);
      removeSubscriber(ctx.conn_id, channels[i]);
      const size_t count = ctx.pubsub_channels.size() + ctx.pubsub_patterns.size();
      CommandResult one = metaReply("unsubscribe", channels[i], count);
      if (!has_first) {
        first = one;
        has_first = true;
      } else {
        first.trailing.push_back(one.value);
      }
    }
    return first;
  }

  CommandResult first;
  bool has_first = false;
  for (size_t i = 0; i < cmd.args.size(); ++i) {
    const String channel = cmd.args[i].str();
    ctx.pubsub_channels.erase(channel);
    removeSubscriber(ctx.conn_id, channel);
    const size_t count = ctx.pubsub_channels.size() + ctx.pubsub_patterns.size();
    CommandResult one = metaReply("unsubscribe", channel, count);
    if (!has_first) {
      first = one;
      has_first = true;
    } else {
      first.trailing.push_back(one.value);
    }
  }
  return first;
}

CommandResult PubSubHub::psubscribe(SessionContext& ctx, const Command& cmd) {
  if (cmd.args.empty()) {
    return CommandResult::error("ERR wrong number of arguments for 'psubscribe' command");
  }
  CommandResult first;
  bool has_first = false;
  for (size_t i = 0; i < cmd.args.size(); ++i) {
    const String pattern = cmd.args[i].str();
    ctx.pubsub_patterns.insert(pattern);
    addPatternSubscriber(ctx.conn_id, ctx.io_thread_id, pattern);
    const size_t count = ctx.pubsub_channels.size() + ctx.pubsub_patterns.size();
    CommandResult one = metaReply("psubscribe", pattern, count);
    if (!has_first) {
      first = one;
      has_first = true;
    } else {
      first.trailing.push_back(one.value);
    }
  }
  return first;
}

CommandResult PubSubHub::punsubscribe(SessionContext& ctx, const Command& cmd) {
  if (cmd.args.empty()) {
    if (ctx.pubsub_patterns.empty()) {
      return metaReply("punsubscribe", String(), ctx.pubsub_channels.size());
    }
    CommandResult first;
    bool has_first = false;
    StdVector<String> patterns;
    for (StdUnorderedSet<String>::const_iterator it = ctx.pubsub_patterns.begin();
         it != ctx.pubsub_patterns.end(); ++it) {
      patterns.push_back(*it);
    }
    for (size_t i = 0; i < patterns.size(); ++i) {
      ctx.pubsub_patterns.erase(patterns[i]);
      removePatternSubscriber(ctx.conn_id, patterns[i]);
      const size_t count = ctx.pubsub_channels.size() + ctx.pubsub_patterns.size();
      CommandResult one = metaReply("punsubscribe", patterns[i], count);
      if (!has_first) {
        first = one;
        has_first = true;
      } else {
        first.trailing.push_back(one.value);
      }
    }
    return first;
  }

  CommandResult first;
  bool has_first = false;
  for (size_t i = 0; i < cmd.args.size(); ++i) {
    const String pattern = cmd.args[i].str();
    ctx.pubsub_patterns.erase(pattern);
    removePatternSubscriber(ctx.conn_id, pattern);
    const size_t count = ctx.pubsub_channels.size() + ctx.pubsub_patterns.size();
    CommandResult one = metaReply("punsubscribe", pattern, count);
    if (!has_first) {
      first = one;
      has_first = true;
    } else {
      first.trailing.push_back(one.value);
    }
  }
  return first;
}

CommandResult PubSubHub::publish(const Command& cmd) {
  if (cmd.args.size() != 2) {
    return CommandResult::error("ERR wrong number of arguments for 'publish' command");
  }
  const String& channel = cmd.args[0].str();
  const String& message = cmd.args[1].str();
  const size_t n = pushToChannel(channel, message) + pushToPatterns(channel, message);
  return CommandResult::integer(static_cast<int64_t>(n));
}

CommandResult PubSubHub::pubsubChannels(const Command& cmd) const {
  if (cmd.args.size() > 1) {
    return CommandResult::error("ERR wrong number of arguments for 'pubsub' command");
  }
  String pattern = cmd.args.empty() ? "*" : cmd.args[0].str();
  RespArray elems;
  for (StdUnorderedMap<String, StdVector<Subscriber>>::const_iterator it =
           channel_subs_.begin();
       it != channel_subs_.end(); ++it) {
    if (pattern == "*" || pubsubPatternMatch(pattern, it->first)) {
      elems.push_back(RespValue::makeBulk(Sds(it->first)));
    }
  }
  return CommandResult::fromValue(RespValue::makeArray(Move(elems)));
}

CommandResult PubSubHub::pubsubNumsub(const Command& cmd) const {
  if (cmd.args.empty()) {
    return CommandResult::error("ERR wrong number of arguments for 'pubsub' command");
  }
  RespArray elems;
  for (size_t i = 0; i < cmd.args.size(); ++i) {
    const String& channel = cmd.args[i].str();
    size_t count = 0;
    const auto it = channel_subs_.find(channel);
    if (it != channel_subs_.end()) {
      count = it->second.size();
    }
    elems.push_back(RespValue::makeBulk(Sds(channel)));
    elems.push_back(RespValue::makeInteger(static_cast<int64_t>(count)));
  }
  return CommandResult::fromValue(RespValue::makeArray(Move(elems)));
}

CommandResult PubSubHub::pubsubNumpat(const Command& cmd) const {
  if (!cmd.args.empty()) {
    return CommandResult::error("ERR wrong number of arguments for 'pubsub' command");
  }
  return CommandResult::integer(static_cast<int64_t>(pattern_subs_.size()));
}

void PubSubHub::disconnect(uint64_t conn_id) {
  const auto it = conn_channels_.find(conn_id);
  if (it != conn_channels_.end()) {
    for (StdUnorderedSet<String>::const_iterator ch = it->second.begin();
         ch != it->second.end(); ++ch) {
      const auto cit = channel_subs_.find(*ch);
      if (cit == channel_subs_.end()) {
        continue;
      }
      StdVector<Subscriber>& subs = cit->second;
      for (size_t i = 0; i < subs.size(); ++i) {
        if (subs[i].conn_id == conn_id) {
          subs.erase(subs.begin() + static_cast<ptrdiff_t>(i));
          break;
        }
      }
      if (subs.empty()) {
        channel_subs_.erase(cit);
      }
    }
    conn_channels_.erase(it);
  }

  const auto pit = conn_patterns_.find(conn_id);
  if (pit != conn_patterns_.end()) {
    for (StdUnorderedSet<String>::const_iterator pat = pit->second.begin();
         pat != pit->second.end(); ++pat) {
      const auto cit = pattern_subs_.find(*pat);
      if (cit == pattern_subs_.end()) {
        continue;
      }
      StdVector<Subscriber>& subs = cit->second;
      for (size_t i = 0; i < subs.size(); ++i) {
        if (subs[i].conn_id == conn_id) {
          subs.erase(subs.begin() + static_cast<ptrdiff_t>(i));
          break;
        }
      }
      if (subs.empty()) {
        pattern_subs_.erase(cit);
      }
    }
    conn_patterns_.erase(pit);
  }
}

void RegisterPubSubCommands(CommandRegistry* registry, PubSubHub* hub) {
  if (!registry || !hub) {
    return;
  }
  registry->registerHandler("SUBSCRIBE", [hub](SessionContext& ctx, DBManager& db,
                                                 const Command& cmd) {
    (void)db;
    return hub->subscribe(ctx, cmd);
  });
  registry->registerHandler("UNSUBSCRIBE", [hub](SessionContext& ctx, DBManager& db,
                                                   const Command& cmd) {
    (void)db;
    return hub->unsubscribe(ctx, cmd);
  });
  registry->registerHandler("PSUBSCRIBE", [hub](SessionContext& ctx, DBManager& db,
                                                  const Command& cmd) {
    (void)db;
    return hub->psubscribe(ctx, cmd);
  });
  registry->registerHandler("PUNSUBSCRIBE", [hub](SessionContext& ctx, DBManager& db,
                                                    const Command& cmd) {
    (void)db;
    return hub->punsubscribe(ctx, cmd);
  });
  registry->registerHandler("PUBLISH", [hub](SessionContext& ctx, DBManager& db,
                                               const Command& cmd) {
    (void)ctx;
    (void)db;
    return hub->publish(cmd);
  });
  registry->registerHandler("PUBSUB", [hub](SessionContext& ctx, DBManager& db,
                                              const Command& cmd) {
    (void)ctx;
    (void)db;
    if (cmd.args.empty()) {
      return CommandResult::error("ERR wrong number of arguments for 'pubsub' command");
    }
    String sub = cmd.args[0].str();
    for (size_t i = 0; i < sub.size(); ++i) {
      sub[i] = static_cast<char>(std::tolower(static_cast<unsigned char>(sub[i])));
    }
    Command tail;
    for (size_t i = 1; i < cmd.args.size(); ++i) {
      tail.args.push_back(cmd.args[i]);
    }
    if (sub == "channels") {
      return hub->pubsubChannels(tail);
    }
    if (sub == "numsub") {
      return hub->pubsubNumsub(tail);
    }
    if (sub == "numpat") {
      return hub->pubsubNumpat(tail);
    }
    return CommandResult::error("ERR Unknown subcommand or wrong number of arguments for 'pubsub'");
  });
}

}  // namespace ledis
