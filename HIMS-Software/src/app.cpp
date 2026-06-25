// HIMS - Hardware Inventory Management System
// Terminal application controller and app-level state management.

#include "App.h"

#include "platform/DigiKeyApi.h"
#include "ui/shared/AppUiShared.h"

#include <ftxui/component/component.hpp>
#include <ftxui/component/screen_interactive.hpp>
#include <ftxui/dom/elements.hpp>

#include <algorithm>
#include <chrono>
#include <cctype>
#include <cstdlib>
#include <initializer_list>
#include <iomanip>
#include <iostream>
#include <limits>
#include <optional>
#include <regex>
#include <sstream>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <thread>

namespace hims {

using namespace std;


KeyEvent translateEvent(const ftxui::Event& event) {
  if (event == ftxui::Event::Return) {
    return {KeyType::Enter, '\0'};
  }
  if (event == ftxui::Event::Escape) {
    return {KeyType::Escape, '\0'};
  }
  if (event == ftxui::Event::CtrlH) {
    return {KeyType::CtrlBackspace, '\0'};
  }
  if (event == ftxui::Event::CtrlZ) {
    return {KeyType::CtrlZ, '\0'};
  }
  if (event == ftxui::Event::Backspace) {
    if (controlModifierPressed()) {
      return {KeyType::CtrlBackspace, '\0'};
    }
    return {KeyType::Backspace, '\0'};
  }
  if (event == ftxui::Event::Tab) {
    return {KeyType::Tab, '\0'};
  }
  if (event == ftxui::Event::ArrowUp) {
    return {KeyType::Up, '\0'};
  }
  if (event == ftxui::Event::ArrowDown) {
    return {KeyType::Down, '\0'};
  }
  if (event == ftxui::Event::ArrowLeft) {
    return {KeyType::Left, '\0'};
  }
  if (event == ftxui::Event::ArrowRight) {
    return {KeyType::Right, '\0'};
  }
  if (event == ftxui::Event::Home) {
    return {KeyType::Home, '\0'};
  }
  if (event == ftxui::Event::End) {
    return {KeyType::End, '\0'};
  }
  if (event == ftxui::Event::PageUp) {
    return {KeyType::PageUp, '\0'};
  }
  if (event == ftxui::Event::PageDown) {
    return {KeyType::PageDown, '\0'};
  }
  if (event == ftxui::Event::Delete) {
    return {KeyType::Delete, '\0'};
  }
  if (event.is_character() && !event.character().empty()) {
    return {KeyType::Character, event.character()[0]};
  }
  return {KeyType::Unknown, '\0'};
}

App::App()
    : root_(filesystem::current_path()),
      dataPath_(discoverHimsDataPath()),
      inventoryPath_(dataPath_ / "inventory.db"),
      printerPath_(dataPath_ / "printer.conf"),
      activityPath_(dataPath_ / "activity.tsv"),
      himsScanConfigPath_(dataPath_ / "hims_scan.conf") {
  loadEnvironmentFile(locateDotEnvFile());
  ensureInventoryDatabaseCopied(inventoryPath_);
  loadHimsScanConfig(himsScanConfigPath_, himsScanConfig_);
  loadState();
  server_.setDeviceCredentials(himsScanConfig_.deviceId, himsScanConfig_.token);

  if (!server_.start(8080, filesystem::current_path() / "scanner.html",
                     [this](const DeviceScanRequest& request) { pushScanCode(request); },
                     [this](const DeviceQuantityRequest& request) { return enqueueDeviceQuantity(request); },
                     [this](const DeviceStatusReport& report) { enqueueDeviceStatus(report); },
                     [this](const DeviceDebugReport& report) { enqueueDeviceDebug(report); })) {
    setMessage("Scanner server failed to start; terminal still works", 5);
  } else {
    mdnsService_.start(server_.port());
    setMessage("Scanner companion page ready", 5);
  }
}

ftxui::Element App::renderUi() const {
  const auto now = time(nullptr);
  const bool scannerFlashing = now <= scannerFlashUntil_;
  const bool printerFlashing = now <= printerFlashUntil_;

  ftxui::Elements body;
  body.push_back(ftxui::hbox({
                     styledText("HIMS Terminal v" + softwareVersion() + "  " + summaryLine(), uiTitleColor(),
                                uiPanelLeftBg()) |
                         ftxui::bold,
                     ftxui::filler(),
                     statusCueChip("HIMS Scan", server_.running(), scannerFlashing,
                                   server_.running() ? uiTitleColor() : uiWarnColor(), ftxui::Color::RGB(28, 36, 44),
                                   ftxui::Color::RGB(28, 66, 40), ftxui::Color::RGB(44, 26, 20)),
                     ftxui::separator(),
                     styledText(ellipsize(himsScanDeviceSummary(), 48),
                                deviceLastSeen_ > 0 && time(nullptr) - deviceLastSeen_ <= 15 ? uiSuccessColor()
                                                                                           : uiMutedColor(),
                                ftxui::Color::RGB(28, 36, 44)) | ftxui::bold,
                     ftxui::separator(),
                     statusCueChip("Printer", printerService_.hasConfiguredPrinter(), printerFlashing,
                                   printerCheck_.ok ? uiSuccessColor()
                                                    : (printerService_.hasConfiguredPrinter() ? uiAccentColor()
                                                                                              : uiWarnColor()),
                                   ftxui::Color::RGB(82, 50, 18), ftxui::Color::RGB(28, 78, 42),
                                   ftxui::Color::RGB(44, 26, 20)),
                     ftxui::separator(),
                     statusCueChip(autoPrintScannedLabels_ ? "Auto Label ON" : "Auto Label OFF",
                                   autoPrintScannedLabels_, false,
                                   autoPrintScannedLabels_ ? uiSuccessColor() : uiWarnColor(),
                                   ftxui::Color::RGB(26, 58, 34), ftxui::Color::RGB(28, 78, 42),
                                   ftxui::Color::RGB(58, 36, 20)),
                 }) |
                 ftxui::bgcolor(uiPanelLeftBg()));
  body.push_back(ftxui::separator());
  body.push_back(renderPageUi() | ftxui::flex);
  body.push_back(ftxui::separator());
  body.push_back(renderSearchBarUi());
  body.push_back(renderMessageUi());
  body.push_back(renderStatusBarUi());
  return ftxui::vbox(move(body));
}

ftxui::Element App::renderPageUi() const {
  switch (page_) {
    case Page::Dashboard:
      return renderDashboardUi();
    case Page::Stock:
      return renderStockUi();
    case Page::Detail:
      return renderDetailUi();
    case Page::RackManagement:
      return renderRackManagementUi();
    case Page::PrinterSetup:
      return renderPrinterSetupUi();
    case Page::HimsScanSetup:
      return renderHimsScanSetupUi();
    case Page::ImportCsv:
      return renderImportCsvUi();
  }

  return ftxui::text("");
}

ftxui::Element App::renderSearchBarUi() const {
  const auto activeBg = inputMode_ == InputMode::Search || inputMode_ == InputMode::EditValue
                            || inputMode_ == InputMode::RackRename || inputMode_ == InputMode::RackType
                            || inputMode_ == InputMode::RackCreate || inputMode_ == InputMode::RackJump
                            || inputMode_ == InputMode::RackFilter
                            ? uiRowSelectedBg()
                            : uiPanelLeftBg();
  const auto bodyColor = inputMode_ == InputMode::Search ? uiTitleColor()
                          : inputMode_ == InputMode::EditValue || inputMode_ == InputMode::RackRename ||
                                    inputMode_ == InputMode::RackType || inputMode_ == InputMode::RackCreate ||
                                    inputMode_ == InputMode::RackJump || inputMode_ == InputMode::RackFilter
                                ? uiLinkColor()
                                                               : uiMutedColor();
  const auto bodyText = inputMode_ == InputMode::Search ? "/" + inputBuffer_ + "_"
                        : inputMode_ == InputMode::EditValue || inputMode_ == InputMode::RackRename ||
                                  inputMode_ == InputMode::RackType || inputMode_ == InputMode::RackCreate ||
                                  inputMode_ == InputMode::RackJump || inputMode_ == InputMode::RackFilter
                              ? activePrompt() + inputBuffer_ + "_"
                                                             : "/" + searchQuery_;

  ftxui::Elements rows;
  rows.push_back(footerField("Search", bodyText, uiAccentColor(), bodyColor, activeBg));

  if (inputMode_ == InputMode::EditFieldMenu) {
    ftxui::Elements options;
    options.push_back(footerField("Edit fields", "", uiAccentColor(), uiMutedColor(), uiPanelLeftBg()));
    for (size_t index = 0; index < menuOptions_.size(); ++index) {
      const auto bg = static_cast<int>(index) == fieldMenuIndex_ ? uiRowSelectedBg() : (index % 2 == 0 ? uiRowDarkBg() : uiRowLightBg());
      auto option = fullLine("  " + menuOptions_[index].label,
                             static_cast<int>(index) == fieldMenuIndex_ ? uiTitleColor() : uiMutedColor(), bg);
      if (static_cast<int>(index) == fieldMenuIndex_) {
        option = option | ftxui::select;
      }
      options.push_back(option);
    }
    rows.push_back(ftxui::vbox(move(options)));
  }

  return ftxui::vbox(move(rows));
}

ftxui::Element App::renderStatusBarUi() const {
  const auto summary = shortcutSummary();

  vector<string> segments;
  size_t start = 0;
  while (start <= summary.size()) {
    const auto separator = summary.find(" | ", start);
    const auto chunk = trim(summary.substr(start, separator == string::npos ? string::npos : separator - start));
    if (!chunk.empty()) {
      segments.push_back(chunk);
    }
    if (separator == string::npos) {
      break;
    }
    start = separator + 3;
  }

  ftxui::Elements actions;
  for (size_t index = 0; index < segments.size(); ++index) {
    if (index > 0) {
      actions.push_back(ftxui::text(" | "));
    }
    actions.push_back(ftxui::text(segments[index]));
  }

  return ftxui::hbox({
             ftxui::filler(),
             ftxui::hbox(move(actions)),
             ftxui::filler(),
         }) |
         ftxui::bgcolor(uiPanelRightBg());
}

ftxui::Element App::renderMessageUi() const {
  if (message_.empty()) {
    return ftxui::text("");
  }
  return fullLine(message_, uiAccentColor(), uiPanelLeftBg());
}

int App::run() {
  running_ = true;
  auto screen = ftxui::ScreenInteractive::Fullscreen();
  screen.ForceHandleCtrlZ(false);
  screen.TrackMouse();
  auto renderer = ftxui::Renderer([this] { return renderUi(); });
  auto component = ftxui::CatchEvent(renderer, [this, &screen](ftxui::Event event) {
    if (event == ftxui::Event::Custom) {
      processScans();
      processDeviceRequests();
      clearMessageIfExpired();
      clearDeleteConfirmationIfExpired();
      return true;
    }

    if (event.is_mouse()) {
      if (page_ == Page::Stock || page_ == Page::ImportCsv) {
        const auto& mouse = event.mouse();
        if (mouse.button == ftxui::Mouse::WheelUp) {
          page_ == Page::ImportCsv ? moveImportSelection(-1) : moveSelection(-1);
          return true;
        }
        if (mouse.button == ftxui::Mouse::WheelDown) {
          page_ == Page::ImportCsv ? moveImportSelection(1) : moveSelection(1);
          return true;
        }
      }
      return false;
    }

    const auto key = translateEvent(event);
    if (key.type == KeyType::Unknown && event != ftxui::Event::Escape) {
      return false;
    }

    handleKey(key);
    if (!running_) {
      screen.ExitLoopClosure()();
    }
    return true;
  });

  thread ticker([this, &screen] {
    while (running_) {
      screen.PostEvent(ftxui::Event::Custom);
      this_thread::sleep_for(chrono::milliseconds(100));
    }
  });

  screen.Loop(component);
  running_ = false;
  if (ticker.joinable()) {
    ticker.join();
  }
  saveState();
  mdnsService_.stop();
  server_.stop();
  return 0;
}

void App::processInput() {
  for (const auto& key : pollKeys()) {
    handleKey(key);
  }
}

void App::handleKey(const KeyEvent& key) {
  switch (inputMode_) {
    case InputMode::Search:
      handleSearchKey(key);
      return;
    case InputMode::EditFieldMenu:
      handleEditMenuKey(key);
      return;
    case InputMode::EditValue:
      handleEditValueKey(key);
      return;
    case InputMode::RackRename:
    case InputMode::RackType:
    case InputMode::RackCreate:
    case InputMode::RackJump:
    case InputMode::RackFilter:
      handleRackValueKey(key);
      return;
    case InputMode::None:
      break;
  }

  if (key.type == KeyType::Character && key.ch == 'P' && page_ != Page::RackManagement) {
    toggleAutoPrintScannedLabels();
    return;
  }

  switch (page_) {
    case Page::Dashboard:
      handleDashboardKey(key);
      break;
    case Page::Stock:
      handleStockKey(key);
      break;
    case Page::Detail:
      handleDetailKey(key);
      break;
    case Page::RackManagement:
      handleRackManagementKey(key);
      break;
    case Page::PrinterSetup:
      handlePrinterSetupKey(key);
      break;
    case Page::HimsScanSetup:
      handleHimsScanSetupKey(key);
      break;
    case Page::ImportCsv:
      handleImportCsvKey(key);
      break;
  }
}

void App::handleSearchKey(const KeyEvent& key) {
  if (key.type == KeyType::Character) {
    inputBuffer_.push_back(key.ch);
    dirty_ = true;
    return;
  }

  if (key.type == KeyType::Backspace) {
    if (!inputBuffer_.empty()) {
      inputBuffer_.pop_back();
      dirty_ = true;
    }
    return;
  }

  if (key.type == KeyType::Enter) {
    searchQuery_ = inputBuffer_;
    inputMode_ = InputMode::None;
    syncSelectionToFilter();
    setMessage(searchQuery_.empty() ? "Filter cleared" : "Filter applied", 2);
    return;
  }

  if (key.type == KeyType::Escape) {
    inputBuffer_ = searchQuery_;
    inputMode_ = InputMode::None;
    setMessage("Search cancelled", 2);
  }
}

void App::handleEditMenuKey(const KeyEvent& key) {
  if (key.type == KeyType::Character) {
    return;
  }

  if (key.type == KeyType::Up) {
    fieldMenuIndex_ = max(0, fieldMenuIndex_ - 1);
    dirty_ = true;
  } else if (key.type == KeyType::Down) {
    fieldMenuIndex_ = min(fieldMenuIndex_ + 1, static_cast<int>(menuOptions_.size()) - 1);
    dirty_ = true;
  } else if (key.type == KeyType::Enter) {
    if (fieldMenuIndex_ >= 0 && fieldMenuIndex_ < static_cast<int>(menuOptions_.size())) {
      if (fieldMenuIndex_ == static_cast<int>(menuOptions_.size()) - 2) {
        saveWorkingCopy();
        return;
      }
      if (fieldMenuIndex_ == static_cast<int>(menuOptions_.size()) - 1) {
        cancelInput();
        return;
      }
      inputBuffer_ = currentFieldValue(menuOptions_[fieldMenuIndex_].field);
      inputMode_ = InputMode::EditValue;
      setMessage("Editing " + fieldLabel(menuOptions_[fieldMenuIndex_].field), 2);
    }
  } else if (key.type == KeyType::Escape) {
    cancelInput();
  }
}

void App::handleEditValueKey(const KeyEvent& key) {
  if (key.type == KeyType::Character) {
    inputBuffer_.push_back(key.ch);
    dirty_ = true;
    return;
  }

  if (key.type == KeyType::Backspace) {
    if (!inputBuffer_.empty()) {
      inputBuffer_.pop_back();
      dirty_ = true;
    }
    return;
  }

  if (key.type == KeyType::Enter) {
    if (fieldMenuIndex_ >= 0 && fieldMenuIndex_ < static_cast<int>(menuOptions_.size())) {
      commitEditField(menuOptions_[fieldMenuIndex_].field, inputBuffer_);
    }
    inputBuffer_.clear();
    inputMode_ = InputMode::EditFieldMenu;
    return;
  }

  if (key.type == KeyType::Escape) {
    inputBuffer_.clear();
    inputMode_ = InputMode::EditFieldMenu;
    setMessage("Edit cancelled", 2);
  }
}

void App::handleRackValueKey(const KeyEvent& key) {
  if (key.type == KeyType::Character) {
    inputBuffer_.push_back(key.ch);
    dirty_ = true;
    return;
  }

  if (key.type == KeyType::Backspace) {
    if (!inputBuffer_.empty()) {
      inputBuffer_.pop_back();
      dirty_ = true;
    }
    return;
  }

  if (key.type == KeyType::Enter) {
    const auto value = inputBuffer_;
    const auto mode = inputMode_;
    inputBuffer_.clear();
    inputMode_ = InputMode::None;
    switch (mode) {
      case InputMode::RackRename:
        renameSelectedRack(value);
        break;
      case InputMode::RackType:
        changeSelectedRackType(value);
        break;
      case InputMode::RackCreate:
        createRackWithType(value);
        break;
      case InputMode::RackJump:
        jumpToRack(value);
        break;
      case InputMode::RackFilter:
        rackFilter_ = trim(value);
        syncRackSelection();
        setMessage(rackFilter_.empty() ? "Rack filter cleared" : "Rack filter applied", 2);
        break;
      default:
        break;
    }
    return;
  }

  if (key.type == KeyType::Escape) {
    inputBuffer_.clear();
    inputMode_ = InputMode::None;
    setMessage("Rack input cancelled", 2);
  }
}

}  // namespace hims


