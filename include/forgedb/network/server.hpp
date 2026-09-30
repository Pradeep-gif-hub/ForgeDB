#pragma once

#include <atomic>
#include <cstdint>
#include <memory>
#include <string>
#include <thread>

#include "forgedb/concurrency/thread_pool.hpp"
#include "forgedb/engine/storage_engine.hpp"

namespace forgedb {

class Server {
 public:
  Server(StorageEngine* engine, uint16_t port, size_t num_worker_threads = 4);
  ~Server();

  Server(const Server&) = delete;
  Server& operator=(const Server&) = delete;

  void start();
  void stop();

  [[nodiscard]] bool is_running() const noexcept { return is_running_.load(); }
  [[nodiscard]] uint16_t get_port() const noexcept { return port_; }

  std::string execute_command(std::string_view request_line);

 private:
  void accept_loop();
  void handle_client(int client_fd);

  StorageEngine* engine_;
  uint16_t port_;
  std::atomic<bool> is_running_{false};
  int server_fd_{-1};

  std::unique_ptr<ThreadPool> thread_pool_;
  std::thread accept_thread_;
  mutable std::mutex clients_latch_;
  std::unordered_set<int> client_fds_;
};

}  // namespace forgedb
