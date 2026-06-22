#pragma once

#include <string>
#include <string_view>
#include <fnmatch.h>
#include <lstl/container/unordered_map.h>
#include <lstl/container/vector.h>

#include "kv_ledis/protocol/resp_writer.h"

namespace ledis {

struct Session;

class PubSubManager {
public:
    // channel 订阅
    int subscribe(Session* s, std::string_view channel) {
        std::string ch(channel);
        auto& vec = channels_[ch];
        for (auto* p : vec) if (p == s) return 0;
        vec.push_back(s);
        s->channels.insert(ch);
        return 1;
    }

    int unsubscribe(Session* s, std::string_view channel) {
        std::string ch(channel);
        auto it = channels_.find(ch);
        if (it != channels_.end()) {
            auto& vec = it->second;
            for (size_t i = 0; i < vec.size(); ++i) {
                if (vec[i] == s) { vec.erase(vec.begin() + static_cast<long>(i)); break; }
            }
            s->channels.erase(ch);
            if (vec.empty()) channels_.erase(ch);
            return 1;
        }
        s->channels.erase(ch);
        return 0;
    }

    int unsubscribeAll(Session* s) {
        int count = 0;
        for (auto& ch : s->channels) {
            auto it = channels_.find(ch);
            if (it != channels_.end()) {
                auto& vec = it->second;
                for (size_t i = 0; i < vec.size(); ++i)
                    if (vec[i] == s) { vec.erase(vec.begin() + static_cast<long>(i)); break; }
                if (vec.empty()) channels_.erase(ch);
                count++;
            }
        }
        s->channels.clear();
        return count;
    }

    int psubscribe(Session* s, std::string_view pattern) {
        std::string pat(pattern);
        auto& vec = patterns_[pat];
        for (auto* p : vec) if (p == s) return 0;
        vec.push_back(s);
        s->patterns.insert(pat);
        return 1;
    }

    int punsubscribe(Session* s, std::string_view pattern) {
        std::string pat(pattern);
        auto it = patterns_.find(pat);
        if (it != patterns_.end()) {
            auto& vec = it->second;
            for (size_t i = 0; i < vec.size(); ++i)
                if (vec[i] == s) { vec.erase(vec.begin() + static_cast<long>(i)); break; }
            s->patterns.erase(pat);
            if (vec.empty()) patterns_.erase(pat);
            return 1;
        }
        s->patterns.erase(pat);
        return 0;
    }

    int punsubscribeAll(Session* s) {
        int count = 0;
        for (auto& pat : s->patterns) {
            auto it = patterns_.find(pat);
            if (it != patterns_.end()) {
                auto& vec = it->second;
                for (size_t i = 0; i < vec.size(); ++i)
                    if (vec[i] == s) { vec.erase(vec.begin() + static_cast<long>(i)); break; }
                if (vec.empty()) patterns_.erase(pat);
                count++;
            }
        }
        s->patterns.clear();
        return count;
    }

    int publish(std::string_view channel, std::string_view message) {
        int count = 0;
        std::string ch(channel);

        auto writeMsg = [&](Session* s) {
            std::string& buf = s->pubsub_buf;
            RespWriter::writeArrayHeader(buf, 3);
            RespWriter::writeBulkString(buf, "message");
            RespWriter::writeBulkString(buf, channel);
            RespWriter::writeBulkString(buf, message);
        };

        // channel 订阅者
        auto chit = channels_.find(ch);
        if (chit != channels_.end())
            for (auto* s : chit->second) { writeMsg(s); count++; }

        // pattern 订阅者
        for (auto& kv : patterns_) {
            if (fnmatch(kv.first.c_str(), ch.c_str(), 0) == 0)
                for (auto* s : kv.second) { writeMsg(s); count++; }
        }
        return count;
    }

    lstl::vector<std::string> pubsubChannels(std::string_view pattern) {
        lstl::vector<std::string> result;
        for (auto& kv : channels_) {
            if (pattern.empty() || pattern == "*" ||
                fnmatch(std::string(pattern).c_str(), kv.first.c_str(), 0) == 0)
                result.push_back(kv.first);
        }
        return result;
    }

    int pubsubNumsub(std::string_view channel) {
        auto it = channels_.find(std::string(channel));
        return it != channels_.end() ? static_cast<int>(it->second.size()) : 0;
    }

    int pubsubNumpat() { return static_cast<int>(patterns_.size()); }

    void cleanup(Session* s) {
        unsubscribeAll(s);
        punsubscribeAll(s);
    }

private:
    lstl::unordered_map<std::string, lstl::vector<Session*>> channels_;
    lstl::unordered_map<std::string, lstl::vector<Session*>> patterns_;
};

} // namespace ledis
