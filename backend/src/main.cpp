#include "estimated_taxes/api.hpp"
#include "estimated_taxes/current_date_provider.hpp"
#include "estimated_taxes/http_server.hpp"

#include <iostream>
#include <stdexcept>

int main(int argc, const char* const argv[])
{
  try {
    const auto configuration = estimated_taxes::http::listener_configuration(argc, argv);
    estimated_taxes::LocalCurrentDateProvider clock;
    estimated_taxes::ApiApplication application(configuration.database_path, clock);
    if (application.handle({"GET", "/api/2026", {}, {}}).status != 200) {
      throw std::runtime_error("initial calculation failed");
    }
    estimated_taxes::http::HttpServer server(configuration);
    std::cout << "Estimated Taxes backend listening on " << configuration.bind_address << ':' << server.port() << ".\n";
    server.run(application);
    return 0;
  } catch (const std::exception& error) {
    std::cerr << "Backend startup failed: " << error.what() << '\n';
    return 2;
  }
}
