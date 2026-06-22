#include "app/hims_scan_app.h"

#include <ArduinoOTA.h>

#include "HimsScanCore.h"
#include "config/config.h"

#include <WiFi.h>

namespace hims_scan {

bool HimsScanApp::begin() {
  Serial.println("HIMS Scan R1 booting");
  keypadInit();
  scanner_.begin(9600);
  scanner_.flushInput();
  powerMode_ = PowerMode::Normal;
  otaRequested_ = false;
  otaActive_ = false;

  clientConfig_.deviceId = DEVICE_ID;
  clientConfig_.token = DEVICE_TOKEN;
  clientConfig_.fallbackHost = FALLBACK_HOST;
  clientConfig_.fallbackPort = FALLBACK_PORT;

  outbox_.begin("hims_scan", DEVICE_ID);
  outbox_.load();

  client_.begin(clientConfig_);
  wifiStarted_ = true;

  if (!WIFI_AUTOSTART) {
    Serial.println("Wi-Fi auto-start disabled; running offline for keypad/scanner testing.");
  } else if (trimCopy(WIFI_SSID).length() == 0) {
    Serial.println("Wi-Fi credentials are empty; staying offline.");
  } else {
    Serial.print("Connecting to local Wi-Fi SSID: ");
    Serial.println(WIFI_SSID);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  }

  logState("boot");
  return true;
}

void HimsScanApp::loop() {
  HimsKeyEvent event;
  while (keypadPoll(event)) {
    handleKey(event);
    if (otaRequested_) {
      break;
    }
  }

  if (powerMode_ == PowerMode::Standby) {
    if (serviceOta()) {
      delay(2);
      return;
    }
    delay(20);
    return;
  }

  // Drain UART before any Wi-Fi operation that may briefly block.
  pollScanner();

  if (serviceOta()) {
    delay(2);
    return;
  }

  reconnectWiFi();
  primeHimsSoftwareConnection();

  const bool quantitySent = flushQueue();
  if (!quantitySent) {
    maybeSendStatus();
  }
  // Drain anything received while the HTTP client was active.
  pollScanner();
  delay(2);
}

void HimsScanApp::pollScanner() {
  String code;
  if (scanner_.poll(code)) {
    handleScan(code);
  }
}

void HimsScanApp::handleScan(const String& code) {
  // Some GM65 profiles prepend an AIM symbology identifier (for example ]C1
  // for Code 128, ]Q3 for QR, or ]d2 for Data Matrix). Normalize that away
  // before handing the code to the rest of the app.
  const auto trimmed = trimCopy(stripAimSymbologyPrefix(code));
  if (trimmed.length() == 0) {
    return;
  }

  scannedCode_ = trimmed;
  quantity_.clear();
  setAwaitingQuantity();
  Serial.print("GM65 scan: ");
  Serial.println(scannedCode_.c_str());
  Serial.println("Enter quantity, then press A to add or B to subtract. C cancels.");
}

void HimsScanApp::handleKey(const HimsKeyEvent& event) {
  if (event.type == HimsKeyEventType::OtaUpdate) {
    requestOtaUpdate();
    return;
  }

  if (event.type == HimsKeyEventType::PowerToggle) {
    if (powerMode_ == PowerMode::Standby) {
      exitStandby();
    } else {
      enterStandby();
    }
    return;
  }

  if (event.type == HimsKeyEventType::Digit) {
    if (state_ == State::Idle) {
      Serial.println("Digit ignored, scan a code first.");
      return;
    }
    quantity_.appendDigit(event.value);
    Serial.print("Quantity: ");
    Serial.println(quantity_.displayText().c_str());
    return;
  }

  if (event.type == HimsKeyEventType::Cancel) {
    resetPending();
    Serial.println("Cancelled pending scan.");
    return;
  }

  if (event.type == HimsKeyEventType::Add || event.type == HimsKeyEventType::Subtract) {
    if (state_ == State::Idle || scannedCode_.length() == 0) {
      Serial.println("No scanned code to submit.");
      return;
    }
    submitCurrent(event.type == HimsKeyEventType::Add ? 'A' : 'B');
  }
}

void HimsScanApp::requestOtaUpdate() {
  if (otaRequested_) {
    Serial.println("OTA update already requested.");
    return;
  }

  resetPending();
  otaRequested_ = true;
  otaActive_ = false;
  client_.end();
  scanner_.flushInput();
  if (powerMode_ == PowerMode::Standby) {
    Serial.println("Waking standby path for OTA update.");
  }
  Serial.println("OTA update requested. Normal scanning is paused until the update finishes.");
}

bool HimsScanApp::serviceOta() {
  if (!otaRequested_) {
    return false;
  }

  if (trimCopy(WIFI_SSID).length() == 0) {
    Serial.println("OTA request ignored because Wi-Fi credentials are empty.");
    otaRequested_ = false;
    otaActive_ = false;
    return false;
  }

  if (!ensureWiFiForOta()) {
    return true;
  }

  if (!otaActive_) {
    if (!startOtaService()) {
      Serial.println("OTA service failed to start; resuming normal operation.");
      otaRequested_ = false;
      otaActive_ = false;
      return false;
    }
  }

  ArduinoOTA.handle();
  return true;
}

bool HimsScanApp::ensureWiFiForOta() {
  if (WiFi.isConnected()) {
    return true;
  }

  if (trimCopy(WIFI_SSID).length() == 0) {
    Serial.println("OTA request ignored because Wi-Fi credentials are empty.");
    otaRequested_ = false;
    otaActive_ = false;
    return false;
  }

  if (WiFi.getMode() != WIFI_STA) {
    WiFi.mode(WIFI_STA);
    delay(50);
  }

  if (millis() - lastReconnectAttempt_ < 2000UL) {
    return false;
  }
  lastReconnectAttempt_ = millis();

  Serial.print("Connecting for OTA to local Wi-Fi SSID: ");
  Serial.println(WIFI_SSID);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  return false;
}

bool HimsScanApp::startOtaService() {
  WiFi.setSleep(false);
  ArduinoOTA.setHostname(clientConfig_.deviceId.c_str());
  ArduinoOTA.setPassword(OTA_PASSWORD);
  ArduinoOTA.onStart([this]() {
    otaActive_ = true;
    Serial.println("OTA start: flashing new firmware.");
  });
  ArduinoOTA.onEnd([this]() {
    Serial.println();
    Serial.println("OTA complete; restarting device.");
    otaRequested_ = false;
    otaActive_ = false;
    delay(250);
    ESP.restart();
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    if (total == 0U) {
      return;
    }
    const unsigned int percent = (progress * 100U) / total;
    Serial.printf("OTA progress: %u%%\r", percent);
  });
  ArduinoOTA.onError([](ota_error_t error) {
    Serial.print("OTA error: ");
    switch (error) {
      case OTA_AUTH_ERROR:
        Serial.println("authentication failed");
        break;
      case OTA_BEGIN_ERROR:
        Serial.println("begin failed");
        break;
      case OTA_CONNECT_ERROR:
        Serial.println("connection failed");
        break;
      case OTA_RECEIVE_ERROR:
        Serial.println("receive failed");
        break;
      case OTA_END_ERROR:
        Serial.println("end failed");
        break;
      default:
        Serial.println("unknown");
        break;
    }
  });

  ArduinoOTA.begin();
  otaActive_ = true;
  Serial.print("OTA update ready for ");
  Serial.print(clientConfig_.deviceId.c_str());
  Serial.print(" at ");
  Serial.println(WiFi.localIP());
  return true;
}

void HimsScanApp::enterStandby() {
  if (powerMode_ == PowerMode::Standby) {
    return;
  }

  Serial.println("Entering standby mode.");
  resetPending();
  otaRequested_ = false;
  otaActive_ = false;
  lastReconnectAttempt_ = 0;
  lastFlushAttempt_ = 0;
  lastStatusAttempt_ = 0;
  wifiConnectedReported_ = false;
  himsSoftwarePrimed_ = false;

  client_.end();
  WiFi.disconnect(true);
  WiFi.mode(WIFI_OFF);
  scanner_.suspend();
  powerMode_ = PowerMode::Standby;
  Serial.println("Standby mode active. Hold D again to wake.");
}

void HimsScanApp::exitStandby() {
  if (powerMode_ != PowerMode::Standby) {
    return;
  }

  Serial.println("Waking from standby mode.");
  scanner_.resume();
  client_.begin(clientConfig_);
  wifiStarted_ = true;
  wifiConnectedReported_ = false;
  himsSoftwarePrimed_ = false;
  lastReconnectAttempt_ = 0;
  lastFlushAttempt_ = 0;
  lastStatusAttempt_ = 0;
  powerMode_ = PowerMode::Normal;

  if (WIFI_AUTOSTART && trimCopy(WIFI_SSID).length() > 0) {
    Serial.print("Reconnecting after standby to local Wi-Fi SSID: ");
    Serial.println(WIFI_SSID);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  } else {
    Serial.println("Standby exit complete; Wi-Fi remains offline.");
  }
}

void HimsScanApp::submitCurrent(char action) {
  const auto delta = quantity_.consume(action == 'A');
  QuantityRequest request;
  request.deviceId = clientConfig_.deviceId;
  request.sequence = outbox_.nextSequence();
  request.requestId = makeRequestId(clientConfig_.deviceId, request.sequence);
  request.code = scannedCode_;
  request.delta = delta;

  outbox_.setNextSequence(request.sequence + 1U);
  if (!outbox_.enqueue(request)) {
    Serial.println("Failed to queue request.");
    return;
  }

  Serial.print("Queued request: ");
  Serial.println(buildQuantityRequestJson(request.deviceId, request.requestId, request.code, request.delta).c_str());
  resetPending();
}

bool HimsScanApp::flushQueue() {
  if (millis() - lastFlushAttempt_ < FLUSH_INTERVAL_MS) {
    return false;
  }
  lastFlushAttempt_ = millis();

  if (!WiFi.isConnected()) {
    return false;
  }

  if (!client_.resolveEndpoint()) {
    return false;
  }

  QuantityRequest request;
  if (!outbox_.peek(request)) {
    return false;
  }

  int status = 0;
  String body;
  const auto ok = client_.postQuantity(request, status, body);
  Serial.print("POST /api/device/quantity -> ");
  Serial.println(ok ? "sent" : "failed");
  if (!ok || status < 200 || status >= 300) {
    Serial.print("Response body: ");
    Serial.println(body.c_str());
    return false;
  }

  Serial.print("Applied quantity delta for ");
  Serial.print(request.code.c_str());
  Serial.print(" status ");
  Serial.println(status);
  outbox_.pop(request);
  return true;
}

void HimsScanApp::reconnectWiFi() {
  if (!wifiStarted_) {
    return;
  }

  if (powerMode_ == PowerMode::Standby) {
    return;
  }

  if (!WIFI_AUTOSTART && !otaRequested_ && !otaActive_) {
    return;
  }

  if (WiFi.status() == WL_CONNECTED) {
    if (!wifiConnectedReported_) {
      wifiConnectedReported_ = true;
      himsSoftwarePrimed_ = false;
      lastStatusAttempt_ = 0;
      lastFlushAttempt_ = 0;
      Serial.print("Local Wi-Fi connected: ");
      Serial.print(WiFi.SSID());
      Serial.print(" @ ");
      Serial.println(WiFi.localIP());
    }
    return;
  }

  wifiConnectedReported_ = false;
  himsSoftwarePrimed_ = false;

  if (millis() - lastReconnectAttempt_ < WIFI_RECONNECT_INTERVAL_MS) {
    return;
  }
  lastReconnectAttempt_ = millis();

  if (trimCopy(WIFI_SSID).length() == 0) {
    Serial.println("Wi-Fi credentials are empty; staying offline.");
    return;
  }

  Serial.print("Reconnecting to local Wi-Fi SSID: ");
  Serial.println(WIFI_SSID);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
}

void HimsScanApp::primeHimsSoftwareConnection() {
  if (!wifiConnectedReported_ || himsSoftwarePrimed_) {
    return;
  }

  himsSoftwarePrimed_ = true;
  Serial.println("Local Wi-Fi ready; discovering HIMS software endpoint...");
  if (client_.resolveEndpoint(true)) {
    Serial.print("HIMS software endpoint: ");
    Serial.println(client_.endpointSummary().c_str());
  }
}

void HimsScanApp::maybeSendStatus() {
  if (!outbox_.empty()) {
    // Prioritize queued quantity updates over a heartbeat so the device keeps
    // making forward progress when Wi-Fi is slow or the PC is busy.
    return;
  }
  if (!WiFi.isConnected()) {
    return;
  }
  if (millis() - lastStatusAttempt_ < STATUS_INTERVAL_MS) {
    return;
  }
  lastStatusAttempt_ = millis();
  const auto ok = client_.postStatus(FIRMWARE_VERSION, WiFi.RSSI());
  Serial.print("Status report: ");
  Serial.println(ok ? "ok" : "failed");
}

void HimsScanApp::logState(const char* reason) const {
  Serial.print("State update [");
  Serial.print(reason);
  Serial.print("]: ");
  Serial.println(state_ == State::Idle ? "idle" : "awaiting quantity");
}

void HimsScanApp::setAwaitingQuantity() {
  state_ = State::AwaitQuantity;
}

void HimsScanApp::resetPending() {
  scannedCode_.clear();
  quantity_.clear();
  state_ = State::Idle;
}

}  // namespace hims_scan
