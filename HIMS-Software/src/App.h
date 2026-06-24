// HIMS - Hardware Inventory Management System
// Terminal application controller and shared app state.

#pragma once

#include "core/Inventory.h"
#include "core/HimsScanProtocol.h"
#include "import/DigiKeyCsvImport.h"
#include "label_printer/LabelPrinter.h"
#include "platform/Console.h"
#include "platform/HttpServer.h"
#include "platform/MdnsService.h"

#include <ftxui/dom/elements.hpp>

#include <cstddef>
#include <condition_variable>
#include <deque>
#include <filesystem>
#include <memory>
#include <mutex>
#include <sstream>
#include <string>
#include <vector>
#include <unordered_map>

namespace hims {

std::filesystem::path documentsHimsPath();
std::filesystem::path discoverHimsDataPath();
std::filesystem::path legacyDatabasePath();
void copyDatabaseSidecar(const std::filesystem::path& sourceBase,
                         const std::filesystem::path& destinationBase, const std::string& suffix);
void ensureInventoryDatabaseCopied(const std::filesystem::path& localBase);
std::filesystem::path locateDotEnvFile();

class App {
 public:
  App();
  int run();

 private:
  enum class Page {
    Dashboard,
    Stock,
    Detail,
    RackManagement,
    PrinterSetup,
    HimsScanSetup,
    ImportCsv,
  };

  enum class InputMode {
    None,
    Search,
    EditFieldMenu,
    EditValue,
    RackRename,
    RackType,
    RackCreate,
    RackJump,
    RackFilter,
  };

  enum class EditField {
    PartName,
    Manufacturer,
    Category,
    Quantity,
    ReorderThreshold,
    Location,
    Tags,
    Parameters,
    Notes,
    DigiKeyPart,
    DatasheetUrl,
    ProductUrl,
    Sku,
    SyncStatus,
    RackLocation,
  };

  struct FieldOption {
    std::string label;
    EditField field;
  };

  struct WorkingCopy {
    InventoryItem item;
    bool isNew = false;
    size_t originalIndex = 0;
  };

  struct UndoSnapshot {
    std::vector<InventoryItem> items;
    std::vector<HimsRack> racks;
    std::vector<ActivityEntry> activities;
    size_t selectedPosition = 0;
    bool valid = false;
  };

  struct PendingDeviceQuantity {
    DeviceQuantityRequest request;
    DeviceQuantityResult result;
    std::mutex mutex;
    std::condition_variable ready;
    bool complete = false;
  };

  void loadState();
  void saveState();
  void processInput();
  void handleKey(const KeyEvent& key);
  void handleDashboardKey(const KeyEvent& key);
  void handleStockKey(const KeyEvent& key);
  void handleDetailKey(const KeyEvent& key);
  void handleRackManagementKey(const KeyEvent& key);
  void handlePrinterSetupKey(const KeyEvent& key);
  void handleHimsScanSetupKey(const KeyEvent& key);
  void handleImportCsvKey(const KeyEvent& key);
  void handleSearchKey(const KeyEvent& key);
  void handleEditMenuKey(const KeyEvent& key);
  void handleEditValueKey(const KeyEvent& key);
  void handleRackValueKey(const KeyEvent& key);

  void render();
  void renderDashboard(std::ostringstream& out, const ConsoleSize& size);
  void renderStock(std::ostringstream& out, const ConsoleSize& size);
  void renderDetail(std::ostringstream& out, const ConsoleSize& size);
  void renderImportCsv(std::ostringstream& out, const ConsoleSize& size);
  void renderSearchBar(std::ostringstream& out, const ConsoleSize& size);
  void renderStatusBar(std::ostringstream& out, const ConsoleSize& size);
  void renderMessage(std::ostringstream& out, const ConsoleSize& size);

  ftxui::Element renderUi() const;
  ftxui::Element renderDashboardUi() const;
  ftxui::Element renderStockUi() const;
  ftxui::Element renderDetailUi() const;
  ftxui::Element renderRackManagementUi() const;
  ftxui::Element renderPrinterSetupUi() const;
  ftxui::Element renderHimsScanSetupUi() const;
  ftxui::Element renderImportCsvUi() const;
  ftxui::Element renderSearchBarUi() const;
  ftxui::Element renderStatusBarUi() const;
  ftxui::Element renderMessageUi() const;
  ftxui::Element renderPageUi() const;

  void setMessage(std::string text, int seconds = 3);
  bool messageVisible() const;
  void clearMessageIfExpired();
  void markDirty();
  void refreshPrinterState();
  void openPrinterSetup();
  bool printSelectedLabel();
  std::string printerSummary() const;
  void openHimsScanSetup();
  bool regenerateHimsScanToken();
  bool clearHimsScanPairing();
  DeviceQuantityResult enqueueDeviceQuantity(const DeviceQuantityRequest& request);
  void enqueueDeviceStatus(const DeviceStatusReport& report);
  void processDeviceRequests();
  std::string himsScanDeviceSummary() const;

  std::vector<size_t> filteredIndices() const;
  size_t selectedIndex() const;
  InventoryItem* selectedItem();
  const InventoryItem* selectedItem() const;
  PrinterQueueInfo* selectedPrinterQueue();
  const PrinterQueueInfo* selectedPrinterQueue() const;
  void syncSelectionToFilter();
  void moveSelection(int delta);
  void changePage(Page page);
  bool chooseHimsFolder();
  void armDeleteConfirmation();
  void cancelDeleteConfirmation();
  void clearDeleteConfirmationIfExpired();
  void confirmDeleteSelectedItem();
  bool deleteConfirmationActive() const;
  bool deleteConfirmationReady() const;
  int deleteConfirmationSecondsLeft() const;
  void openSelectedDetail();
  void openRackManagement();
  std::vector<size_t> sortedRackIndices() const;
  const HimsRack* selectedRack() const;
  HimsRack* selectedRack();
  std::string selectedRackSlot() const;
  InventoryItem* selectedRackItem();
  const InventoryItem* selectedRackItem() const;
  void syncRackSelection();
  void moveRackSlot(int rowDelta, int columnDelta);
  void moveRackPage(int delta);
  void beginOrCompleteRackMove();
  void unassignSelectedRackItem();
  void autoAssignSelectedRackItem();
  void renameSelectedRack(const std::string& value);
  void changeSelectedRackType(const std::string& value);
  void createRackWithType(const std::string& value);
  void deleteSelectedRack();
  void jumpToRack(const std::string& value);
  void beginRackFilter();
  bool printSelectedRackPartLabel();
  bool printSelectedRackLabel();
  void startSearch();
  void cancelInput();
  void beginEditCurrentItem(bool createNew);
  void beginEditImportCandidate();
  void openFieldMenu();
  void commitEditField(EditField field, const std::string& value);
  void saveWorkingCopy();
  void adjustQuantity(int delta);
  void captureUndoSnapshot();
  bool undoLastInventoryChange();
  void logActivity(const std::string& kind, const std::string& message);
  void pushScanCode(const std::string& code);
  void processScans();
  void beginCsvImport();
  void moveImportSelection(int delta);
  void acceptImportCandidate();
  void skipImportCandidate();
  void finishImportReview();
  void finishCsvImport(bool syncWithDigiKey);
  void syncAcceptedImports();
  CsvImportCandidate* currentImportCandidate();
  const CsvImportCandidate* currentImportCandidate() const;
  std::string importCompletionMessage() const;
  void openCurrentUrl(const std::string& url, const std::string& label);
  std::string fieldLabel(EditField field) const;
  std::string currentFieldValue(EditField field) const;
  std::vector<FieldOption> fieldOptions() const;
  std::string softwareVersion() const;
  std::string itemDetailText(const InventoryItem& item, int width) const;
  std::string summaryLine() const;
  std::string scannerUrl() const;
  std::string activePrompt() const;
  std::string shortcutSummary() const;

  InventoryStore store_;
  std::vector<ActivityEntry> activities_;
  LabelPrinterService printerService_;
  std::vector<PrinterQueueInfo> printerQueues_;
  PrinterCheckResult printerCheck_;
  LocalHttpServer server_;
  MdnsService mdnsService_;
  std::filesystem::path root_;
  std::filesystem::path dataPath_;
  std::filesystem::path inventoryPath_;
  std::filesystem::path printerPath_;
  std::filesystem::path activityPath_;
  std::filesystem::path himsScanConfigPath_;
  Page page_ = Page::Dashboard;
  InputMode inputMode_ = InputMode::None;
  std::string searchQuery_;
  std::string inputBuffer_;
  std::string message_;
  time_t messageUntil_ = 0;
  size_t selectedPosition_ = 0;
  size_t stockScroll_ = 0;
  size_t detailScroll_ = 0;
  size_t rackSelection_ = 0;
  int rackRow_ = 0;
  int rackColumn_ = 0;
  std::string movingRackItemId_;
  std::string movingRackSource_;
  std::string rackFilter_;
  std::vector<std::string> scanQueue_;
  std::vector<CsvImportCandidate> importCandidates_;
  std::vector<std::string> importAcceptedItemIds_;
  std::filesystem::path importSourcePath_;
  std::vector<InventoryHistoryPoint> inventoryHistory_;
  std::mutex scanMutex_;
  HimsScanConfig himsScanConfig_;
  std::mutex deviceQueueMutex_;
  std::vector<std::shared_ptr<PendingDeviceQuantity>> deviceQuantityQueue_;
  std::vector<DeviceStatusReport> deviceStatusQueue_;
  std::unordered_map<std::string, DeviceQuantityResult> deviceRequestCache_;
  std::deque<std::string> deviceRequestOrder_;
  time_t deviceLastSeen_ = 0;
  std::string deviceFirmwareVersion_;
  int deviceRssi_ = 0;
  std::string deviceLastResult_;
  bool running_ = true;
  bool dirty_ = true;
  ConsoleSize lastDrawSize_{};
  WorkingCopy workingCopy_;
  UndoSnapshot undoSnapshot_;
  bool editingImportCandidate_ = false;
  size_t importEditIndex_ = 0;
  size_t importSelection_ = 0;
  bool importSyncPrompt_ = false;
  int importCreatedCount_ = 0;
  int importMergedCount_ = 0;
  int importSkippedCount_ = 0;
  int importSyncedCount_ = 0;
  int importSyncFailedCount_ = 0;
  int fieldMenuIndex_ = 0;
  std::vector<FieldOption> menuOptions_;
  std::string deleteConfirmationItemId_;
  time_t deleteConfirmationUntil_ = 0;
  size_t printerSelection_ = 0;
  time_t scannerFlashUntil_ = 0;
  time_t printerFlashUntil_ = 0;
};

}  // namespace hims

