#ifndef ROUTER_H
#define ROUTER_H

#define PCRE2_CODE_UNIT_WIDTH 8

#include <pcre2.h>

#include "response.hpp"

typedef enum RouteType { NormalRoute, StaticRoute } RouteType;
typedef void (*RouteHandler)(Response *response);

class Route {
  HttpMethod method;    // HTTP Method.
  std::string pattern;  // Pattern as a string

  // PCRE - compiled extended regex pattern See
  // https://www.pcre.org/current/doc/html/
  pcre2_code *compiledPattern;
  RouteHandler handler;  // Handler for the route
  RouteType type;        // Type of Route
  std::string dirname;   // Dirname for static route.

 public:
  // Public constructor
  // If regex are not included in pattern, they are added.
  // Only ^ & $ are added to avoid partial matches.
  Route(HttpMethod method, const std::string &pattern, RouteHandler handler,
        RouteType type);

  // Explicit member copy constructor
  Route(const Route &);

  //   get the registered router handler
  RouteHandler getRouteHandler();
  HttpMethod getMethod() const;
  const std::string &getPattern() const;
  pcre2_code_8 *getCompiledPattern() const;
  RouteHandler getHandler() const;
  RouteType getType() const;
  const std::string &getDirname() const;
  void setDirname(const std::string &dir) {
    if (type == StaticRoute) {
      dirname = dir;
    }
  }
};

// Function to expand the tilde (~) character in a path to
// the user's home directory
char *expandVar(const std::string &path);

// Match the best regex pattern.
Route *matchBestRoute(HttpMethod method, const std::string &path);

// Global router;
class Router {
 public:
  // Register routes
  void GET(const std::string &pattern, RouteHandler handler);
  void POST(const std::string &pattern, RouteHandler handler);
  void PUT(const std::string &pattern, RouteHandler handler);
  void PATCH(const std::string &pattern, RouteHandler handler);
  void DELETE(const std::string &pattern, RouteHandler handler);
  void OPTIONS(const std::string &pattern, RouteHandler handler);

  // Serve static directory at dirname.
  // e.g   STATIC("/web", "/var/www/html");
  void STATIC(const std::string &pattern, const std::string &dirname);
};

#endif /* ROUTER_H */
