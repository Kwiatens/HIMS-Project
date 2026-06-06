#pragma once

#include <ctime>
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace hims {

struct Parameter {
  std::string name;
  std::string value;
};

struct InventoryItem {
  std::string id;
  std::string partName;
  std::string manufacturer;
  std::string category;
  int quantity = 0;
  int reorderThreshold = 0;
  std::string location;
  std::vector<std::string> tags;
  std::vector<Parameter> parameters;
  std::string notes;
  std::string digikeyPartNumber;
  std::string datasheetUrl;
  std::string productUrl;
  std::string syncStatus = "synced";
  std::string sku;
  std::time_t lastUpdated = 0;

  bool lowStock() const;
  bool hasMissingMetadata() const;
  std::string searchableText() const;
};

struct ActivityEntry {
  std::time_t timestamp = 0;
  std::string kind;
  std::string message;
};

struct Summary {
  std::size_t itemCount = 0;
  std::size_t totalUnits = 0;
  std::size_t lowStockCount = 0;
  std::size_t missingMetadataCount = 0;
  std::size_t unsyncedCount = 0;
};

struct ScanResolution {
  bool matched = false;
  bool created = false;
  std::string itemId;
  std::string message;
};

std::string trim(const std::string& value);
std::string toLower(std::string value);
std::string nowTimestampString(std::time_t value);
std::string makeId();
std::string join(const std::vector<std::string>& values, char delimiter);
std::vector<std::string> split(const std::string& value, char delimiter);
std::vector<std::string> tokenizeQuery(const std::string& query);

bool containsInsensitive(std::string_view haystack, std::string_view needle);
bool matchesQuery(const InventoryItem& item, const std::string& query);
std::vector<std::size_t> filterItems(const std::vector<InventoryItem>& items, const std::string& query);
Summary summarize(const std::vector<InventoryItem>& items);

std::vector<InventoryItem> seedInventory();

class InventoryStore {
 public:
  std::vector<InventoryItem>& items();
  const std::vector<InventoryItem>& items() const;

  bool load(const std::filesystem::path& path);
  bool save(const std::filesystem::path& path) const;

  InventoryItem* findById(const std::string& id);
  const InventoryItem* findById(const std::string& id) const;
  InventoryItem* findByCode(const std::string& code);
  const InventoryItem* findByCode(const std::string& code) const;

 private:
  std::vector<InventoryItem> items_;
};

bool loadActivities(const std::filesystem::path& path, std::vector<ActivityEntry>& activities);
bool saveActivities(const std::filesystem::path& path, const std::vector<ActivityEntry>& activities);
void appendActivity(std::vector<ActivityEntry>& activities, const ActivityEntry& entry, std::size_t maxEntries = 100);
ActivityEntry makeActivity(std::string kind, std::string message);

ScanResolution resolveScanCode(InventoryStore& store, const std::string& rawCode);

std::string serializeItem(const InventoryItem& item);
bool deserializeItem(const std::string& line, InventoryItem& item);
std::string serializeActivity(const ActivityEntry& entry);
bool deserializeActivity(const std::string& line, ActivityEntry& entry);

}  // namespace hims

