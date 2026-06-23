#include "kvstore/server/client_session.h"
namespace zero { namespace kvstore {
Status ClientSession::Process(const std::string& input, std::string& output) {
    return handler_(input, output);
}
}} // namespace
