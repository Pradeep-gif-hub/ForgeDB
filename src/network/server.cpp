#include "forgedb/network/server.hpp"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <array>
#include <cstring>
#include <iostream>
#include <sstream>
#include <stdexcept>

#include "forgedb/network/parser.hpp"

namespace forgedb {

Server::Server(StorageEngine* engine, uint16_t port, size_t num_worker_threads)
    : engine_(engine),
      port_(port),
      thread_pool_(std::make_unique<ThreadPool>(num_worker_threads)) {
  if (engine_ == nullptr) {
    throw std::invalid_argument("StorageEngine cannot be null in Server");
  }
}

Server::~Server() {
  stop();
}

void Server::start() {
  if (is_running_.exchange(true)) {
    return;
  }

  server_fd_ = socket(AF_INET, SOCK_STREAM, 0);
  if (server_fd_ < 0) {
    is_running_ = false;
    throw std::runtime_error("Failed to create socket");
  }

  int opt = 1;
  setsockopt(server_fd_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

  sockaddr_in address{};
  address.sin_family = AF_INET;
  address.sin_addr.s_addr = INADDR_ANY;
  address.sin_port = htons(port_);

  if (bind(server_fd_, reinterpret_cast<struct sockaddr*>(&address), sizeof(address)) < 0) {
    close(server_fd_);
    server_fd_ = -1;
    is_running_ = false;
    throw std::runtime_error("Failed to bind server socket to port " + std::to_string(port_));
  }

  if (listen(server_fd_, 128) < 0) {
    close(server_fd_);
    server_fd_ = -1;
    is_running_ = false;
    throw std::runtime_error("Failed to listen on server socket");
  }

  accept_thread_ = std::thread(&Server::accept_loop, this);
}

#include <poll.h>

void Server::stop() {
  if (!is_running_.exchange(false)) {
    return;
  }

  {
    std::scoped_lock lock(clients_latch_);
    for (int fd : client_fds_) {
      shutdown(fd, SHUT_RDWR);
      close(fd);
    }
    client_fds_.clear();
  }

  if (server_fd_ >= 0) {
    shutdown(server_fd_, SHUT_RDWR);
    close(server_fd_);
    server_fd_ = -1;
  }

  if (accept_thread_.joinable()) {
    accept_thread_.join();
  }

  if (thread_pool_) {
    thread_pool_->stop();
  }
}

void Server::accept_loop() {
  while (is_running_) {
    struct pollfd pfd{};
    pfd.fd = server_fd_;
    pfd.events = POLLIN;

    int poll_res = poll(&pfd, 1, 100);
    if (poll_res <= 0) {
      continue;
    }

    sockaddr_in client_addr{};
    socklen_t client_len = sizeof(client_addr);
    int client_fd = accept(server_fd_, reinterpret_cast<struct sockaddr*>(&client_addr), &client_len);

    if (client_fd < 0) {
      continue;
    }

    std::cout << "[Server] Accepted new connection (fd: " << client_fd << ")\n";

    {
      std::scoped_lock lock(clients_latch_);
      client_fds_.insert(client_fd);
    }

    thread_pool_->submit([this, client_fd] {
      handle_client(client_fd);
    });
  }
}

void Server::handle_client(int client_fd) {
  std::array<char, 4096> buffer{};
  std::string accumulated;

  while (is_running_) {
    struct pollfd pfd{};
    pfd.fd = client_fd;
    pfd.events = POLLIN;

    int poll_res = poll(&pfd, 1, 100);
    if (poll_res == 0) {
      continue;
    }
    if (poll_res < 0) {
      break;
    }

    ssize_t bytes_read = read(client_fd, buffer.data(), buffer.size());
    if (bytes_read <= 0) {
      break;
    }

    accumulated.append(buffer.data(), static_cast<size_t>(bytes_read));

    size_t pos = 0;
    while ((pos = accumulated.find('\n')) != std::string::npos) {
      std::string line = accumulated.substr(0, pos);
      accumulated.erase(0, pos + 1);

      if (!line.empty() && line.back() == '\r') {
        line.pop_back();
      }

      if (line.empty()) {
        continue;
      }

      std::cout << "[Server] Received command from fd " << client_fd << ": \"" << line << "\"\n";

      std::string response = execute_command(line);
      ssize_t written = write(client_fd, response.data(), response.size());
      if (written < 0) {
        break;
      }
    }
  }

  std::cout << "[Server] Client disconnected (fd: " << client_fd << ")\n";

  {
    std::scoped_lock lock(clients_latch_);
    client_fds_.erase(client_fd);
  }
  close(client_fd);
}

std::string Server::execute_command(std::string_view request_line) {
  Command cmd = Parser::parse(request_line);

  switch (cmd.type) {
    case CommandType::kPing:
      return Parser::format_simple_string("PONG");

    case CommandType::kGet: {
      if (cmd.args.empty()) {
        return Parser::format_error("wrong number of arguments for 'GET' command");
      }
      auto val = engine_->get(cmd.args[0]);
      if (val.has_value()) {
        return Parser::format_bulk_string(*val);
      }
      return Parser::format_null();
    }

    case CommandType::kSet: {
      if (cmd.args.size() < 2) {
        return Parser::format_error("wrong number of arguments for 'SET' command");
      }
      bool ok = engine_->put(cmd.args[0], cmd.args[1]);
      if (ok) {
        return Parser::format_simple_string("OK");
      }
      return Parser::format_error("failed to execute SET");
    }

    case CommandType::kDel: {
      if (cmd.args.empty()) {
        return Parser::format_error("wrong number of arguments for 'DEL' command");
      }
      bool deleted = engine_->delete_key(cmd.args[0]);
      return Parser::format_integer(deleted ? 1 : 0);
    }

    case CommandType::kScan: {
      if (cmd.args.size() < 2) {
        return Parser::format_error("wrong number of arguments for 'SCAN' command");
      }
      auto results = engine_->scan(cmd.args[0], cmd.args[1]);
      std::vector<std::string> formatted;
      formatted.reserve(results.size() * 2);
      for (const auto& [k, v] : results) {
        formatted.push_back(k);
        formatted.push_back(v);
      }
      return Parser::format_array(formatted);
    }

    case CommandType::kStats:
      return Parser::format_bulk_string(engine_->get_stats().to_json());

    case CommandType::kCheckpoint:
      engine_->checkpoint();
      return Parser::format_simple_string("OK");

    case CommandType::kQuit:
      return Parser::format_simple_string("OK");

    case CommandType::kUnknown:
    default:
      return Parser::format_error("unknown command");
  }
}

}  // namespace forgedb
