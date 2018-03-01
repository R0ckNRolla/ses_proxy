#ifndef SES_PROXY_JOBRESULT_HPP
#define SES_PROXY_JOBRESULT_HPP

#include <string>
#include <array>

#include "proxy/workeridentifier.hpp"

namespace ses {
namespace proxy {


class JobResult
{
public:
  enum
  {
    HASH_SIZE_BYTES = 32
  };
  typedef std::array<uint8_t, HASH_SIZE_BYTES> Hash;

  enum SubmitStatus
  {
    SUBMIT_ACCEPTED,
    SUBMIT_REJECTED_IP_BANNED,
    SUBMIT_REJECTED_UNAUTHENTICATED,
    SUBMIT_REJECTED_DUPLICATE,
    SUBMIT_REJECTED_EXPIRED,
    SUBMIT_REJECTED_INVALID_JOB_ID,
    SUBMIT_REJECTED_LOW_DIFFICULTY_SHARE
  };
  typedef std::function<void(SubmitStatus submitStatus)> SubmitStatusHandler;
  typedef std::function<void(const WorkerIdentifier& workerIdentifier,
                             const JobResult& jobResult,
                             const SubmitStatusHandler& submitStatusHandler)> Handler;

public:
  JobResult(const std::string& jobId, const std::string& nonce, const std::string& hash);

  JobResult(const std::string& btId, const std::string& nonce, const std::string& resultHash,
            const std::string& workerNonce);

  bool isNodeJsResult() const;

  std::string getNonceHexString() const;
  std::string getHashHexString() const;
  std::string getWorkerNonceHexString() const;

  const std::string& getJobId() const;
  void setJobId(const std::string& jobId);
  uint32_t getNonce() const;
  const Hash& getHash() const;
  uint32_t getWorkerNonce() const;

  uint8_t getNiceHash() const;

private:
  std::string jobId_;
  uint32_t nonce_;
  Hash hash_;
  uint32_t workerNonce_;
  bool isNodeJsResult_;
};

} // namespace proxy
} // namespace ses

#endif //SES_PROXY_JOBRESULT_HPP