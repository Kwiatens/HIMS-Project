#pragma once

#include <ctime>
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace hims {

namespace filesystem = std::filesystem;
using std::filesystem::path;
using std::size_t;
using std::string;
using std::string_view;
using std::time_t;
using std::vector;

struct Parameter {
  string name;
  string value;
};

struct InventoryItem {
  string id;
  string partName;
  string manufacturer;
  string category;
  int quantity = 0;
  int reorderThreshold = 0;
  string location;
  vector<string> tags;
  vector<Parameter> parameters;
  string notes;
  string digikeyPartNumber;
  string datasheetUrl;
  string productUrl;
  string syncStatus = "synced";
  string sku;
  time_t lastUpdated = 0;
  string himsId;
  time_t createdAt = 0;
  string machineCode;

  bool lowStock() const;
  bool hasMissingMetadata() const;
  string searchableText() const;
};

struct ActivityEntry {
  time_t timestamp = 0;
  string kind;
  string message;
};

struct Summary {
  size_t itemCount = 0;
  size_t totalUnits = 0;
  size_t lowStockCount = 0;
  size_t missingMetadataCount = 0;
  size_t unsyncedCount = 0;
};

struct InventoryHistoryPoint {
  time_t timestamp = 0;
  size_t itemCount = 0;
  size_t totalUnits = 0;
  size_t lowStockCount = 0;
  size_t outOfStockCount = 0;
  size_t dataErrorCount = 0;
};

struct ScanResolution {
  bool matched = false;
  bool created = false;
  string itemId;
  string message;
};

string trim(const string& value);
string toLower(string value);
string nowTimestampString(time_t value);
string makeId();
string himsCategoryPrefix(const string& category);
string makeHimsId(const string& category, size_t sequence);
bool isHimsId(const string& value);
void ensureInventoryIdentifiers(vector<InventoryItem>& items);
string join(const vector<string>& values, char delimiter);
vector<string> split(const string& value, char delimiter);
vector<string> tokenizeQuery(const string& query);

bool containsInsensitive(string_view haystack, string_view needle);
bool matchesQuery(const InventoryItem& item, const string& query);
vector<size_t> filterItems(const vector<InventoryItem>& items, const string& query);
Summary summarize(const vector<InventoryItem>& items);
int categoryLowStockThreshold(const string& category);
bool lowStockByCategory(const InventoryItem& item);
InventoryHistoryPoint makeInventoryHistoryPoint(const vector<InventoryItem>& items, time_t timestamp = 0);
bool loadInventoryHistory(const filesystem::path& path, vector<InventoryHistoryPoint>& history);
bool saveInventoryHistory(const filesystem::path& path, const vector<InventoryHistoryPoint>& history);
void appendInventoryHistory(vector<InventoryHistoryPoint>& history, const InventoryHistoryPoint& point,
                            size_t maxEntries = 180);

class InventoryStore {
 public:
  vector<InventoryItem>& items();
  const vector<InventoryItem>& items() const;

  bool load(const filesystem::path& path);
  bool save(const filesystem::path& path) const;

  InventoryItem* findById(const string& id);
  const InventoryItem* findById(const string& id) const;
  InventoryItem* findByCode(const string& code);
  const InventoryItem* findByCode(const string& code) const;
  InventoryItem* findByMachineCode(const string& machineCode);
  const InventoryItem* findByMachineCode(const string& machineCode) const;

 private:
  vector<InventoryItem> items_;
};

bool loadActivities(const filesystem::path& path, vector<ActivityEntry>& activities);
bool saveActivities(const filesystem::path& path, const vector<ActivityEntry>& activities);
void appendActivity(vector<ActivityEntry>& activities, const ActivityEntry& entry, size_t maxEntries = 100);
ActivityEntry makeActivity(string kind, string message);

ScanResolution resolveScanCode(InventoryStore& store, const string& rawCode);

string serializeItem(const InventoryItem& item);
bool deserializeItem(const string& line, InventoryItem& item);
string serializeActivity(const ActivityEntry& entry);
bool deserializeActivity(const string& line, ActivityEntry& entry);

}  // namespace hims

