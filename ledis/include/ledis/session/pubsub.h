#pragma once

#include "ledis/command/command.h"
#include "ledis/command/command_result.h"
#include "ledis/session/session_context.h"
#include "ledis/types.h"

namespace ledis {

class CommandRegistry;
class ReplyRouter;

/** 进程内 Pub/Sub 频道表；PUBLISH 经 ReplyRouter 推送到订阅连接。 */
class PubSubHub {
 public:
  void setReplyRouter(ReplyRouter* router) { router_ = router; }

  CommandResult subscribe(SessionContext& ctx, const Command& cmd);
  CommandResult unsubscribe(SessionContext& ctx, const Command& cmd);
  CommandResult psubscribe(SessionContext& ctx, const Command& cmd);
  CommandResult punsubscribe(SessionContext& ctx, const Command& cmd);
  CommandResult publish(const Command& cmd);
  CommandResult pubsubChannels(const Command& cmd) const;
  CommandResult pubsubNumsub(const Command& cmd) const;
  CommandResult pubsubNumpat(const Command& cmd) const;
  void disconnect(uint64_t conn_id);

 private:
  struct Subscriber {
    uint64_t conn_id = 0;
    uint32_t io_thread_id = 0;
  };

  static CommandResult metaReply(const char* kind, const String& channel,
                                 size_t count);
  static CommandResult messageReply(const String& channel, const String& message);
  static CommandResult pmessageReply(const String& pattern, const String& channel,
                                     const String& message);
  void addSubscriber(uint64_t conn_id, uint32_t io_thread_id,
                     const String& channel);
  void removeSubscriber(uint64_t conn_id, const String& channel);
  void addPatternSubscriber(uint64_t conn_id, uint32_t io_thread_id,
                            const String& pattern);
  void removePatternSubscriber(uint64_t conn_id, const String& pattern);
  size_t pushToChannel(const String& channel, const String& message);
  size_t pushToPatterns(const String& channel, const String& message);

  ReplyRouter* router_ = nullptr;
  StdUnorderedMap<String, StdVector<Subscriber>> channel_subs_;
  StdUnorderedMap<String, StdVector<Subscriber>> pattern_subs_;
  StdUnorderedMap<uint64_t, StdUnorderedSet<String>> conn_channels_;
  StdUnorderedMap<uint64_t, StdUnorderedSet<String>> conn_patterns_;
};

/** Redis 风格 glob：* ? [abc] \\ */
bool pubsubPatternMatch(const String& pattern, const String& text);

void RegisterPubSubCommands(CommandRegistry* registry, PubSubHub* hub);

}  // namespace ledis
