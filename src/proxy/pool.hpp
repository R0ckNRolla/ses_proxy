//
// Created by ses on 18.02.18.
//

#ifndef SES_PROXY_POOL_HPP
#define SES_PROXY_POOL_HPP

#include <memory>
#include <unordered_map>
#include <list>
#include <mutex>

#include "net/connection.hpp"
#include "stratum/stratum.hpp"
#include "proxy/algorithm.hpp"
#include "proxy/job.hpp"
#include "proxy/worker.hpp"

namespace ses {
namespace proxy {

class Pool : public std::enable_shared_from_this<Pool>
{
public:
  typedef std::shared_ptr<Pool> Ptr;

  struct Configuration
  {
    Configuration() : algorithm_(ALGORITHM_CRYPTONIGHT) {}
    Configuration(const net::EndPoint& endPoint, const std::string& user, const std::string& pass,
                  Algorithm algorithm)
      : endPoint_(endPoint), user_(user), pass_(pass), algorithm_(algorithm)
    {
    }

    net::EndPoint endPoint_;
    std::string user_;
    std::string pass_;
    Algorithm algorithm_;
  };

public:
  void connect(const Configuration& configuration);

  bool addWorker(const Worker::Ptr& worker);

private:
  void handleReceived(char* data, std::size_t size);
  void handleError(const std::string& error);

  void handleLoginSuccess(const std::string& id, const std::optional<stratum::Job>& job);
  void handleLoginError(int code, const std::string& message);

  void handleGetJobSuccess(const stratum::Job& job);
  void handleGetJobError(int code, const std::string& message);

  void handleSubmitSuccess(const std::string& jobId, const Job::SubmitStatusHandler& submitStatusHandler,
                           const std::string& status);
  void handleSubmitError(const std::string& jobId, const Job::SubmitStatusHandler& submitStatusHandler,
                         int code, const std::string& message);

  void handleNewJob(const stratum::Job& job);

  Job::SubmitStatus handleJobResult(const WorkerIdentifier& workerIdentifier,
                                    const JobResult& jobResult,
                                    const Job::SubmitStatusHandler& submitStatusHandler);

private:

  typedef uint32_t RequestIdentifier;
  enum RequestType
  {
    REQUEST_TYPE_LOGIN,
    REQUEST_TYPE_GETJOB,
    REQUEST_TYPE_SUBMIT
  };
  RequestIdentifier sendRequest(RequestType type, const std::string& params = "");
  void setJob(const stratum::Job& job);
  void activateJob(const MasterJob::Ptr& job);
  void removeJob(const std::string& jobId);
  bool assignJobToWorker(const Worker::Ptr& worker);
  void login();
  void submit(const JobResult& jobResult, const Job::SubmitStatusHandler& submitStatusHandler);

private:
  std::recursive_mutex mutex_;

  Configuration configuration_;

  net::Connection::Ptr connection_;
  RequestIdentifier nextRequestIdentifier_ = 1;
  std::unordered_map<RequestIdentifier, RequestType> outstandingRequests_;
  std::unordered_map<RequestIdentifier, std::tuple<std::string, Job::SubmitStatusHandler> > outstandingSubmits_;

  std::string workerIdentifier_;
  MasterJob::Ptr activeJob_;
  std::map<std::string, MasterJob::Ptr> jobs_;

  std::list<Worker::Ptr> worker_;
};

} // namespace proxy
} // namespace ses

#endif //SES_PROXY_POOL_HPP
