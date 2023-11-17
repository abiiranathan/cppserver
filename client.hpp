#ifndef CLIENT_H
#define CLIENT_H

#include "status.hpp"
#include <iostream>
#include <string>
#include <sys/socket.h>
#include <unistd.h>
#include <vector>

namespace cppserver {
class Client {
private:
  int client_fd;
  int epoll_fd;

public:
  explicit Client(int client_fd, int epoll_fd);
  ~Client();

  // Reads data from a client socket
  // Returns the total bytes read or -1 on failure
  int Read(std::string &buffer);
  size_t Send(const std::string &data);

  // Returns the client file descriptor.
  int fd();
  void SendHttpError(HttpStatus status, const std::string &message);
};
} // namespace cppserver

#endif /* CLIENT_H */
