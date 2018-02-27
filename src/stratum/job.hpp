#ifndef SES_STRATUM_JOB_HPP
#define SES_STRATUM_JOB_HPP

#include <string>
#include <vector>
#include <memory>

namespace ses {
namespace stratum {

class Job
{
public:
  Job(const std::string& id, const std::string& jobId, const std::string& blobHexString,
      const std::string& targetHexString);

  Job(const std::string& id, const std::string& jobId, const std::string& blobHexString,
      const std::string& targetHexString,
      const std::string& blocktemplate_blob, const std::string& difficulty, const std::string& height,
      const std::string& reserved_offset, const std::string& client_nonce_offset,
      const std::string& client_pool_offset, const std::string& target_diff, const std::string& target_diff_hex,
      const std::string& job_id);

  const std::string& getId() const;
  const std::string& getJobId() const;
  const std::string& getBlob() const;
  const std::string& getTarget() const;

  const std::string& getBlocktemplate_blob() const;
  const std::string& getDifficulty() const;
  const std::string& getHeight() const;
  const std::string& getReserved_offset() const;
  const std::string& getClient_nonce_offset() const;
  const std::string& getClient_pool_offset() const;
  const std::string& getTarget_diff() const;
  const std::string& getTarget_diff_hex() const;
  const std::string& getJob_id() const;

  bool isNodeJs() const;

private:
  std::string blob_;
  std::string jobId_;
  std::string target_;
  std::string id_;

  //nodejs specific fields
  std::string blocktemplate_blob_;
  std::string difficulty_;
  std::string height_;
  std::string reserved_offset_;
  std::string client_nonce_offset_;
  std::string client_pool_offset_;
  std::string target_diff_;
  std::string target_diff_hex_;
  std::string job_id_;
};

} // namespace stratum
} // namespace ses

#endif //SES_STRATUM_JOB_HPP
