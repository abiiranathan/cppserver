#include "response.hpp"
#include "mime.hpp"

static bool caseInsensitiveStringCompare(const std::string &str1,
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

Response::Response(cppserver::Client *client, std::unique_ptr<Request> &request)
    : chunked(false), stream_complete(false), status(HttpStatus::StatusOK),
      headers_sent(false), body_sent(false), client(client), request(request) {}

// Getters
bool Response::isChunked() const { return chunked; }
bool Response::isStreamComplete() const { return stream_complete; }
HttpStatus Response::getStatus() const { return status; }
const std::vector<Header> &Response::getHeaders() const { return headers; }
cppserver::Client *Response::getClient() const { return client; }
std::unique_ptr<Request> &Response::getRequest() const { return request; }

Header *Response::findResponseHeader(const std::string &name) {
  size_t i;
  for (i = 0; i < headers.size(); i++) {
    if (caseInsensitiveStringCompare(headers.at(i).name, name)) {
      return &headers.at(i);
    }
  }
  return nullptr;
}

// Setters
void Response::setChunked(bool value) { chunked = value; }
void Response::setStreamComplete(bool value) { stream_complete = value; }
void Response::setStatus(HttpStatus value) { status = value; }
void Response::setHeader(const std::string &name, const std::string &value) {
  headers.push_back({name, value});
}

void Response::writeHeaders() {
  if (headers_sent)
    return;

  // Set default status code if none exists
  if (status == 0) {
    status = HttpStatus::StatusOK;
  }

  std::string headerData =
      "HTTP/1.1 " + std::to_string(status) + " " + StatusText(status) + "\r\n";

  // Add headers
  for (size_t i = 0; i < headers.size(); i++) {
    Header h = headers[i];
    if (!h.name.empty()) {
      headerData += h.name + ": " + h.value + "\r\n";
    }
  }

  // Add an additional line break before the body
  headerData += "\r\n";

  // Send the response headers
  int bytes_sent = client->Send(headerData);
  if (bytes_sent == -1) {
    perror("send");
    throw std::runtime_error("failed to send HTTP headers to client");
  }
  headers_sent = true;
}

// Sending data
int Response::Send(const std::string &data) {
  if (body_sent) {
    throw std::runtime_error("body already sent");
  }

  // Assume user set all neccessary headers
  setHeader("Content-Length", std::to_string(data.size()));
  Header *contentType = findResponseHeader("Content-Type");
  if (!contentType) {
    setHeader("Content-Type", "text/html");
  }

  try {
    writeHeaders();
    int n = client->Send(data);
    if (n == -1) {
      throw std::runtime_error("error sending data");
    }
    body_sent = true;
    return n;
  } catch (std::exception &e) {
    std::cerr << "Error sending response: " << e.what() << std::endl;
    return -1;
  }
}

static void write_range_headers(Response *res, ssize_t start, ssize_t end,
                                off64_t file_size) {
  std::string content_len = std::to_string(end - start + 1);
  res->setHeader("Accept-Ranges", "bytes");
  res->setHeader("Content-Length", content_len);
  res->setHeader("Content-Range", "bytes " + std::to_string(start) + "-" +
                                      std::to_string(end) + "/" +
                                      std::to_string(file_size));

  // Set the appropriate status code for partial content
  res->setStatus(StatusPartialContent);
}

int Response::SendFile(const std::string &fname) {
  if (body_sent) {
    throw std::runtime_error("body already sent");
  }

  Header *contentType = findResponseHeader("Content-Type");

  // Decode filename
  std::string filename;
  filename.reserve(fname.size() + 1);
  urldecode(filename.data(), fname.size() + 1, fname.c_str());

  // If content-type not already set by user, guess it from our mapped content
  // types.
  if (!contentType) {
    setHeader("Content-Type", getContentType(filename));
  }

  ssize_t start, end;
  const char *range_header = NULL;
  bool valid_range = false;
  bool has_end_range = false;

  Header *h = request->findRequestHeader("Range");
  if (h) {
    range_header = h->value.c_str();

    if (strstr(range_header, "bytes=") != NULL) {
      if (sscanf(range_header, "bytes=%ld-%ld", &start, &end) == 2) {
        valid_range = true;
        has_end_range = true;
      } else if (sscanf(range_header, "bytes=%ld-", &start) == 1) {
        valid_range = true;
        has_end_range = false;
      };
    }
  }

  // Open the file with ftello64 for large file support
  FILE *file = fopen64(filename.c_str(), "rb");
  if (file == NULL) {
    fprintf(stderr, "Unable to open the file\n");
    perror("fopen64");
    setStatus(StatusInternalServerError);
    writeHeaders();
    return -1;
  }

  // determine file size.
  fseeko64(file, 0, SEEK_END);
  off64_t file_size = ftello64(file);
  fseeko64(file, 0, SEEK_SET);

  // Set appropriate headers for partial content
  if (valid_range) {
    if (start >= file_size) {
      printf("The requested range is outside of the file size");
      setStatus(StatusRequestedRangeNotSatisfiable);
      writeHeaders();
      return -1;
    }

    ssize_t range_chunk_size = (4 * 1024 * 1024) - 1; // send 4MB
    if (!has_end_range && start >= 0) {
      if (start == 0) {
        end = file_size - 1;
      } else {
        end = start + range_chunk_size;
        if (end >= file_size) {
          end = file_size - 1; // End of range
        }
      }
    } else if (start < 0) {
      start = file_size + start; // filesize - start(from end of file)
      end = start + range_chunk_size;
      if (end >= file_size) {
        end = file_size - 1; // End of range
      }
    } else if (end < 0) {
      end = file_size + end;
      if (end >= file_size) {
        end = file_size - 1; // End of range
      }
    }

    // Sanity checks
    if (start < 0 || end < 0 || end >= file_size) {
      printf("The requested range is outside of the file size\n");
      setStatus(StatusRequestedRangeNotSatisfiable);
      writeHeaders();
      return -1;
    }

    write_range_headers(this, start, end, file_size);

    // Move file position to the start of the requested range
    if (fseeko64(file, start, SEEK_SET) != 0) {
      setStatus(StatusRequestedRangeNotSatisfiable);
      writeHeaders();
      perror("fseeko64");
      fclose(file);
      return -1;
    }
  } else {
    // Normal NON-RANGE REQUESTS
    std::string content_length = std::to_string(file_size);
    setHeader("Content-Length", content_length);
  }

  setHeader("Connection", "close");
  writeHeaders();

  // Read and send the file in chunks
  ssize_t total_bytes_sent = 0;
  char buffer[BUFSIZ];
  ssize_t chunk_size;
  ssize_t buffer_size = sizeof(buffer);

  // If it's a valid range request, adjust the buffer size
  if (valid_range) {
    // Ensure the buffer size doesn't exceed the remaining bytes in the
    // requested range
    off64_t remaining_bytes = (end - start + 1);
    buffer_size = remaining_bytes < (off64_t)sizeof(buffer)
                      ? remaining_bytes
                      : (off64_t)sizeof(buffer);
  }

  while ((chunk_size = fread(buffer, 1, buffer_size, file)) > 0) {
    ssize_t body_bytes_sent; // total body bytes
    ssize_t sent = 0;        // total_bytes for this chunk

    while (sent < chunk_size) {
      body_bytes_sent = send(client->fd(), buffer + sent, chunk_size - sent, 0);
      if (body_bytes_sent == -1) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
          // Handle non-blocking case, e.g., retry the send later
          continue;
        } else {
          perror("send");
          fclose(file);
          return -1;
        }
      } else if (body_bytes_sent == 0) {
        // Connection closed by peer
        fclose(file);
        return total_bytes_sent;
      } else {
        sent += body_bytes_sent;
        total_bytes_sent += body_bytes_sent;
      }
    }

    // If it's a range request, and we've sent the requested range, break out
    // of the loop
    if (valid_range && total_bytes_sent >= (end - start + 1)) {
      break;
    }

    // Update the remaining bytes based on the data sent to the client.
    if (valid_range) {
      off64_t remaining_bytes = (end - start + 1) - total_bytes_sent;
      buffer_size = remaining_bytes < (off64_t)sizeof(buffer)
                        ? remaining_bytes
                        : (off64_t)sizeof(buffer);
    }
  }

  fclose(file);
  body_sent = true;
  return total_bytes_sent;
}
