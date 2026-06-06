#define NOMINMAX

#include "platform/HttpServer.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <chrono>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>

#include <winsock2.h>
#include <ws2tcpip.h>

#pragma comment(lib, "Ws2_32.lib")

#include "core/Inventory.h"
#include "platform/Console.h"

namespace hims {

namespace {

std::string jsonEscape(const std::string& value) {
  std::ostringstream out;
  for (char ch : value) {
    switch (ch) {
      case '\\':
        out << "\\\\";
        break;
      case '"':
        out << "\\\"";
        break;
      case '\n':
        out << "\\n";
        break;
      case '\r':
        out << "\\r";
        break;
      case '\t':
        out << "\\t";
        break;
      default:
        out << ch;
        break;
    }
  }
  return out.str();
}

std::string trimHttp(const std::string& value) {
  std::size_t begin = 0;
  while (begin < value.size() && std::isspace(static_cast<unsigned char>(value[begin])) != 0) {
    ++begin;
  }

  std::size_t end = value.size();
  while (end > begin && std::isspace(static_cast<unsigned char>(value[end - 1])) != 0) {
    --end;
  }

  return value.substr(begin, end - begin);
}

std::string extractScanCode(const std::string& body) {
  const auto raw = trimHttp(body);
  if (raw.empty()) {
    return {};
  }

  if (raw.front() != '{') {
    return raw;
  }

  const auto codePos = raw.find("\"code\"");
  if (codePos == std::string::npos) {
    return raw;
  }

  const auto colon = raw.find(':', codePos);
  if (colon == std::string::npos) {
    return raw;
  }

  const auto firstQuote = raw.find('"', colon);
  if (firstQuote == std::string::npos) {
    return raw;
  }

  const auto secondQuote = raw.find('"', firstQuote + 1);
  if (secondQuote == std::string::npos) {
    return raw;
  }

  return raw.substr(firstQuote + 1, secondQuote - firstQuote - 1);
}

}  // namespace

LocalHttpServer::~LocalHttpServer() {
  stop();
}

bool LocalHttpServer::start(std::uint16_t preferredPort, ScanCallback onScan) {
  stop();

  WSADATA data{};
  if (WSAStartup(MAKEWORD(2, 2), &data) != 0) {
    lastError_ = "Failed to initialize Winsock";
    return false;
  }
  winsockStarted_ = true;

  onScan_ = std::move(onScan);

  for (std::uint16_t candidate = preferredPort; candidate < static_cast<std::uint16_t>(preferredPort + 20); ++candidate) {
    if (bindSocket(candidate)) {
      running_.store(true);
      worker_ = std::thread(&LocalHttpServer::workerLoop, this);
      return true;
    }
  }

  lastError_ = "Unable to bind any scanner port";
  if (winsockStarted_) {
    WSACleanup();
    winsockStarted_ = false;
  }
  return false;
}

void LocalHttpServer::stop() {
  running_.store(false);

  if (listenSocket_ != INVALID_SOCKET) {
    shutdown(listenSocket_, SD_BOTH);
    closesocket(listenSocket_);
    listenSocket_ = INVALID_SOCKET;
  }

  if (worker_.joinable()) {
    worker_.join();
  }

  if (winsockStarted_) {
    WSACleanup();
    winsockStarted_ = false;
  }
}

void LocalHttpServer::setRecentActivity(std::vector<ActivityEntry> activities) {
  std::lock_guard<std::mutex> lock(stateMutex_);
  recentActivities_ = std::move(activities);
}

bool LocalHttpServer::running() const {
  return running_.load();
}

std::uint16_t LocalHttpServer::port() const {
  return port_;
}

std::string LocalHttpServer::baseUrl() const {
  const auto addresses = this->addresses();
  const auto host = addresses.empty() ? std::string("127.0.0.1") : addresses.front();
  std::ostringstream out;
  out << "http://" << host << ':' << port_;
  return out.str();
}

std::vector<std::string> LocalHttpServer::addresses() const {
  std::lock_guard<std::mutex> lock(stateMutex_);
  if (!addresses_.empty()) {
    return addresses_;
  }
  return {"127.0.0.1"};
}

std::string LocalHttpServer::lastScan() const {
  std::lock_guard<std::mutex> lock(stateMutex_);
  return lastScan_;
}

bool LocalHttpServer::bindSocket(std::uint16_t port) {
  SOCKET socketHandle = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
  if (socketHandle == INVALID_SOCKET) {
    return false;
  }

  BOOL reuse = TRUE;
  setsockopt(socketHandle, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<const char*>(&reuse), sizeof(reuse));

  sockaddr_in address{};
  address.sin_family = AF_INET;
  address.sin_addr.s_addr = htonl(INADDR_ANY);
  address.sin_port = htons(port);

  if (bind(socketHandle, reinterpret_cast<sockaddr*>(&address), sizeof(address)) == SOCKET_ERROR) {
    closesocket(socketHandle);
    return false;
  }

  if (listen(socketHandle, SOMAXCONN) == SOCKET_ERROR) {
    closesocket(socketHandle);
    return false;
  }

  listenSocket_ = socketHandle;
  port_ = port;
  addresses_ = localAddresses();
  return true;
}

void LocalHttpServer::workerLoop() {
  while (running_.load()) {
    sockaddr_in clientAddress{};
    int clientSize = sizeof(clientAddress);
    SOCKET client = accept(listenSocket_, reinterpret_cast<sockaddr*>(&clientAddress), &clientSize);
    if (client == INVALID_SOCKET) {
      if (running_.load()) {
        continue;
      }
      break;
    }

    std::string request;
    std::array<char, 4096> buffer{};
    int received = 0;
    do {
      received = recv(client, buffer.data(), static_cast<int>(buffer.size()), 0);
      if (received > 0) {
        request.append(buffer.data(), buffer.data() + received);
      }
    } while (received > 0 && request.find("\r\n\r\n") == std::string::npos);

    serveConnection(client, std::move(request));
    closesocket(client);
  }
}

std::string LocalHttpServer::responseText(const std::string& status, const std::string& contentType, const std::string& body) const {
  std::ostringstream out;
  out << "HTTP/1.1 " << status << "\r\n";
  out << "Content-Type: " << contentType << "\r\n";
  out << "Content-Length: " << body.size() << "\r\n";
  out << "Connection: close\r\n";
  out << "Cache-Control: no-store\r\n\r\n";
  out << body;
  return out.str();
}

std::string LocalHttpServer::scanCallbackMessage(const std::string& code) const {
  std::ostringstream out;
  out << "{"
      << "\"ok\":true,"
      << "\"code\":\"" << jsonEscape(code) << "\","
      << "\"message\":\"queued\""
      << "}";
  return out.str();
}

std::string LocalHttpServer::jsonStatus() const {
  std::lock_guard<std::mutex> lock(stateMutex_);
  std::ostringstream out;
  const auto addresses = addresses_;
  const auto activities = recentActivities_;
  out << "{"
      << "\"ok\":true,"
      << "\"port\":" << port_ << ','
      << "\"baseUrl\":\"http://" << (addresses.empty() ? std::string("127.0.0.1") : addresses.front()) << ':' << port_ << "\","
      << "\"lastScan\":\"" << jsonEscape(lastScan_) << "\","
      << "\"addresses\":[";
  for (std::size_t index = 0; index < addresses.size(); ++index) {
    if (index > 0) {
      out << ',';
    }
    out << "\"" << jsonEscape(addresses[index]) << "\"";
  }
  out << "],";
  out << "\"activity\":[";
  for (std::size_t index = 0; index < activities.size(); ++index) {
    if (index > 0) {
      out << ',';
    }
    out << "{"
        << "\"kind\":\"" << jsonEscape(activities[index].kind) << "\","
        << "\"message\":\"" << jsonEscape(activities[index].message) << "\","
        << "\"timestamp\":\"" << jsonEscape(nowTimestampString(activities[index].timestamp)) << "\""
        << "}";
  }
  out << "]";
  out << "}";
  return out.str();
}

std::string LocalHttpServer::scannerPage() const {
  std::ostringstream out;
  out << R"HTML(<!doctype html>
<html lang="en">
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>HIMS Scanner</title>
  <style>
    :root {
      color-scheme: dark;
      --bg: #050607;
      --flash: rgba(64, 255, 128, 0.34);
    }
    * { box-sizing: border-box; }
    html, body {
      width: 100%;
      height: 100%;
    }
    body {
      margin: 0;
      overflow: hidden;
      background: radial-gradient(circle at center, #0b0f10 0%, var(--bg) 100%);
      font-family: ui-sans-serif, system-ui, -apple-system, Segoe UI, Roboto, Arial, sans-serif;
    }
    body::before {
      content: '';
      position: fixed;
      inset: 0;
      background: var(--flash);
      opacity: 0;
      pointer-events: none;
    }
    body.flash::before {
      animation: flashGreen 320ms ease-out;
    }
    @keyframes flashGreen {
      0% { opacity: 0.85; }
      100% { opacity: 0; }
    }
    .frame {
      position: fixed;
      inset: 0;
      overflow: hidden;
      background: #000;
    }
    video {
      position: absolute;
      inset: 0;
      width: 100%;
      height: 100%;
      object-fit: cover;
      background: #000;
    }
    .status {
      position: absolute;
      left: 16px;
      bottom: 16px;
      padding: 8px 10px;
      border-radius: 999px;
      background: rgba(0,0,0,0.45);
      color: rgba(255,255,255,0.72);
      font-size: 12px;
      letter-spacing: 0.08em;
      text-transform: uppercase;
      backdrop-filter: blur(8px);
      opacity: 0;
      transition: opacity 160ms ease;
    }
    .status.show {
      opacity: 1;
    }
    .status.ok {
      background: rgba(64, 255, 128, 0.16);
      color: #b9ffd0;
    }
    .status.err {
      background: rgba(255, 90, 90, 0.16);
      color: #ffd0d0;
    }
    .hidden {
      display: flex;
      position: absolute;
      width: 1px;
      height: 1px;
      opacity: 0;
      pointer-events: none;
      left: -9999px;
    }
  </style>
</head>
<body>
  <div class="frame">
    <video id="video" playsinline muted></video>
    <div class="status" id="status"></div>
    <input id="scanText" class="hidden" autocomplete="off" inputmode="none" aria-hidden="true">
  </div>

  <script>
    let detector = null;
    let stream = null;
    let scanBusy = false;
    let lastScanValue = '';
    let lastScanAt = 0;
    let scanning = false;
    const statusEl = document.getElementById('status');
    const videoEl = document.getElementById('video');
    const bodyEl = document.body;

    function showStatus(message, kind) {
      if (!message) {
        statusEl.className = 'status';
        statusEl.textContent = '';
        return;
      }
      statusEl.textContent = message;
      statusEl.className = 'status show' + (kind ? ` ${kind}` : '');
    }

    async function submitScan(code) {
      const clean = (code || '').trim();
      if (!clean || scanBusy) return;
      scanBusy = true;
      try {
        const response = await fetch('/api/scan', {
          method: 'POST',
          headers: { 'Content-Type': 'application/json' },
          body: JSON.stringify({ code: clean })
        });
        const payload = await response.json();
        if (response.ok) {
          bodyEl.classList.remove('flash');
          void bodyEl.offsetWidth;
          bodyEl.classList.add('flash');
        } else {
          showStatus(payload.message || 'Scan rejected', 'err');
        }
        document.getElementById('scanText').value = '';
      } catch (error) {
        showStatus('Offline', 'err');
      } finally {
        scanBusy = false;
      }
    }

    async function startCamera() {
      try {
        stream = await navigator.mediaDevices.getUserMedia({ video: { facingMode: 'environment' } });
        videoEl.srcObject = stream;
        await videoEl.play();
        if ('BarcodeDetector' in window) {
          detector = new BarcodeDetector({ formats: ['qr_code', 'data_matrix', 'code_128', 'ean_13'] });
          if (!scanning) {
            scanning = true;
            scanLoop();
          }
        } else if (!detector) {
          showStatus('Barcode detector unavailable', 'err');
        }
      } catch (error) {
        showStatus('Camera denied', 'err');
      }
    }

    async function scanLoop() {
      if (!detector || !stream) {
        scanning = false;
        return;
      }
      try {
        const results = await detector.detect(videoEl);
        if (results.length > 0) {
          const value = results[0].rawValue || '';
          const now = Date.now();
          if (value && (value !== lastScanValue || now - lastScanAt > 1500)) {
            lastScanValue = value;
            lastScanAt = now;
            await submitScan(value);
          }
        }
      } catch (error) {
        // Keep retrying; camera feeds often hiccup while focusing.
      }
      requestAnimationFrame(scanLoop);
    }

    startCamera();
  </script>
</body>
</html>)HTML";
  return out.str();
}

bool LocalHttpServer::serveConnection(SOCKET clientSocket, std::string requestText) {
  const auto headerEnd = requestText.find("\r\n\r\n");
  if (headerEnd == std::string::npos) {
    return false;
  }

  const auto headers = requestText.substr(0, headerEnd);
  const auto body = requestText.substr(headerEnd + 4);
  std::istringstream input(headers);
  std::string method;
  std::string target;
  std::string version;
  input >> method >> target >> version;
  (void)version;

  if (method == "GET" && (target == "/" || target == "/index.html")) {
    const auto response = responseText("200 OK", "text/html; charset=utf-8", scannerPage());
    send(clientSocket, response.c_str(), static_cast<int>(response.size()), 0);
    return true;
  }

  if (method == "GET" && target == "/api/state") {
    const auto response = responseText("200 OK", "application/json; charset=utf-8", jsonStatus());
    send(clientSocket, response.c_str(), static_cast<int>(response.size()), 0);
    return true;
  }

  if (method == "POST" && target == "/api/scan") {
    const auto code = extractScanCode(body);
    {
      std::lock_guard<std::mutex> lock(stateMutex_);
      lastScan_ = code;
    }
    if (onScan_) {
      onScan_(code);
    }
    const auto response = responseText("200 OK", "application/json; charset=utf-8", scanCallbackMessage(code));
    send(clientSocket, response.c_str(), static_cast<int>(response.size()), 0);
    return true;
  }

  const auto response = responseText("404 Not Found", "text/plain; charset=utf-8", "Not found");
  send(clientSocket, response.c_str(), static_cast<int>(response.size()), 0);
  return false;
}

}  // namespace hims
