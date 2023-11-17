#include "request.hpp"
#include <sstream>
#include <string>

const std::string LF = "\r\n";
const std::string DOUBLE_LF = "\r\n\r\n";
const std::string SCHEME = "http";

Request::Request() {
  body = "";
  headers.reserve(20);
  method = HttpMethod::INVALID;
}

void Request::ParseHttp(const std::string &req_data) {
  try {
    ParseMethodAndPath(req_data);
    ParseHeaders(req_data);
    headers.shrink_to_fit();
    ParseBody(req_data);
  } catch (...) {
    throw;
  }
}

bool Request::caseInsensitiveStringCompare(const std::string &str1,
                                           const std::string &str2) {
  // Make sure the strings are of the same length
  if (str1.length() != str2.length()) {
    return false;
  }
  return std::equal(str1.begin(), str1.end(), str2.begin(), str2.end(),
                    [](char ch1, char ch2) {
                      return std::tolower(ch1) == std::tolower(ch2);
                    });
}

Header *Request::findRequestHeader(const std::string &name) {
  size_t i;
  for (i = 0; i < headers.size(); i++) {
    if (caseInsensitiveStringCompare(headers.at(i).name, name)) {
      return &headers.at(i);
    }
  }
  return nullptr;
}

// Getters
std::vector<Header> Request::getHeaders() { return headers; }

HttpMethod Request::getMethod() const { return method; }
const std::string &Request::Body() const { return body; };
std::unique_ptr<URL> &Request::getURL() { return url; }

std::string *Request::Query(const std::string &key,
                            const std::string &defaultValue) {
  for (auto &pair : queries) {
    if (pair.first == key) {
      return &pair.second;
    }
  }

  // Key not found, return a pointer to the default value
  return const_cast<std::string *>(&defaultValue);
}

void Request::ParseMethodAndPath(const std::string &req_data) {
  std::string method_string;
  std::string version;

  std::istringstream requestStream(req_data);
  std::string requestLine;

  // Read the first line of the HTTP request
  if (std::getline(requestStream, requestLine)) {
    std::istringstream lineStream(requestLine);

    // Extract method, path, and version
    lineStream >> method_string >> path >> version;
  } else {
    throw std::runtime_error("Invalid Http request");
  }

  std::cout << method_string << " " << path << " " << version << std::endl;

  // SET http method member
  method = method_fromstring(method_string);
  if (method == HttpMethod::INVALID) {
    throw std::runtime_error("Invalid Http method");
  }
}

void Request::ParseHeaders(const std::string &req_data) {
  // Parse headers from the request
  const char *header_start = strstr(req_data.data(), LF.data());
  if (!header_start) {
    throw std::runtime_error("cannot parse header end: Invalid HTTP format");
  }

  // Set header start member
  header_start_pos = (header_start - req_data.data()) + 2; // skip LF
  const char *header_end = strstr(req_data.data(), DOUBLE_LF.data());
  if (!header_end) {
    throw std::runtime_error("cannot parse header end: Invalid HTTP format");
  }

  // Set header end pos
  header_end_pos = header_end - req_data.data();
  size_t header_length = header_end_pos - header_start_pos;

  // Copy headers from request data.
  std::string header_content(req_data.substr(header_start_pos, header_length));
  header_content.push_back('\0');

  // parse a line and create a Header object and append it to headers
  auto parseLine = [this](const std::string &line) {
    size_t pos = line.find(": ");
    if (pos != std::string::npos) {
      headers.push_back(Header{line.substr(0, pos), line.substr(pos + 2)});
    } else {
      throw std::runtime_error("Invalid header format: " + line);
    }
  };

  // Split header_content into individual header lines
  std::vector<std::string> header_lines;
  size_t pos = 0;
  while ((pos = header_content.find("\r\n")) != std::string::npos) {
    header_lines.push_back(header_content.substr(0, pos));
    header_content.erase(0, pos + 2); // Move past the "\r\n"
  }
  // Add the last header line (or the only line if there's only one)
  header_lines.push_back(header_content);

  for (const auto &header : header_lines) {
    try {
      parseLine(header);
    } catch (const std::runtime_error &e) {
      std::cerr << "Error: " << e.what() << std::endl;
    }
  }

  // Get content length from headers
  if (!isSafeMethod(method)) {
    for (auto &header : headers) {
      if (caseInsensitiveStringCompare(header.name, "Content-Length")) {
        // Update content length member
        content_length = std::atoi(header.value.data());
        break;
      }
    }
  }

  // Parse URL
  // Get the Host header and compose the full url
  Header *hostHeader = findRequestHeader("Host");
  if (!hostHeader) {
    throw std::runtime_error("Host header must be set for proper URL parsing");
  }

  std::string url_string;
  url_string += SCHEME + "://";
  url_string += hostHeader->value;
  url_string += path;

  //   Set the URL
  url = std::make_unique<URL>(URL(url_string));

  // Set query params in unordered map 'queries'.
  if (!url->query.empty()) {
    size_t pos = 0;
    while ((pos = url->query.find('&')) != std::string::npos) {
      // Split key-value pairs based on '&'
      std::string key_value = url->query.substr(0, pos);
      size_t equals_pos = key_value.find('=');
      if (equals_pos != std::string::npos) {
        std::string key = key_value.substr(0, equals_pos);
        std::string value = key_value.substr(equals_pos + 1);
        // Insert key-value pair into the map
        queries[key] = value;
      }
      url->query.erase(0, pos + 1); // Move past the '&'
    }

    // Process the last key-value pair (or the only one if there's only one)
    size_t equals_pos = url->query.find('=');
    if (equals_pos != std::string::npos) {
      std::string key = url->query.substr(0, equals_pos);
      std::string value = url->query.substr(equals_pos + 1);
      // Insert key-value pair into the map
      queries[key] = value;
    }
  }
}

void Request::ParseBody(const std::string &req_data) {
  if (isSafeMethod(method))
    return;
  body = req_data.substr(header_end_pos + 4, content_length);
}
