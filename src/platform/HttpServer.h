#pragma once

#include <atomic>
#include <cstdint>
#include <functional>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <winsock2.h>

#include "core/Inventory.h"

namespace hims {

class LocalHttpServer {
 public:
  using ScanCallback = std::function<void(const std::string&)>;

  LocalHttpServer() = default;
  ~LocalHttpServer();

  LocalHttpServer(const LocalHttpServer&) = delete;
  LocalHttpServer& operator=(const LocalHttpServer&) = delete;

  bool start(std::uint16_t preferredPort, ScanCallback onScan);
  void stop();
  void setRecentActivity(std::vector<ActivityEntry> activities);

  bool running() const;
  std::uint16_t port() const;
  std::string baseUrl() const;
  std::vector<std::string> addresses() const;
  std::string lastScan() const;

 private:
  void workerLoop();
  bool serveConnection(SOCKET clientSocket, std::string requestText);
  std::string scannerPage() const;
  std::string jsonStatus() const;
  std::string responseText(const std::string& status, const std::string& contentType, const std::string& body) const;
  bool bindSocket(std::uint16_t port);
  std::string scanCallbackMessage(const std::string& code) const;

  std::atomic<bool> running_{false};
  bool winsockStarted_ = false;
  std::thread worker_;
  ScanCallback onScan_;
  mutable std::mutex stateMutex_;
  std::uint16_t port_ = 0;
  std::string lastScan_;
  std::string lastError_;
  std::vector<std::string> addresses_;
  std::vector<ActivityEntry> recentActivities_;
  SOCKET listenSocket_ = INVALID_SOCKET;
};

}  // namespace hims
