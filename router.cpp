#include "router.hpp"

// Store all registered routes. Calls to GET/POST etc append to this vector.
static std::vector<Route> routes;

// GETTERS
HttpMethod Route::getMethod() const { return method; }
const std::string &Route::getPattern() const { return pattern; }
pcre2_code_8 *Route::getCompiledPattern() const { return compiledPattern; }
RouteHandler Route::getHandler() const { return handler; }
RouteType Route::getType() const { return type; }
const std::string &Route::getDirname() const { return dirname; }

Route::Route(HttpMethod method, const std::string &pattern,
             RouteHandler handler, RouteType type)
    : method(method), handler(handler), type(type) {
  if (pattern.empty()) {
    std::cerr << "pattern must be at least one character" << std::endl;
    exit(1);
  }

  // Set transformed pattern
  std::string anchoredPattern;

  if (type == NormalRoute) {
    if (pattern.front() != '^' && pattern.back() != '$') {
      anchoredPattern = "^" + pattern + "$";
    } else if (pattern.front() != '^') {
      anchoredPattern = "^" + pattern;
    } else if (pattern.back() != '$') {
      anchoredPattern = pattern + "$";
    } else {
      anchoredPattern = pattern;
    }
  } else {
    anchoredPattern = pattern;
  }

  this->pattern = anchoredPattern;

  // Compile the pattern
  int error_code;
  PCRE2_SIZE error_offset;
  this->compiledPattern =
      pcre2_compile((PCRE2_SPTR)this->pattern.c_str(), PCRE2_ZERO_TERMINATED, 0,
                    &error_code, &error_offset, NULL);

  // Check for compilation errors
  if (this->compiledPattern == NULL) {
    PCRE2_UCHAR buffer[256];
    pcre2_get_error_message(error_code, buffer, sizeof(buffer));
    std::cerr << "PCRE2 compilation error at offset " << error_offset << ": "
              << buffer << std::endl;
    exit(1);
  }
}

// Copy constructor
Route::Route(const Route &other)
    : method(other.method),
      pattern(other.pattern),
      compiledPattern(other.compiledPattern),
      handler(other.handler),
      type(other.type),
      dirname(other.dirname) {
  // If compiledPattern is a pointer, we might need to perform a deep copy
  // here. Otherwise, the default member-wise copy should be sufficient.
}

void Router::GET(const std::string &pattern, RouteHandler handler) {
  Route route(HttpMethod::GET, pattern, handler, NormalRoute);
  routes.push_back(route);
}

void Router::POST(const std::string &pattern, RouteHandler handler) {
  Route route(HttpMethod::POST, pattern, handler, NormalRoute);
  routes.push_back(route);
}

void Router::PUT(const std::string &pattern, RouteHandler handler) {
  Route route(HttpMethod::PUT, pattern, handler, NormalRoute);
  routes.push_back(route);
}

void Router::PATCH(const std::string &pattern, RouteHandler handler) {
  Route route(HttpMethod::PATCH, pattern, handler, NormalRoute);
  routes.push_back(route);
}

void Router::DELETE(const std::string &pattern, RouteHandler handler) {
  Route route(HttpMethod::DELETE, pattern, handler, NormalRoute);
  routes.push_back(route);
}

void Router::OPTIONS(const std::string &pattern, RouteHandler handler) {
  Route route(HttpMethod::OPTIONS, pattern, handler, NormalRoute);
  routes.push_back(route);
}

void Router::STATIC(const std::string &pattern, const std::string &dirname) {
  Route route(HttpMethod::GET, pattern, nullptr, StaticRoute);
  route.setDirname(dirname);
  routes.push_back(route);
}

RouteHandler Route::getRouteHandler() { return handler; }

Route *matchBestRoute(HttpMethod method, const std::string &path) {
  Route *bestMatch = NULL;
  size_t bestMatchLength = 0;
  size_t subject_length = path.size();

  for (auto &route : routes) {
    if (route.getType() == NormalRoute) {
      // Use pre-compiled PCRE2 pattern
      int rc;
      pcre2_match_data *match_data = NULL;
      match_data = pcre2_match_data_create_from_pattern(
          route.getCompiledPattern(), NULL);

      if (match_data == NULL) {
        printf("Failed to create match data for pattern: %s\n",
               route.getPattern().c_str());
        return NULL;
      }

      rc = pcre2_match(route.getCompiledPattern(), (PCRE2_SPTR)path.c_str(),
                       subject_length, 0, 0, match_data, NULL);

      if (rc >= 0 && route.getMethod() == method) {
        size_t matchLength = pcre2_get_ovector_pointer(match_data)[1] -
                             pcre2_get_ovector_pointer(match_data)[0];

        // Ensure the match covers the entire string
        if (matchLength == subject_length) {
          if (matchLength > bestMatchLength) {
            bestMatch = &route;
            bestMatchLength = matchLength;
          }
        }
      }

      pcre2_match_data_free(match_data);
    } else if (route.getType() == StaticRoute) {
      // Match static route
      if (route.getPattern() == path && route.getType() == StaticRoute) {
        bestMatch = &route;
        break;
      }
    } else {
      fprintf(stderr, "Unknown route type\n");
      return NULL;
    }
  }
  return bestMatch;
}
