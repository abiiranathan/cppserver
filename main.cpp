#include "router.hpp"
#include "server.hpp"

void homePage(Response *res) { res->Send("Hello from C++\n"); }

int main(void) {
  cppserver::TCPServer server(8080);
  Router router;
  router.GET("/", homePage);
  server.Listen();
  return 0;
}
