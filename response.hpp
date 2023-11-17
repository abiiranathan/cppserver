#ifndef RESPONSE_H
#define RESPONSE_H

#ifndef _LARGEFILE64_SOURCE
#define _LARGEFILE64_SOURCE
#define _FILE_OFFSET_BITS 64
#endif

#include "client.hpp"
#include "request.hpp"
#include "url.hpp"
#include <cstdio>
#include <fstream>
#include <memory>

extern "C" {
#include <sys/stat.h>
#include <unistd.h>
}

// Http response object.
class Response {
private:
  bool chunked;                // Chunked transfer encoding
  bool stream_complete;        // Chunked transfer completed
  HttpStatus status;           // Status code
  std::vector<Header> headers; // Response headers
  bool headers_sent;
  bool body_sent;
  cppserver::Client *client;         // Http client
  std::unique_ptr<Request> &request; // Request pointer

  void writeHeaders();

public:
  // Constructors
  explicit Response(cppserver::Client *client,
                    std::unique_ptr<Request> &request);

  // Getters
  bool isChunked() const;
  bool isStreamComplete() const;
  HttpStatus getStatus() const;
  const std::vector<Header> &getHeaders() const;
  Header *findResponseHeader(const std::string &name);
  cppserver::Client *getClient() const;
  std::unique_ptr<Request> &getRequest() const;

  // Setters
  void setChunked(bool value);
  void setStreamComplete(bool value);
  void setStatus(HttpStatus value);
  void setHeader(const std::string &name, const std::string &value);

  // Sending responses
  int Send(const std::string &data);
  int SendFile(const std::string &filename);
};

#endif /* RESPONSE_H */
