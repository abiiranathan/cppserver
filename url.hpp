#ifndef URL_H
#define URL_H

extern "C" {
#include <curl/curl.h>
}
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <string>

// Http Headers
struct Header {
  std::string name;
  std::string value;
};

void urldecode(char *dst, size_t dst_size, const char *src);

// Represent a URL object.
struct URL {
  std::string original_url; // Original URL from client.
  std::string scheme;       // Protocol
  std::string host;         // Host
  std::string path;         // PathName for url.
  std::string query;        // Query string if present or NULL.
  std::string port;         // port.

  // Constructor
  URL() = default;
  URL(const std::string &url);
  std::string toString() const;
};

#endif /* URL_H */
