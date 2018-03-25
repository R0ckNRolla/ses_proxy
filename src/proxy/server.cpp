#include <boost/uuid/random_generator.hpp>

#include "proxy/server.hpp"
#include "util/log.hpp"

#undef LOG_COMPONENT
#define LOG_COMPONENT server

namespace ses {
namespace proxy {

Server::Server(const std::shared_ptr<boost::asio::io_service>& ioService)
  : ioService_(ioService)
{
}

void Server::start(const Configuration& configuration, const NewClientHandler& newClientHandler)
{
  LOG_INFO << "Starting server - " << configuration;
  configuration_ = configuration;
  newClientHandler_ = newClientHandler;
  server_ = net::server::createServer(ioService_,
                                      std::bind(&Server::handleNewConnection, this, std::placeholders::_1),
                                      configuration.endPoint_);
}

void Server::handleNewConnection(net::Connection::Ptr connection)
{
  auto workerId = boost::uuids::random_generator()();
  auto client = std::make_shared<Client>(ioService_, workerId, configuration_.defaultAlgorithm_,
                                         configuration_.defaultDifficulty_, configuration_.targetSecondsBetweenSubmits_);
  if (newClientHandler_)
  {
    newClientHandler_(client);
  }
  // assigns the connection after calling newClientHandler, so that a job can be assigned before the client's submit is processed
  client->setConnection(connection);
}

} // namespace proxy
} // namespace ses