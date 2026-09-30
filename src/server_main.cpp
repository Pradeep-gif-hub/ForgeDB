#include <csignal>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <string>

#include "forgedb/engine/storage_engine.hpp"
#include "forgedb/network/server.hpp"

namespace {
std::atomic<bool> g_shutdown_requested{false};

void signal_handler(int /*signal*/) {
  g_shutdown_requested.store(true);
}
}  // namespace

int main(int argc, char* argv[]) {
  uint16_t port = 7654;
  std::filesystem::path db_path = "forgedb_data/forgedb.db";
  size_t pool_size = 64;

  for (int i = 1; i < argc; ++i) {
    std::string arg = argv[i];
    if (arg == "--port" && i + 1 < argc) {
      port = static_cast<uint16_t>(std::stoi(argv[++i]));
    } else if (arg == "--db" && i + 1 < argc) {
      db_path = argv[++i];
    } else if (arg == "--pool" && i + 1 < argc) {
      pool_size = static_cast<size_t>(std::stoul(argv[++i]));
    }
  }

  std::signal(SIGINT, signal_handler);
  std::signal(SIGTERM, signal_handler);

  std::cout << "Starting ForgeDB Server on port " << port << " [DB: " << db_path.string() << "]\n";

  try {
    forgedb::StorageEngine engine(db_path, pool_size);
    forgedb::Server server(&engine, port);

    server.start();
    std::cout << "ForgeDB Server is listening for connections...\n";

    while (!g_shutdown_requested.load()) {
      std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    std::cout << "Shutting down ForgeDB Server gracefully...\n";
    server.stop();
    engine.flush();
    std::cout << "ForgeDB Server stopped.\n";
  } catch (const std::exception& ex) {
    std::cerr << "Fatal server error: " << ex.what() << "\n";
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}
