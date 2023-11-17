#include "server.hpp"

#include <sys/stat.h>
#include <unistd.h>

#include "client.hpp"
#include "request.hpp"
#include "response.hpp"

volatile sig_atomic_t should_exit = 0;

// Define a handler function for serving static files
static void staticFileHandler(Response *res, Route *route);

static void handle_sigint(int signal) {
  if (signal == SIGINT || signal == SIGKILL) {
    should_exit = 1;
    printf("\nDetected %s (%d).\n", strsignal(signal), signal);
  }
}

static void install_sigint_handler() {
  struct sigaction sa;
  sa.sa_handler = handle_sigint;
  sigemptyset(&sa.sa_mask);
  sa.sa_flags = 0;
  sigaction(SIGINT, &sa, NULL);

  // Ignore SIGPIPE
  // Otherwise it will crash the program.
  signal(SIGPIPE, SIG_IGN);
}

void cppserver::nonblocking(int sockfd) {
  int flags = fcntl(sockfd, F_GETFL, 0);
  if (flags == -1) {
    perror("fcntl");
    exit(EXIT_FAILURE);
  }

  if (fcntl(sockfd, F_SETFL, flags | O_NONBLOCK) == -1) {
    perror("fcntl");
    exit(EXIT_FAILURE);
  }
}

void cppserver::epoll_ctl_add(int epoll_fd, int sock_fd,
                              struct epoll_event *event, uint32_t events) {
  event->data.fd = sock_fd;
  event->events = events;
  if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, sock_fd, event) == -1) {
    perror("epoll_ctl");
    exit(EXIT_FAILURE);
  }
}

cppserver::TCPServer::TCPServer(int port) {
  this->port = port;
  server_fd = socket(AF_INET, SOCK_STREAM, 0);
  if (server_fd == -1) {
    perror("socket");
    exit(EXIT_FAILURE);
  }
  nonblocking(server_fd);

  int enable = 1;
  if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int)) <
      0) {
    perror("setsockopt");
    exit(1);
  }

  memset(&server_addr, 0, sizeof(server_addr));
  server_addr.sin_family = AF_INET;
  server_addr.sin_addr.s_addr = INADDR_ANY;
  server_addr.sin_port = htons(port);

  if (bind(server_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) ==
      -1) {
    perror("bind");
    exit(EXIT_FAILURE);
  }

  // Listen for connections
  if (listen(server_fd, SOMAXCONN) == -1) {
    perror("listen");
    exit(EXIT_FAILURE);
  }

  // Create epoll instance
  epoll_fd = epoll_create1(0);
  if (epoll_fd == -1) {
    perror("epoll_create1");
    exit(1);
  }

  // register events with epoll
  epoll_ctl_add(epoll_fd, server_fd, &event, EPOLLIN);

  // Init thread pool with
  pool = new ThreadPool;
}

int cppserver::TCPServer::Accept() {
  int client_fd;
  socklen_t client_len = sizeof(server_addr);
  client_fd = accept(server_fd, (struct sockaddr *)&server_addr, &client_len);
  if (client_fd != -1) {
    nonblocking(client_fd);
    epoll_ctl_add(epoll_fd, client_fd, &event,
                  EPOLLIN | EPOLLET | EPOLLONESHOT);
  }
  return client_fd;
}

void cppserver::TCPServer::Listen() {
  curl_global_init(CURL_GLOBAL_DEFAULT);
  install_sigint_handler();

  printf("Server listening on port %d\n", port);
  RunForever();
}

static void handleRequest(int client_fd, int epoll_fd) {
  cppserver::Client client(client_fd, epoll_fd);
  std::string request_buffer;
  int bytes_read = client.Read(request_buffer);

  if (bytes_read == -1) {
    client.SendHttpError(HttpStatus::StatusBadRequest,
                         "Unable to process request\n");
    return;
  }

  // Create a new request.
  std::unique_ptr<Request> req = std::make_unique<Request>();

  try {
    req->ParseHttp(request_buffer);
  } catch (const std::exception &e) {
    std::cerr << "Exception caught: " << e.what() << std::endl;
    client.SendHttpError(HttpStatus::StatusBadRequest, e.what());
    return;
  }

  try {
    Response response(&client, req);
    Route *matchingRoute =
        matchBestRoute(req->getMethod(), req->getURL()->path);
    if (!matchingRoute) {
      client.SendHttpError(HttpStatus::StatusNotFound, "Not Found");
      return;
    }

    if (matchingRoute->getType() == NormalRoute) {
      RouteHandler handler = matchingRoute->getRouteHandler();
      handler(&response);
    } else {
      staticFileHandler(&response, matchingRoute);
    }

  } catch (std::exception &e) {
    std::cerr << "error sending response: " << e.what() << std::endl;
  }
}

void cppserver::TCPServer::HandleClient(int client_fd) {
  pool->QueueJob(handleRequest, client_fd, epoll_fd);
}

void cppserver::TCPServer::UnregisterClient(int client_fd) {
  epoll_ctl(epoll_fd, EPOLL_CTL_DEL, client_fd, NULL);
}

void cppserver::TCPServer::RunForever() {
  while (!should_exit) {
    int nfds = epoll_wait(epoll_fd, events, MAX_EVENTS, -1);
    if (nfds == -1) {
      perror("epoll_wait");
      break;
    }

    for (int i = 0; i < nfds; i++) {
      if (events[i].data.fd == server_fd) {
        int client_fd = Accept();
        if (client_fd == -1) {
          perror("accept");
          std::cout << "failed to accept new connection" << std::endl;
          exit(1);
        }
      } else {
        int client_fd = events[i].data.fd;
        HandleClient(client_fd);
      }
    }
  }
}

cppserver::TCPServer::~TCPServer() {
  curl_global_cleanup();

  pool->Wait();
  pool->Stop();
  delete pool;

  shutdown(server_fd, SHUT_RDWR);
  close(server_fd);
}

#define MAX_PATH_SIZE 256

static int is_directory(const char *path) {
  struct stat path_stat;
  stat(path, &path_stat);
  return S_ISDIR(path_stat.st_mode);
}

// Define a handler function for serving static files
static void staticFileHandler(Response *res, Route *route) {
  const char *dirname = route->getDirname().c_str();
  const char *requestedPath = res->getRequest()->getURL()->path.c_str();

  // trim prefix(context.route.pattern) from requested path
  // e.g /static -> /
  // Trim the prefix from the requested path
  const char *trimmedPath = requestedPath + route->getPattern().size();

  // Build the full file path by concatenating the requested path with the
  // directory path
  char fullFilePath[MAX_PATH_SIZE];
  snprintf(fullFilePath, MAX_PATH_SIZE, "%s%s", dirname, trimmedPath);

  char decodedPath[MAX_PATH_SIZE];
  urldecode(decodedPath, sizeof(decodedPath), fullFilePath);

  printf("[STATIC]: %s\n", decodedPath);

  // If it's a directory, append /index.html to decoded path
  if (is_directory(decodedPath)) {
    // temporary buffer to hold the concatenated path
    char tempPath[MAX_PATH_SIZE + 16];

    if (decodedPath[strlen(decodedPath) - 1] == '/') {
      snprintf(tempPath, sizeof(tempPath), "%sindex.html", decodedPath);
    } else {
      snprintf(tempPath, sizeof(tempPath), "%s/index.html", decodedPath);
    }

    // Check if the resulting path is within bounds
    if (strlen(tempPath) < MAX_PATH_SIZE) {
      strcpy(decodedPath, tempPath);
    } else {
      // Handle the case where the concatenated path exceeds the buffer size
      std::cerr << "Error: Concatenated path exceeds buffer size" << std::endl;

      res->getClient()->SendHttpError(
          HttpStatus::StatusBadRequest,
          "url is too long to fit in 256 characters");
    }
  }

  res->SendFile(decodedPath);
}
