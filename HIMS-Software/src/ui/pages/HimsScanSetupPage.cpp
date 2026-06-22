// HIMS - Hardware Inventory Management System
// Simple pairing and status page for one HIMS Scan R1 device.

#include "App.h"

#include "ui/shared/AppUiShared.h"

#include <algorithm>
#include <string>

namespace hims {

using namespace std;

bool App::regenerateHimsScanToken() {
  himsScanConfig_.token = generateHimsScanToken();
  himsScanConfig_.deviceId.clear();
  deviceLastSeen_ = 0;
  deviceFirmwareVersion_.clear();
  deviceRssi_ = 0;
  deviceLastResult_.clear();
  deviceRequestCache_.clear();
  deviceRequestOrder_.clear();
  server_.setDeviceCredentials(himsScanConfig_.deviceId, himsScanConfig_.token);
  if (!saveHimsScanConfig(himsScanConfigPath_, himsScanConfig_)) {
    setMessage("Generated a new token, but HIMS could not save it", 4);
    return false;
  }
  setMessage("Generated a new pairing token", 3);
  dirty_ = true;
  return true;
}

bool App::clearHimsScanPairing() {
  if (trim(himsScanConfig_.deviceId).empty()) {
    setMessage("No paired device to clear", 2);
    return false;
  }

  himsScanConfig_.deviceId.clear();
  deviceLastSeen_ = 0;
  deviceFirmwareVersion_.clear();
  deviceRssi_ = 0;
  deviceLastResult_.clear();
  server_.setDeviceCredentials(himsScanConfig_.deviceId, himsScanConfig_.token);
  if (!saveHimsScanConfig(himsScanConfigPath_, himsScanConfig_)) {
    setMessage("Cleared pairing in memory, but HIMS could not save it", 4);
    return false;
  }
  setMessage("Cleared paired device identity", 3);
  dirty_ = true;
  return true;
}

void App::openHimsScanSetup() {
  page_ = Page::HimsScanSetup;
  inputMode_ = InputMode::None;
  setMessage("Pair the device with the token shown here", 4);
}

ftxui::Element App::renderHimsScanSetupUi() const {
  const auto bridgeUrl = server_.running() ? server_.baseUrl() : string("scanner bridge offline");
  const auto deviceState = trim(himsScanConfig_.deviceId).empty() ? string("Waiting for first valid device report")
                                                                  : string("Paired device: ") + himsScanConfig_.deviceId;
  const auto lastSeen = deviceLastSeen_ == 0 ? string("No heartbeat yet") : string("Last heartbeat: ") +
                                                                         nowTimestampString(deviceLastSeen_);
  const auto statusLine = himsScanDeviceSummary();

  ftxui::Elements leftRows;
  leftRows.push_back(fullLine("Pairing flow", uiAccentColor(), uiPanelLeftBg()));
  leftRows.push_back(fullLine("1. Open the device portal on the ESP32.", uiTitleColor(), uiPanelLeftBg()));
  leftRows.push_back(fullLine("2. Join Wi-Fi and enter the HIMS bridge URL.", uiTitleColor(), uiPanelLeftBg()));
  leftRows.push_back(fullLine("3. Paste the pairing token shown on this page.", uiTitleColor(), uiPanelLeftBg()));
  leftRows.push_back(fullLine("4. Scan a HIMS ID, enter a quantity, then press A or B.", uiTitleColor(),
                              uiPanelLeftBg()));
  leftRows.push_back(ftxui::separator());
  leftRows.push_back(fullLine("Current bridge", uiAccentColor(), uiPanelLeftBg()));
  leftRows.push_back(fullLine(bridgeUrl, server_.running() ? uiLinkColor() : uiWarnColor(), uiPanelLeftBg()));
  leftRows.push_back(fullLine("Pairing token", uiAccentColor(), uiPanelLeftBg()));
  leftRows.push_back(fullLine(himsScanConfig_.token.empty() ? string("n/a") : himsScanConfig_.token,
                              uiTitleColor(), uiPanelLeftBg()));

  ftxui::Elements rightRows;
  rightRows.push_back(fullLine("Device status", uiAccentColor(), uiPanelRightBg()));
  rightRows.push_back(fullLine(statusLine, uiTitleColor(), uiPanelRightBg()));
  rightRows.push_back(fullLine(deviceState, uiMutedColor(), uiPanelRightBg()));
  rightRows.push_back(fullLine(lastSeen, uiMutedColor(), uiPanelRightBg()));
  rightRows.push_back(ftxui::separator());
  rightRows.push_back(fullLine("What the device sends", uiAccentColor(), uiPanelRightBg()));
  rightRows.push_back(fullLine("A scan code from the GM65 UART module", uiTitleColor(), uiPanelRightBg()));
  rightRows.push_back(fullLine("A quantity built from keypad digits", uiTitleColor(), uiPanelRightBg()));
  rightRows.push_back(fullLine("A or B to add or subtract the quantity", uiTitleColor(), uiPanelRightBg()));
  rightRows.push_back(ftxui::separator());
  rightRows.push_back(fullLine("Actions", uiAccentColor(), uiPanelRightBg()));
  rightRows.push_back(fullLine("'r' regenerate token", uiTitleColor(), uiPanelRightBg()));
  rightRows.push_back(fullLine("'c' clear paired device", uiTitleColor(), uiPanelRightBg()));
  rightRows.push_back(fullLine("'o' open mobile scanner page", uiTitleColor(), uiPanelRightBg()));
  rightRows.push_back(fullLine("'Esc' return to dashboard", uiTitleColor(), uiPanelRightBg()));

  auto left = ftxui::vbox(move(leftRows)) | ftxui::bgcolor(uiPanelLeftBg()) | ftxui::flex;
  auto right = ftxui::vbox(move(rightRows)) | ftxui::bgcolor(uiPanelRightBg()) | ftxui::flex;
  return ftxui::hbox({move(left), ftxui::separator(), move(right)});
}

void App::handleHimsScanSetupKey(const KeyEvent& key) {
  if (key.type == KeyType::Escape) {
    changePage(Page::Dashboard);
    return;
  }

  if (key.type == KeyType::Character) {
    const auto ch = static_cast<char>(tolower(static_cast<unsigned char>(key.ch)));
    if (ch == 'r') {
      regenerateHimsScanToken();
      return;
    }
    if (ch == 'c') {
      clearHimsScanPairing();
      return;
    }
    if (ch == 'o') {
      openUrl(scannerUrl());
      setMessage("Opened the mobile scanner page", 3);
      return;
    }
    if (ch == 'q') {
      running_ = false;
      return;
    }
  }

  if (key.type == KeyType::Enter) {
    openUrl(scannerUrl());
    setMessage("Opened the mobile scanner page", 3);
  }
}

}  // namespace hims
