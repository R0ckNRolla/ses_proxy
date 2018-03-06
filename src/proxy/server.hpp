#pragma once

#include <list>
#include <memory>
#include <functional>
#include <boost/uuid/uuid.hpp>
#include <boost/asio/io_service.hpp>

#include "net/server/server.hpp"
#include "proxy/client.hpp"
#include "proxy/algorithm.hpp"

namespace ses {
namespace proxy {

class Server : public std::enable_shared_from_this<ses::proxy::Server>
{
public:
  typedef std::shared_ptr<ses::proxy::Server> Ptr;

  typedef std::function<void(Client::Ptr newClient)> NewClientHandler;

  struct Configuration
  {
    Configuration() : defaultAlgorithm_(ALGORITHM_CRYPTONIGHT) {}
    Configuration(const net::EndPoint& endPoint, Algorithm defaultAlgorithm)
      : endPoint_(endPoint), defaultAlgorithm_(defaultAlgorithm)
    {
    }

    net::EndPoint endPoint_;
    Algorithm defaultAlgorithm_;
  };

public:
  Server(const std::shared_ptr<boost::asio::io_service>& ioService);
  void start(const Configuration& configuration, const NewClientHandler& newClientHandler);

public:
  void handleNewConnection(net::Connection::Ptr connection);

private:
  std::shared_ptr<boost::asio::io_service> ioService_;
  Configuration configuration_;
  NewClientHandler newClientHandler_;
  net::server::Server::Ptr server_;
};

} // namespace proxy
} // namespace ses
