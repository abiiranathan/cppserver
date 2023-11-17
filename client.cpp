#include "client.hpp"
#include <cstring>

cppserver::Client::Client(int client_fd, int epoll_fd)
    : client_fd(client_fd), epoll_fd(epoll_fd) {}

cppserver::Client::~Client() {
  shutdown(client_fd, SHUT_WR);
  close(client_fd);
}

// Reads data from a client socket
// Returns the total bytes read or -1 on failure
int cppserver::Client::Read(std::string &buffer) {
  int success = 1;

  while (true) {
    char inner_buf[BUFSIZ];
    ssize_t bytes_read = read(client_fd, inner_buf, sizeof(inner_buf));

    if (bytes_read <= 0) {
      if (bytes_read == 0) {
        // End of file. The remote has closed the connection.
        success = 1;
      } else if (bytes_read < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
          // No more data to read for now, try again later
        } else {
          perror("read");
          success = 0;
        }
      }
      break;
    } else {
      buffer.append(inner_buf, static_cast<size_t>(bytes_read));
    }
  }
  return success ? buffer.size() : -1;
}

void cppserver::Client::SendHttpError(HttpStatus status,
                                      const std::string &message) {
  std::string reply;

  reply +=
      "HTTP/1.1 " + std::to_string(status) + " " + StatusText(status) + "\r\n";
  reply += "Content-Type: text/html\r\n";
  reply += "Content-Length: " + std::to_string(message.size()) + "\r\n";
  reply += "\r\n" + message + "\r\n";

  send(client_fd, reply.c_str(), reply.size(), 0);
  std::cout << "[ERROR]: " << message << std::endl;
}

size_t cppserver::Client::Send(const std::string &data) {
  return send(client_fd, data.c_str(), data.size(), 0);
}

int cppserver::Client::fd() { return client_fd; }