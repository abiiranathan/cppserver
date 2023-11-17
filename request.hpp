#ifndef REQUEST_H
#define REQUEST_H

#include "url.hpp"
#include <cstdio>
#include <cstring>
#include <memory>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

// default scheme is "http"
extern const std::string SCHEME;

// Valid Http methods.
enum HttpMethod {
  INVALID = -1,
  OPTIONS,
  GET,
  POST,
  PUT,
  PATCH,
  DELETE,
};

__attribute__((always_inline)) inline const std::string
method_tostring(HttpMethod method) {
  static const std::unordered_map<HttpMethod, std::string> methodToStringMap = {
      {HttpMethod::OPTIONS, "OPTIONS"}, {HttpMethod::GET, "GET"},
      {HttpMethod::POST, "POST"},       {HttpMethod::PUT, "PUT"},
      {HttpMethod::PATCH, "PATCH"},     {HttpMethod::DELETE, "DELETE"},
  };

  auto it = methodToStringMap.find(method);
  if (it != methodToStringMap.end()) {
    return it->second;
  } else {
    throw std::invalid_argument("Invalid HttpMethod");
  }
}

__attribute__((always_inline)) inline HttpMethod
method_fromstring(const std::string &method) {
  static const std::unordered_map<std::string, HttpMethod> stringToMethodMap = {
      {"OPTIONS", HttpMethod::OPTIONS}, {"GET", HttpMethod::GET},
      {"POST", HttpMethod::POST},       {"PUT", HttpMethod::PUT},
      {"PATCH", HttpMethod::PATCH},     {"DELETE", HttpMethod::DELETE},
  };

  auto it = stringToMethodMap.find(method);
  return (it != stringToMethodMap.end()) ? it->second : HttpMethod::INVALID;
}

__attribute__((always_inline)) inline bool isSafeMethod(HttpMethod method) {
  switch (method) {
  case HttpMethod::OPTIONS:
  case HttpMethod::GET:
    return true;
  default:
    return false;
  }
}

class Request {
private:
  HttpMethod method;           // enum for the request method.
  std::string path;            // Pathname
  std::unique_ptr<URL> url;    // URL for this request
  std::vector<Header> headers; // vector of request headers
  std::string body;            // Body of request;
  size_t header_start_pos;     // Header start index
  size_t header_end_pos;       // Header end index
  size_t content_length;       // Content Length(for POST/PUT/PATCH/DELETE)

  std::unordered_map<std::string, std::string> queries; // Query params

  void ParseMethodAndPath(const std::string &req_data);
  void ParseHeaders(const std::string &req_data);
  void ParseBody(const std::string &req_data);

  bool caseInsensitiveStringCompare(const std::string &str1,
                                    const std::string &str2);

public:
  // constructor
  explicit Request();

  // destructor
  ~Request() = default;

  // Parse http request from client
  void ParseHttp(const std::string &req_data);

  Header *findRequestHeader(const std::string &name);

  // Getters
  std::vector<Header> getHeaders();

  HttpMethod getMethod() const;
  const std::string &Body() const;
  std::unique_ptr<URL> &getURL();

  std::string *Query(const std::string &key,
                     const std::string &defaultValue = "");
};

#endif /* REQUEST_H */
