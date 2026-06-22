#pragma once

#include "HimsScanCore.h"
#include "gm65/gm65_scanner.h"
#include "keypad/keypad.h"
#include "net/hims_client.h"
#include "storage/outbox_store.h"

namespace hims_scan {

class HimsScanApp {
 public:
  bool begin();
  void loop();

 private:
  enum class State {
    Idle,
    AwaitQuantity,
  };

  enum class PowerMode {
    Normal,
    Standby,
  };

  void handleScan(const String& code);
  void pollScanner();
  void handleKey(const HimsKeyEvent& event);
  void requestOtaUpdate();
  bool serviceOta();
  bool startOtaService();
  bool ensureWiFiForOta();
  void enterStandby();
  void exitStandby();
  void submitCurrent(char action);
  bool flushQueue();
  void reconnectWiFi();
  void primeHimsSoftwareConnection();
  void maybeSendStatus();
  void logState(const char* reason) const;
  void setAwaitingQuantity();
  void resetPending();

  Gm65Scanner scanner_;
  OutboxStore outbox_;
  HimsClient client_;
  HimsClientConfig clientConfig_;
  QuantityComposer quantity_;
  String scannedCode_;
  State state_ = State::Idle;
  PowerMode powerMode_ = PowerMode::Normal;
  bool wifiStarted_ = false;
  bool wifiConnectedReported_ = false;
  bool himsSoftwarePrimed_ = false;
  bool otaRequested_ = false;
  bool otaActive_ = false;
  unsigned long lastReconnectAttempt_ = 0;
  unsigned long lastFlushAttempt_ = 0;
  unsigned long lastStatusAttempt_ = 0;
};

}  // namespace hims_scan
