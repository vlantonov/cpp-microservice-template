#pragma once

#include <memory>
#include <string>

namespace app::core {
class GreeterService;
}
namespace app::observability {
class Metrics;
}

namespace app::transport::grpc {

class GreeterGrpcServer {
 public:
  GreeterGrpcServer(std::shared_ptr<app::core::GreeterService> service,
                    std::shared_ptr<app::observability::Metrics> metrics,
                    std::string listen_address);

  void RunBlocking();

 private:
  std::shared_ptr<app::core::GreeterService> service_;
  std::shared_ptr<app::observability::Metrics> metrics_;
  std::string listen_address_;
};

}  // namespace app::transport::grpc
