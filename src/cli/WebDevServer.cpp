#include "neuronc/cli/WebDevServer.h"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <cerrno>
#include <cstring>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#if defined(_WIN32)
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "Ws2_32.lib")
#else
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

namespace fs = std::filesystem;

namespace neuron::cli {

namespace {

#if defined(_WIN32)
using SocketHandle = SOCKET;
constexpr SocketHandle kInvalidSocket = INVALID_SOCKET;
#else
using SocketHandle = int;
constexpr SocketHandle kInvalidSocket = -1;
#endif

struct SocketRuntime {
  bool initialized = false;
};

bool initializeSockets(SocketRuntime *runtime, std::string *outError) {
  if (runtime == nullptr) {
    return false;
  }
#if defined(_WIN32)
  WSADATA wsa;
  const int rc = WSAStartup(MAKEWORD(2, 2), &wsa);
  if (rc != 0) {
    if (outError != nullptr) {
      *outError = "WSAStartup failed";
    }
    return false;
  }
#endif
  runtime->initialized = true;
  return true;
}

void shutdownSockets(SocketRuntime *runtime) {
  if (runtime == nullptr || !runtime->initialized) {
    return;
  }
#if defined(_WIN32)
  WSACleanup();
#endif
  runtime->initialized = false;
}

void closeSocket(SocketHandle sock) {
  if (sock == kInvalidSocket) {
    return;
  }
#if defined(_WIN32)
  closesocket(sock);
#else
  close(sock);
#endif
}

bool sendAll(SocketHandle sock, const char *data, size_t size) {
  size_t sent = 0;
  while (sent < size) {
#if defined(_WIN32)
    const int n = send(sock, data + sent,
                       static_cast<int>(std::min<size_t>(size - sent, 1 << 20)),
                       0);
#else
    const ssize_t n = send(sock, data + sent, size - sent, 0);
#endif
    if (n <= 0) {
      return false;
    }
    sent += static_cast<size_t>(n);
  }
  return true;
}

std::string mimeTypeForPath(const fs::path &path) {
  const std::string ext = path.extension().string();
  if (ext == ".html" || ext == ".htm") {
    return "text/html; charset=utf-8";
  }
  if (ext == ".js") {
    return "application/javascript; charset=utf-8";
  }
  if (ext == ".wasm") {
    return "application/wasm";
  }
  if (ext == ".wgsl") {
    return "text/plain; charset=utf-8";
  }
  if (ext == ".json") {
    return "application/json; charset=utf-8";
  }
  if (ext == ".css") {
    return "text/css; charset=utf-8";
  }
  if (ext == ".svg") {
    return "image/svg+xml";
  }
  if (ext == ".png") {
    return "image/png";
  }
  if (ext == ".jpg" || ext == ".jpeg") {
    return "image/jpeg";
  }
  if (ext == ".gif") {
    return "image/gif";
  }
  return "application/octet-stream";
}

std::string statusTextForCode(int code) {
  switch (code) {
  case 200:
    return "OK";
  case 400:
    return "Bad Request";
  case 403:
    return "Forbidden";
  case 404:
    return "Not Found";
  case 405:
    return "Method Not Allowed";
  default:
    return "Internal Server Error";
  }
}

std::string urlDecode(std::string value) {
  std::string out;
  out.reserve(value.size());
  for (size_t i = 0; i < value.size(); ++i) {
    if (value[i] == '%' && i + 2 < value.size()) {
      const char hi = value[i + 1];
      const char lo = value[i + 2];
      auto hexVal = [](char ch) -> int {
        if (ch >= '0' && ch <= '9') {
          return ch - '0';
        }
        if (ch >= 'a' && ch <= 'f') {
          return 10 + (ch - 'a');
        }
        if (ch >= 'A' && ch <= 'F') {
          return 10 + (ch - 'A');
        }
        return -1;
      };
      const int hiVal = hexVal(hi);
      const int loVal = hexVal(lo);
      if (hiVal >= 0 && loVal >= 0) {
        out.push_back(static_cast<char>((hiVal << 4) | loVal));
        i += 2;
        continue;
      }
    }
    if (value[i] == '+') {
      out.push_back(' ');
      continue;
    }
    out.push_back(value[i]);
  }
  return out;
}

bool isPathSafe(const fs::path &relativePath) {
  for (const fs::path &component : relativePath) {
    if (component == "..") {
      return false;
    }
  }
  return true;
}

bool loadFile(const fs::path &path, std::vector<char> *outData) {
  if (outData == nullptr) {
    return false;
  }
  std::ifstream in(path, std::ios::binary);
  if (!in.is_open()) {
    return false;
  }
  outData->assign(std::istreambuf_iterator<char>(in),
                  std::istreambuf_iterator<char>());
  return true;
}

void sendHttpResponse(SocketHandle client, int code, const std::string &mimeType,
                      const std::vector<char> &payload) {
  std::ostringstream headers;
  headers << "HTTP/1.1 " << code << ' ' << statusTextForCode(code) << "\r\n";
  headers << "Content-Type: " << mimeType << "\r\n";
  headers << "Content-Length: " << payload.size() << "\r\n";
  headers << "Cache-Control: no-cache\r\n";
  headers << "Cross-Origin-Opener-Policy: same-origin\r\n";
  headers << "Cross-Origin-Embedder-Policy: require-corp\r\n";
  headers << "Cross-Origin-Resource-Policy: same-origin\r\n";
  headers << "Connection: close\r\n\r\n";

  const std::string headerText = headers.str();
  (void)sendAll(client, headerText.data(), headerText.size());
  if (!payload.empty()) {
    (void)sendAll(client, payload.data(), payload.size());
  }
}

bool openBrowserUrl(const std::string &url) {
#if defined(_WIN32)
  const std::string cmd = "start \"\" \"" + url + "\"";
#elif defined(__APPLE__)
  const std::string cmd = "open \"" + url + "\"";
#else
  const std::string cmd = "xdg-open \"" + url + "\" >/dev/null 2>&1";
#endif
  return std::system(cmd.c_str()) == 0;
}

} // namespace

int runWebDevServer(const WebDevServerOptions &options, std::string *outError) {
  if (options.rootDirectory.empty() || !fs::exists(options.rootDirectory) ||
      !fs::is_directory(options.rootDirectory)) {
    if (outError != nullptr) {
      *outError = "web root directory is missing: " +
                  options.rootDirectory.string();
    }
    return 1;
  }
  if (options.port <= 0 || options.port > 65535) {
    if (outError != nullptr) {
      *outError = "invalid dev server port";
    }
    return 1;
  }

  SocketRuntime socketRuntime;
  if (!initializeSockets(&socketRuntime, outError)) {
    return 1;
  }

  SocketHandle server = socket(AF_INET, SOCK_STREAM, 0);
  if (server == kInvalidSocket) {
    if (outError != nullptr) {
      *outError = "failed to create dev server socket";
    }
    shutdownSockets(&socketRuntime);
    return 1;
  }

  int reuseValue = 1;
#if defined(_WIN32)
  setsockopt(server, SOL_SOCKET, SO_REUSEADDR,
             reinterpret_cast<const char *>(&reuseValue), sizeof(reuseValue));
#else
  setsockopt(server, SOL_SOCKET, SO_REUSEADDR, &reuseValue, sizeof(reuseValue));
#endif

  sockaddr_in address;
  std::memset(&address, 0, sizeof(address));
  address.sin_family = AF_INET;
  address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
  address.sin_port = htons(static_cast<uint16_t>(options.port));

  if (bind(server, reinterpret_cast<sockaddr *>(&address), sizeof(address)) !=
      0) {
    if (outError != nullptr) {
      *outError = "failed to bind dev server socket to 127.0.0.1:" +
                  std::to_string(options.port);
    }
    closeSocket(server);
    shutdownSockets(&socketRuntime);
    return 1;
  }

  if (listen(server, 16) != 0) {
    if (outError != nullptr) {
      *outError = "failed to listen on dev server socket";
    }
    closeSocket(server);
    shutdownSockets(&socketRuntime);
    return 1;
  }

  const std::string serverUrl =
      "http://127.0.0.1:" + std::to_string(options.port) + "/";
  std::cout << "Web dev server listening at " << serverUrl << std::endl;
  std::cout << "Serving directory: " << options.rootDirectory.string()
            << std::endl;

  if (options.openBrowser) {
    (void)openBrowserUrl(serverUrl);
  }

  for (;;) {
    sockaddr_in clientAddr;
#if defined(_WIN32)
    int clientLen = sizeof(clientAddr);
#else
    socklen_t clientLen = sizeof(clientAddr);
#endif
    SocketHandle client = accept(server, reinterpret_cast<sockaddr *>(&clientAddr),
                                 &clientLen);
    if (client == kInvalidSocket) {
      continue;
    }

    char requestBuffer[8192];
#if defined(_WIN32)
    const int received = recv(client, requestBuffer,
                              static_cast<int>(sizeof(requestBuffer) - 1), 0);
#else
    const ssize_t received =
        recv(client, requestBuffer, sizeof(requestBuffer) - 1, 0);
#endif
    if (received <= 0) {
      closeSocket(client);
      continue;
    }

    requestBuffer[received] = '\0';
    std::string requestText(requestBuffer);
    const size_t firstLineEnd = requestText.find("\r\n");
    const std::string requestLine =
        firstLineEnd == std::string::npos ? requestText
                                          : requestText.substr(0, firstLineEnd);

    std::istringstream lineStream(requestLine);
    std::string method;
    std::string uri;
    std::string version;
    lineStream >> method >> uri >> version;

    if (method.empty() || uri.empty()) {
      const std::vector<char> payload{'B', 'a', 'd', ' ', 'R', 'e', 'q', 'u',
                                      'e', 's', 't'};
      sendHttpResponse(client, 400, "text/plain; charset=utf-8", payload);
      closeSocket(client);
      continue;
    }

    if (method != "GET") {
      const std::vector<char> payload{'M', 'e', 't', 'h', 'o', 'd', ' ',
                                      'N', 'o', 't', ' ', 'A', 'l', 'l', 'o',
                                      'w', 'e', 'd'};
      sendHttpResponse(client, 405, "text/plain; charset=utf-8", payload);
      closeSocket(client);
      continue;
    }

    size_t queryPos = uri.find('?');
    if (queryPos != std::string::npos) {
      uri = uri.substr(0, queryPos);
    }
    if (!uri.empty() && uri.front() == '/') {
      uri.erase(uri.begin());
    }
    uri = urlDecode(uri);

    fs::path relativePath = uri.empty() ? fs::path("index.html") : fs::path(uri);
    if (!isPathSafe(relativePath)) {
      const std::vector<char> payload{'F', 'o', 'r', 'b', 'i', 'd', 'd', 'e', 'n'};
      sendHttpResponse(client, 403, "text/plain; charset=utf-8", payload);
      closeSocket(client);
      continue;
    }

    fs::path requestedPath = options.rootDirectory / relativePath;
    if (fs::is_directory(requestedPath)) {
      requestedPath /= "index.html";
    }

    std::vector<char> payload;
    if (!loadFile(requestedPath, &payload)) {
      const std::vector<char> notFound{'N', 'o', 't', ' ', 'F', 'o', 'u', 'n',
                                       'd'};
      sendHttpResponse(client, 404, "text/plain; charset=utf-8", notFound);
      closeSocket(client);
      continue;
    }

    sendHttpResponse(client, 200, mimeTypeForPath(requestedPath), payload);
    closeSocket(client);
  }

  closeSocket(server);
  shutdownSockets(&socketRuntime);
  return 0;
}

} // namespace neuron::cli
