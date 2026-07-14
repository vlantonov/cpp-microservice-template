#pragma once

#include <memory>
#include <string>

namespace app::core {
class GreeterService;
}
namespace app::observability {
class Metrics;
}

namespace app::transport::rest {

class RestServer {
 public:
  RestServer(std::shared_ptr<app::core::GreeterService> service,
             std::shared_ptr<app::observability::Metrics> metrics,
             std::string host, int port);

  void RunBlocking();

 private:
  std::shared_ptr<app::core::GreeterService> service_;
  std::shared_ptr<app::observability::Metrics> metrics_;
  std::string host_;
  int port_;
};

}  // namespace app::transport::rest
