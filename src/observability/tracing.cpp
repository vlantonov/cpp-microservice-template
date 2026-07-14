#include "observability/tracing.hpp"

#include <opentelemetry/exporters/otlp/otlp_http_exporter_factory.h>
#include <opentelemetry/sdk/resource/resource.h>
#include <opentelemetry/sdk/resource/semantic_conventions.h>
#include <opentelemetry/sdk/trace/batch_span_processor_factory.h>
#include <opentelemetry/sdk/trace/tracer_provider.h>
#include <opentelemetry/trace/provider.h>

namespace nostd = opentelemetry::nostd;
namespace otlp = opentelemetry::exporter::otlp;
namespace resource = opentelemetry::sdk::resource;
namespace sdktrace = opentelemetry::sdk::trace;

namespace app::observability {

void Tracing::Init(const std::string &service_name,
                   const std::string &otlp_http_endpoint) {
  otlp::OtlpHttpExporterOptions exporter_opts;
  exporter_opts.url = otlp_http_endpoint;

  auto exporter = otlp::OtlpHttpExporterFactory::Create(exporter_opts);
  auto processor = sdktrace::BatchSpanProcessorFactory::Create(std::move(exporter));

  resource::ResourceAttributes attributes = {
      {resource::SemanticConventions::kServiceName, service_name}};

  auto provider = nostd::shared_ptr<opentelemetry::trace::TracerProvider>(
      new sdktrace::TracerProvider(std::move(processor),
                                   resource::Resource::Create(attributes)));

  opentelemetry::trace::Provider::SetTracerProvider(provider);
}

opentelemetry::nostd::shared_ptr<opentelemetry::trace::Tracer> Tracing::GetTracer(
    const std::string &library_name) {
  auto provider = opentelemetry::trace::Provider::GetTracerProvider();
  return provider->GetTracer(library_name);
}

}  // namespace app::observability
