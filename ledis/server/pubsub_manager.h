#pragma once

#include <string>
#include <string_view>
#include <algorithm>
#include <fnmatch.h>

#include <lstl/container/unordered_map.h>
#include <lstl/container/vector.h>

#include "ledis/server/client_context.h"
#include "ledis/protocol/resp_writer.h"

namespace ledis {

class PubSubManager {
public:
    void subscribe(ClientContext* client, const std::string& channel) {
        auto it = channels_.find(channel);
        if (it == channels_.end()) {
            lstl::vector<ClientContext*> v;
            v.push_back(client);
            channels_.insert({channel, v});
        } else {
            it->second.push_back(client);
        }
        client->channels.insert(channel);
    }

    bool unsubscribe(ClientContext* client, const std::string& channel) {
        auto it = channels_.find(channel);
        if (it != channels_.end()) {
            removeOne(it->second, client);
            if (it->second.empty()) channels_.erase(channel);
        }
        client->channels.erase(channel);
        return true;
    }

    void unsubscribeAll(ClientContext* client) {
        for (auto& ch : client->channels) {
            auto it = channels_.find(ch);
            if (it != channels_.end()) {
                removeOne(it->second, client);
                if (it->second.empty()) channels_.erase(it->first);
            }
        }
        client->channels.clear();
        for (auto& pat : client->patterns) {
            auto it = patterns_.find(pat);
            if (it != patterns_.end()) {
                removeOne(it->second, client);
                if (it->second.empty()) patterns_.erase(it->first);
            }
        }
        client->patterns.clear();
    }

    void psubscribe(ClientContext* client, const std::string& pattern) {
        auto it = patterns_.find(pattern);
        if (it == patterns_.end()) {
            lstl::vector<ClientContext*> v;
            v.push_back(client);
            patterns_.insert({pattern, v});
        } else {
            it->second.push_back(client);
        }
        client->patterns.insert(pattern);
    }

    int publish(const std::string& channel, const std::string& message) {
        int count = 0;
        auto it = channels_.find(channel);
        if (it != channels_.end()) {
            for (auto* c : it->second) { deliverMessage(c, channel, message, false); count++; }
        }
        for (auto& kv : patterns_) {
            if (fnmatch(kv.first.c_str(), channel.c_str(), 0) == 0) {
                for (auto* c : kv.second) { deliverMessage(c, kv.first, message, true); count++; }
            }
        }
        return count;
    }

    bool isSubscribed(ClientContext* client) const {
        return !client->channels.empty() || !client->patterns.empty();
    }
    int subscriptionCount(ClientContext* client) const {
        return static_cast<int>(client->channels.size() + client->patterns.size());
    }

private:
    // Remove first occurrence of `client` from vector (O(n), n is small)
    static void removeOne(lstl::vector<ClientContext*>& vec, ClientContext* client) {
        for (auto it = vec.begin(); it != vec.end(); ++it) {
            if (*it == client) {
                vec.erase(it);
                return;
            }
        }
    }

    void deliverMessage(ClientContext* client, const std::string& channel,
                        const std::string& message, bool is_pattern) {
        std::string& buf = client->write_buf;
        buf.clear();
        if (is_pattern) {
            buf = "*4\r\n$8\r\npmessage\r\n";
            RespWriter::writeBulkStringInline(buf, channel);
            RespWriter::writeBulkStringInline(buf, channel);
            RespWriter::writeBulkStringInline(buf, message);
        } else {
            buf = "*3\r\n$7\r\nmessage\r\n";
            RespWriter::writeBulkStringInline(buf, channel);
            RespWriter::writeBulkStringInline(buf, message);
        }
        client->pubsub_msg.store(true, std::memory_order_release);
        uint64_t v = 1;
        ::write(client->response_event_fd_, &v, sizeof(v));
    }

    lstl::unordered_map<std::string, lstl::vector<ClientContext*>> channels_;
    lstl::unordered_map<std::string, lstl::vector<ClientContext*>> patterns_;
};

} // namespace ledis
