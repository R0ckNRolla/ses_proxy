
#include <iostream>
#include <sstream>

#include "net/jsonrpc/jsonrpc.hpp"
#include "stratum/stratum.hpp"
#include "proxy/client.hpp"
#include "util/difficulty.hpp"
#include "util/log.hpp"

#undef LOG_COMPONENT
#define LOG_COMPONENT client
#define LOG_CLIENT_INFO LOG_INFO << clientName_ << ": "

namespace ses {
namespace proxy {

Client::Client(const std::shared_ptr<boost::asio::io_service>& ioService,
               const WorkerIdentifier& id, Algorithm defaultAlgorithm, uint32_t defaultDifficulty,
               uint32_t targetSecondsBetweenSubmits)
  : ioService_(ioService), identifier_(id), algorithm_(defaultAlgorithm), type_(WorkerType::UNKNOWN),
    difficulty_(defaultDifficulty), targetSecondsBetweenSubmits_(targetSecondsBetweenSubmits)
{
}

void Client::setDisconnectHandler(const DisconnectHandler& disconnectHandler)
{
  disconnectHandler_ = disconnectHandler;
}

void Client::setConnection(const net::Connection::Ptr& connection)
{
  std::lock_guard<std::recursive_mutex> lock(mutex_);

  auto oldConnection = connection_.lock();
  if (oldConnection && oldConnection != connection)
  {
    oldConnection->disconnect();
  }
  connection_ = connection;
  if (connection)
  {
    connection->setSelfSustainingUntilDisconnect(true);
    connection->setHandler(std::bind(&Client::handleReceived, this, std::placeholders::_1),
                           std::bind(&Client::handleDisconnect, this, std::placeholders::_1));
  }
}

WorkerIdentifier Client::getIdentifier() const
{
  return identifier_;
}

Algorithm Client::getAlgorithm() const
{
  return algorithm_;
}

WorkerType Client::getType() const
{
  return type_;
}

void Client::assignJob(const Job::Ptr& job)
{
  std::lock_guard<std::recursive_mutex> lock(mutex_);
  LOG_DEBUG << __PRETTY_FUNCTION__;
  if (job)
  {
//    job->setAssignedWorker(identifier_);
    jobs_[job->getJobIdentifier()].first = job;
    currentJob_ = job;
    //TODO dynamic difficulty adjustment
    //difficulty_ = job->getDifficulty();//std::min(difficulty_, job->getDifficulty());
    sendJobNotification();
  }
}

bool Client::isConnected() const
{
  auto connection = connection_.lock();
  return connection && connection->isConnected();
}

const util::HashRateCalculator& Client::getHashRate() const
{
  return hashrate_;
}

const std::string& Client::getUseragent() const
{
  return useragent_;
}

const std::string& Client::getUsername() const
{
  return username_;
}

const std::string& Client::getPassword() const
{
  return password_;
}

void Client::handleReceived(const std::string& data)
{
  using namespace std::placeholders;

  std::lock_guard<std::recursive_mutex> lock(mutex_);

  net::jsonrpc::parse(
    data,
    [this](const std::string& id, const std::string& method, const std::string& params)
    {
//      LOG_DEBUG << "proxy::Client::handleReceived request, id, " << id << ", method, " << method
//                << ", params, " << params;
      stratum::server::parseRequest(id, method, params,
                                    std::bind(&Client::handleLogin, this, _1, _2, _3, _4),
                                    std::bind(&Client::handleGetJob, this, _1),
                                    std::bind(&Client::handleSubmit, this, _1, _2, _3, _4, _5, _6, _7),
                                    std::bind(&Client::handleKeepAliveD, this, _1, _2),
                                    std::bind(&Client::handleUnknownMethod, this, _1));
    },
    [this](const std::string& id, const std::string& result, const std::string& error)
    {
      LOG_DEBUG << "proxy::Client::handleReceived response, id, " << id << ", result, " << result
                << ", error, " << error;
    },
    [this](const std::string& method, const std::string& params)
    {
      LOG_DEBUG << "proxy::Client::handleReceived notification, method, " << method << ", params, "
                << params;
    });
}

void Client::handleDisconnect(const std::string& error)
{
  LOG_CLIENT_INFO << "Disconnected";
  if (disconnectHandler_)
  {
    disconnectHandler_();
  }
}

void Client::handleLogin(const std::string& jsonRequestId, const std::string& login, const std::string& pass, const std::string& agent)
{
  LOG_DEBUG << "ses::proxy::Client::handleLogin()"
            << ", login, " << login
            << ", pass, " << pass
            << ", agent, " << agent;

  if (login.empty())
  {
    sendErrorResponse(jsonRequestId, "missing login");
    useragent_.clear();
    username_.clear();
    password_.clear();
  }
  else
  {
    // TODO 'invalid address used for login'

    useragent_ = agent;
    username_ = login;
    password_ = pass;

    if (useragent_.find("xmr-node-proxy") != std::string::npos)
    {
      type_ = WorkerType::PROXY;
    }
    else
    {
      type_ = WorkerType::MINER;
    }
    //TODO agent can contain "NiceHash", which asks for a certain difficulty (check nodejs code)

    //TODO login parsing: <wallet>[.<payment id>][+<difficulty>]
    //TODO password parsing: <identifier>[:<email>]

    updateName();

    LOG_CLIENT_INFO << "Logged in as " << username_ << " with " << useragent_;

    std::string responseResult =
      stratum::server::createLoginResponse(boost::uuids::to_string(identifier_),
                                           currentJob_ ? buildStratumJob() : boost::optional<stratum::Job>());
    sendResponse(jsonRequestId, responseResult);
  }
}

void Client::handleGetJob(const std::string& jsonRequestId)
{
  LOG_DEBUG << __PRETTY_FUNCTION__;

  if (currentJob_)
  {
    sendResponse(jsonRequestId, stratum::server::createJobNotification(buildStratumJob()));
  }
  else
  {
    sendErrorResponse(jsonRequestId, "No job available");
  }
}

void Client::handleSubmit(const std::string& jsonRequestId,
                          const std::string& identifier, const std::string& jobIdentifier,
                          const std::string& nonce, const std::string& result,
                          const std::string& workerNonce, const std::string& poolNonce)
{
  LOG_DEBUG << "ses::proxy::Client::handleSubmit()"
            << ", identifier, " << identifier
            << ", jobIdentifier, " << jobIdentifier
            << ", nonce, " << nonce
            << ", result, " << result
            << ", workerNonce, " << workerNonce
            << ", poolNonce, " << poolNonce;

  if (identifier != toString(identifier_))
  {
    sendErrorResponse(jsonRequestId, "Unauthenticated");
  }
  else
  {
    auto jobIt = jobs_.find(jobIdentifier);
    if (jobIt != jobs_.end())
    {
      JobResult jobResult(jobIdentifier, nonce, result);
      uint32_t resultDifficulty = jobResult.getDifficulty();
      LOG_DEBUG << " difficulty = " << jobResult.getDifficulty();

      Job::Ptr& job = jobIt->second.first;
      uint32_t anouncedDifficulty = jobIt->second.second;
      uint32_t jobDifficulty = job->getDifficulty();

      if (resultDifficulty < anouncedDifficulty)
      {
        sendErrorResponse(jsonRequestId, "Low difficulty share");
      }
      else
      {
        sendSuccessResponse(jsonRequestId, "OK");
        updateHashrates(anouncedDifficulty);

        if (resultDifficulty >= jobDifficulty)
        {
          jobIt->second.first->submitResult(jobResult,
                                            std::bind(&Client::handleUpstreamSubmitStatus, shared_from_this(),
                                                      jsonRequestId, std::placeholders::_1));
        }
        else
        {
          // ignores the submit
        }
      }
    }
    else
    {
      sendErrorResponse(jsonRequestId, "Invalid job id");
    }
  }
}

void Client::handleKeepAliveD(const std::string& jsonRequestId, const std::string& identifier)
{
  LOG_DEBUG << "ses::proxy::Client::handleKeepAliveD(), identifier, " << identifier;
  if (identifier != toString(identifier_))
  {
    sendErrorResponse(jsonRequestId, "Unauthenticated");
  }
  else
  {
    sendSuccessResponse(jsonRequestId, "KEEPALIVED");
  }
}

void Client::handleUnknownMethod(const std::string& jsonRequestId)
{
  sendErrorResponse(jsonRequestId, "invalid method");
}

void Client::handleUpstreamSubmitStatus(std::string jsonRequestId, JobResult::SubmitStatus submitStatus)
{
  std::lock_guard<std::recursive_mutex> lock(mutex_);

  // Towards the client, the submit has already been reported as accepted ...
  // TODO handling of malicious clients

  switch (submitStatus)
  {
    case JobResult::SUBMIT_REJECTED_IP_BANNED:
//      sendErrorResponse(jsonRequestId, "IP Address currently banned");
      break;

    case JobResult::SUBMIT_REJECTED_UNAUTHENTICATED:
//      sendErrorResponse(jsonRequestId, "Unauthenticated");
      break;

    case JobResult::SUBMIT_REJECTED_DUPLICATE:
//      sendErrorResponse(jsonRequestId, "Duplicate share");
      break;

    case JobResult::SUBMIT_REJECTED_EXPIRED:
//      sendErrorResponse(jsonRequestId, "Block expired");
      break;

    case JobResult::SUBMIT_REJECTED_INVALID_JOB_ID:
//      sendErrorResponse(jsonRequestId, "Invalid job id");
      break;

    case JobResult::SUBMIT_REJECTED_LOW_DIFFICULTY_SHARE:
//      sendErrorResponse(jsonRequestId, "Low difficulty share");
      break;

    case JobResult::SUBMIT_ACCEPTED:
    default:
    {
      break;
    }
  }
}

void Client::updateName()
{
  std::ostringstream clientNameStream;
  clientNameStream << "<" << identifier_;
  if (auto connection = connection_.lock())
  {
    clientNameStream << "@" << connection->getConnectedIp() << ":" << connection->getConnectedPort() << ">";
  }
  clientName_ = clientNameStream.str();
}

void Client::updateHashrates(uint32_t difficulty)
{
  ++submits_;
  hashrate_.addHashes(difficulty);

  if (hashrate_.getAverageHashRateLongTimeWindow() != 0 &&
      hashrate_.secondsSinceStart() > std::chrono::seconds(10))
  {
    difficulty_ = hashrate_.getAverageHashRateLongTimeWindow() * targetSecondsBetweenSubmits_;
  }

  LOG_CLIENT_INFO << "Submit success "
                  << ", submits, " << submits_ << ", hashes, " << hashrate_.getTotalHashes()
                  << ", hashrate, " << hashrate_.getHashRateLastUpdate()
                  << ", hashrate1Min, " << hashrate_.getAverageHashRateShortTimeWindow()
                  << ", hashrate10Min, " << hashrate_.getAverageHashRateLongTimeWindow()
                  << ", newClientDifficulty, " << difficulty_;

//  if (timeLastJobTransmit > std::chrono::seconds(30))
//  {
//    // TODO update job if necessary, especially if difficulty has to change
//  }
}

void Client::sendResponse(const std::string& jsonRequestId, const std::string& response)
{
  if (auto connection = connection_.lock())
  {
    connection->send(net::jsonrpc::response(jsonRequestId, response, ""));
  }
}

void Client::sendSuccessResponse(const std::string& jsonRequestId, const std::string& status)
{
  if (auto connection = connection_.lock())
  {
    connection->send(net::jsonrpc::statusResponse(jsonRequestId, status));
  }
}

void Client::sendErrorResponse(const std::string& jsonRequestId, const std::string& message)
{
  if (auto connection = connection_.lock())
  {
    connection->send(net::jsonrpc::errorResponse(jsonRequestId, -1, message));
  }
}

void Client::sendJobNotification()
{
  auto connection = connection_.lock();
  if (currentJob_ && connection)
  {
    connection->send(
      net::jsonrpc::notification("job", stratum::server::createJobNotification(buildStratumJob())));
  }
}

stratum::Job Client::buildStratumJob()
{
  uint32_t modifiedDifficulty = std::min(difficulty_, currentJob_->getDifficulty());
  std::string modifiedTarget = util::difficultyToTarget(modifiedDifficulty).toHexString();
  jobs_[currentJob_->getJobIdentifier()].second = modifiedDifficulty;
  stratum::Job stratumJob = currentJob_->asStratumJob();
  stratumJob.setTarget(modifiedTarget);
  LOG_CLIENT_INFO << "Sending job to client"
                  << ", id, " << currentJob_->getJobIdentifier()
                  << ", clientDifficulty, " << modifiedDifficulty
                  << ", jobDifficulty, " << currentJob_->getDifficulty()
                  << ", target, " << modifiedTarget;
//  lastJobTransmitTimePoint_ = std::chrono::system_clock::now();
  return stratumJob;
}

} // namespace proxy
} // namespace ses