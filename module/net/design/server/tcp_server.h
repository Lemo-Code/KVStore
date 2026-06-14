#ifndef NET_DESIGN_SERVER_TCP_SERVER_H
#define NET_DESIGN_SERVER_TCP_SERVER_H

#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace net {

class Address;
class Runtime;
class Socket;

using ConnectionHandler = std::function<void(Socket::ptr)>;

/**
 * @brief TCP 服务端：bind + accept + 分发到 worker Runtime。
 */
class TcpServer {
 public:
  using ptr = std::shared_ptr<TcpServer>;

  TcpServer(const std::string& name, std::vector<Runtime*> workers);
  ~TcpServer();

  bool bind(const Address* addr);
  bool start();
  void stop();

  void setConnectionHandler(ConnectionHandler cb);

 private:
  std::string name_;
  std::vector<Runtime*> workers_;
  ConnectionHandler handler_;
};

}  // namespace net

#endif  // NET_DESIGN_SERVER_TCP_SERVER_H
