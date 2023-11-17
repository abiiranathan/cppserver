
#include "url.hpp"

URL::URL(const std::string &url) {
  CURLU *urlhandle = curl_url();
  CURLUcode ucode;

  // Parse the URL
  ucode = curl_url_set(urlhandle, CURLUPART_URL, url.data(), 0);
  if (ucode != CURLUE_OK) {
    std::string error;
    error = std::string("URL parsing failed: ") + curl_url_strerror(ucode);
    curl_url_cleanup(urlhandle);
    throw std::invalid_argument(error);
  }

  char *m_scheme = NULL, *m_host = NULL, *m_path = NULL, *m_query = NULL,
       *m_port = NULL;

  curl_url_get(urlhandle, CURLUPART_SCHEME, &m_scheme, 0);
  curl_url_get(urlhandle, CURLUPART_HOST, &m_host, 0);
  curl_url_get(urlhandle, CURLUPART_PATH, &m_path, 0);
  curl_url_get(urlhandle, CURLUPART_QUERY, &m_query, 0);
  curl_url_get(urlhandle, CURLUPART_PORT, &m_port, 0);
  curl_url_cleanup(urlhandle);

  // validate scheme, host, path
  if (m_scheme == NULL) {
    throw std::runtime_error("invalid scheme");
  }
  if (m_host == NULL) {
    throw std::runtime_error("invalid host");
  }
  if (m_path == NULL) {
    throw std::runtime_error("invalid path");
  }

  // Assign to members
  original_url = url;
  scheme = m_scheme;
  host = m_host;
  path = m_path;
  if (m_query != NULL) {
    query = m_query;
  }

  if (m_port == NULL) {
    if (strcmp(m_scheme, "https") == 0) {
      port = "443";
    } else if (strcmp(m_scheme, "http") == 0) {
      port = 80;
    }
  } else {
    port = m_port;
  }

  // Free allocated resources.
  for (char *ptr : {m_scheme, m_host, m_path, m_query, m_port}) {
    if (ptr != NULL) {
      curl_free(ptr);
    }
  }
}

std::string URL::toString() const {
  std::string result;
  result += scheme + "://";
  result += host;

  if (!port.empty() && port != "80" && port != "443") {
    result += ":" + port;
  }

  if (!query.empty()) {
    result += "?" + query;
  }
  return result;
}

void urldecode(char *dst, size_t dst_size, const char *src) {
  char a, b;
  size_t written = 0; // Track the number of characters written to dst

  while (*src &&
         written + 1 <
             dst_size) { // Ensure there's space for at least one more character
    if ((*src == '%') && ((a = src[1]) && (b = src[2])) &&
        (isxdigit(a) && isxdigit(b))) {
      if (a >= 'a')
        a -= 'a' - 'A';
      if (a >= 'A')
        a -= 'A' - 10;
      else
        a -= '0';
      if (b >= 'a')
        b -= 'a' - 'A';
      if (b >= 'A')
        b -= 'A' - 10;
      else
        b -= '0';
      *dst++ = 16 * a + b;
      src += 3;
      written++;
    } else {
      *dst++ = *src++;
      written++;
    }
  }

  // Null-terminate the destination buffer
  *dst = '\0';
}
