#pragma once

#include "core/Inventory.h"
#include "platform/Console.h"
#include "platform/HttpServer.h"

#include <filesystem>
#include <mutex>
#include <sstream>
#include <string>
#include <vector>

namespace hims {

class App {
 public:
  App();
  int run();

 private:
  enum class Page {
    Dashboard,
    Stock,
    Detail,
  };

  enum class InputMode {
    None,
    Search,
    EditFieldMenu,
    EditValue,
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
  };

  struct FieldOption {
    std::string label;
    EditField field;
  };

  struct WorkingCopy {
    InventoryItem item;
    bool isNew = false;
    std::size_t originalIndex = 0;
  };

  void loadState();
  void saveState();
  void processInput();
  void handleKey(const KeyEvent& key);
  void handleDashboardKey(const KeyEvent& key);
  void handleStockKey(const KeyEvent& key);
  void handleDetailKey(const KeyEvent& key);
  void handleSearchKey(const KeyEvent& key);
  void handleEditMenuKey(const KeyEvent& key);
  void handleEditValueKey(const KeyEvent& key);

  void render();
  void renderDashboard(std::ostringstream& out, const ConsoleSize& size);
  void renderStock(std::ostringstream& out, const ConsoleSize& size);
  void renderDetail(std::ostringstream& out, const ConsoleSize& size);
  void renderSearchBar(std::ostringstream& out, const ConsoleSize& size);
  void renderStatusBar(std::ostringstream& out, const ConsoleSize& size);
  void renderMessage(std::ostringstream& out, const ConsoleSize& size);

  void setMessage(std::string text, int seconds = 3);
  bool messageVisible() const;
  void clearMessageIfExpired();
  void markDirty();

  std::vector<std::size_t> filteredIndices() const;
  std::size_t selectedIndex() const;
  InventoryItem* selectedItem();
  const InventoryItem* selectedItem() const;
  void syncSelectionToFilter();
  void moveSelection(int delta);
  void changePage(Page page);
  void openSelectedDetail();
  void startSearch();
  void cancelInput();
  void beginEditCurrentItem(bool createNew);
  void openFieldMenu();
  void commitEditField(EditField field, const std::string& value);
  void saveWorkingCopy();
  void adjustQuantity(int delta);
  void logActivity(const std::string& kind, const std::string& message);
  void pushScanCode(const std::string& code);
  void processScans();
  void openCurrentUrl(const std::string& url, const std::string& label);
  std::string fieldLabel(EditField field) const;
  std::string currentFieldValue(EditField field) const;
  std::vector<FieldOption> fieldOptions() const;
  std::string itemDetailText(const InventoryItem& item, int width) const;
  std::string summaryLine() const;
  std::string scannerUrl() const;
  std::string activePrompt() const;

  InventoryStore store_;
  std::vector<ActivityEntry> activities_;
  LocalHttpServer server_;
  ConsoleSession console_;
  std::filesystem::path root_;
  std::filesystem::path dataPath_;
  std::filesystem::path inventoryPath_;
  std::filesystem::path activityPath_;
  Page page_ = Page::Dashboard;
  InputMode inputMode_ = InputMode::None;
  std::string searchQuery_;
  std::string inputBuffer_;
  std::string message_;
  std::time_t messageUntil_ = 0;
  std::size_t selectedPosition_ = 0;
  std::size_t stockScroll_ = 0;
  std::size_t detailScroll_ = 0;
  std::vector<std::string> scanQueue_;
  std::mutex scanMutex_;
  bool running_ = true;
  bool dirty_ = true;
  ConsoleSize lastDrawSize_{};
  WorkingCopy workingCopy_;
  int fieldMenuIndex_ = 0;
  std::vector<FieldOption> menuOptions_;
};

}  // namespace hims
