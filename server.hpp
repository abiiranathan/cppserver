#ifndef SERVER_H
#define SERVER_H

#include "router.hpp"
extern "C" {
#include <arpa/inet.h>
#include <curl/curl.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <unistd.h>
}

#include <iostream>
#include <stdexcept>
#include <string>

#include "threadpool.hpp"

#define MAX_EVENTS 100
#define POOL_SIZE 5

// Called with method and request url to match with RouteHandler to call.
namespace cppserver {
// Set socket file descriptor to non-blocking mode.
void nonblocking(int sockfd);

// Register a socket file descriptor with epoll.
// Specify the events to register.
void epoll_ctl_add(int epoll_fd, int sock_fd, struct epoll_event *event,
                   uint32_t events);

class TCPServer {
 private:
  int server_fd;                   // Server socket file descriptor.
  int port;                        // Port to bind server.
  int epoll_fd;                    // epoll file descriptor.
  ThreadPool *pool;                // ThreadPool
  struct sockaddr_in server_addr;  // Server address.
  // Epoll events
  struct epoll_event event, events[MAX_EVENTS];

  // Event loop.
  void RunForever();

  // Handle request.
  void HandleClient(int client_fd);

  // Accept new client connection and register it with epoll.
  // Returns client file descriptor or -1 for errors.
  int Accept();

  // Unregister client
  void UnregisterClient(int client_fd);

 public:
  // Implicit copy/move members
  TCPServer(const TCPServer &) = default;
  TCPServer(TCPServer &&) = delete;

  TCPServer &operator=(const TCPServer &) = default;
  TCPServer &operator=(TCPServer &&) = delete;

  explicit TCPServer(int port);  // constructor
  ~TCPServer();                  // destructor

  // Start the event loop.
  void Listen();
};

};  // namespace cppserver

#endif /* SERVER_H */
