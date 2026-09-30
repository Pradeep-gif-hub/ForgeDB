#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <array>
#include <cstdlib>
#include <iostream>
#include <string>

int main(int argc, char* argv[]) {
  std::string host = "127.0.0.1";
  uint16_t port = 7654;

  for (int i = 1; i < argc; ++i) {
    std::string arg = argv[i];
    if (arg == "--host" && i + 1 < argc) {
      host = argv[++i];
    } else if (arg == "--port" && i + 1 < argc) {
      port = static_cast<uint16_t>(std::stoi(argv[++i]));
    }
  }

  int sock_fd = socket(AF_INET, SOCK_STREAM, 0);
  if (sock_fd < 0) {
    std::cerr << "Error creating socket\n";
    return EXIT_FAILURE;
  }

  sockaddr_in server_addr{};
  server_addr.sin_family = AF_INET;
  server_addr.sin_port = htons(port);
  if (inet_pton(AF_INET, host.c_str(), &server_addr.sin_addr) <= 0) {
    std::cerr << "Invalid server address: " << host << "\n";
    close(sock_fd);
    return EXIT_FAILURE;
  }

  if (connect(sock_fd, reinterpret_cast<struct sockaddr*>(&server_addr), sizeof(server_addr)) < 0) {
    std::cerr << "Failed to connect to ForgeDB at " << host << ":" << port << "\n";
    close(sock_fd);
    return EXIT_FAILURE;
  }

  std::cout << "Connected to ForgeDB at " << host << ":" << port << "\n";
  std::cout << "Type commands (e.g. SET k v, GET k, SCAN a z, STATS, QUIT)\n";

  std::string line;
  std::array<char, 4096> buffer{};

  while (true) {
    std::cout << "forgedb> " << std::flush;
    if (!std::getline(std::cin, line)) {
      break;
    }

    if (line.empty()) {
      continue;
    }

    line += "\n";
    ssize_t written = write(sock_fd, line.data(), line.size());
    if (written < 0) {
      std::cerr << "Connection closed by server.\n";
      break;
    }

    ssize_t bytes_read = read(sock_fd, buffer.data(), buffer.size() - 1);
    if (bytes_read <= 0) {
      std::cerr << "Connection closed by server.\n";
      break;
    }

    buffer[static_cast<size_t>(bytes_read)] = '\0';
    std::cout << buffer.data();

    if (line == "QUIT\n" || line == "quit\n") {
      break;
    }
  }

  close(sock_fd);
  return EXIT_SUCCESS;
}
