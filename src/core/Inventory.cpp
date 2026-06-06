#include "core/Inventory.h"

#include <cstdlib>
#include <algorithm>
#include <cctype>
#include <chrono>
#include <ctime>
#include <fstream>
#include <iomanip>
#include <random>
#include <sstream>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#endif

namespace hims {

namespace {

std::time_t nowEpoch() {
  return std::time(nullptr);
}

std::vector<std::string> splitTokensRespectingQuotes(const std::string& query) {
  std::vector<std::string> tokens;
  std::string current;
  bool inQuotes = false;

  for (char ch : query) {
    if (ch == '"') {
      inQuotes = !inQuotes;
      continue;
    }

    if (!inQuotes && std::isspace(static_cast<unsigned char>(ch))) {
      if (!current.empty()) {
        tokens.push_back(current);
        current.clear();
      }
      continue;
    }

    current.push_back(ch);
  }

  if (!current.empty()) {
    tokens.push_back(current);
  }

  return tokens;
}

bool tokenMatchesParameter(const InventoryItem& item, const std::string& value) {
  const auto equalsPos = value.find('=');
  const auto needleKey = toLower(equalsPos == std::string::npos ? value : value.substr(0, equalsPos));
  const auto needleValue = equalsPos == std::string::npos ? std::string() : toLower(value.substr(equalsPos + 1));

  for (const auto& parameter : item.parameters) {
    const auto key = toLower(parameter.name);
    const auto parameterValue = toLower(parameter.value);

    if (!needleKey.empty() && key.find(needleKey) == std::string::npos) {
      continue;
    }

    if (needleValue.empty() || parameterValue.find(needleValue) != std::string::npos) {
      return true;
    }
  }

  return false;
}

bool tokenMatchesQuantity(const InventoryItem& item, const std::string& token) {
  if (token.rfind("qty>=", 0) == 0) {
    return item.quantity >= std::stoi(token.substr(5));
  }
  if (token.rfind("qty<=", 0) == 0) {
    return item.quantity <= std::stoi(token.substr(5));
  }
  if (token.rfind("qty>", 0) == 0) {
    return item.quantity > std::stoi(token.substr(4));
  }
  if (token.rfind("qty<", 0) == 0) {
    return item.quantity < std::stoi(token.substr(4));
  }
  if (token.rfind("qty=", 0) == 0) {
    return item.quantity == std::stoi(token.substr(4));
  }
  return false;
}

bool tokenMatchesStatus(const InventoryItem& item, const std::string& value) {
  const auto lowerValue = toLower(value);
  if (lowerValue == "low") {
    return item.lowStock();
  }
  if (lowerValue == "missing") {
    return item.hasMissingMetadata();
  }
  if (lowerValue == "synced") {
    return toLower(item.syncStatus) == "synced";
  }
  if (lowerValue == "unsynced") {
    return toLower(item.syncStatus) != "synced";
  }
  return containsInsensitive(item.syncStatus, lowerValue);
}

bool tokenMatchesCategory(const InventoryItem& item, const std::string& value) {
  return containsInsensitive(item.category, value);
}

bool tokenMatchesField(const std::string& field, const std::string& value) {
  return containsInsensitive(field, value);
}

std::string sanitizeIdPart(const std::string& value) {
  std::string output;
  output.reserve(value.size());
  for (char ch : value) {
    if (std::isalnum(static_cast<unsigned char>(ch))) {
      output.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(ch))));
    } else if (!output.empty() && output.back() != '-') {
      output.push_back('-');
    }
  }

  while (!output.empty() && output.back() == '-') {
    output.pop_back();
  }

  if (output.empty()) {
    output = "item";
  }
  return output;
}

}  // namespace

#ifdef _WIN32
using sqlite3 = struct sqlite3;
using sqlite3_stmt = struct sqlite3_stmt;
using sqlite3_int64 = long long;
using sqlite3_destructor_type = void (*)(void*);

const sqlite3_destructor_type SQLITE_TRANSIENT = reinterpret_cast<sqlite3_destructor_type>(-1);
constexpr int SQLITE_OK = 0;
constexpr int SQLITE_ROW = 100;
constexpr int SQLITE_DONE = 101;
constexpr int SQLITE_OPEN_READWRITE = 0x00000002;
constexpr int SQLITE_OPEN_CREATE = 0x00000004;

using sqlite3_initialize_fn = int (*)();
using sqlite3_open_v2_fn = int (*)(const char*, sqlite3**, int, const char*);
using sqlite3_close_fn = int (*)(sqlite3*);
using sqlite3_exec_fn = int (*)(sqlite3*, const char*, int (*)(void*, int, char**, char**), void*, char**);
using sqlite3_errmsg_fn = const char* (*)(sqlite3*);
using sqlite3_prepare_v2_fn = int (*)(sqlite3*, const char*, int, sqlite3_stmt**, const char**);
using sqlite3_step_fn = int (*)(sqlite3_stmt*);
using sqlite3_finalize_fn = int (*)(sqlite3_stmt*);
using sqlite3_reset_fn = int (*)(sqlite3_stmt*);
using sqlite3_clear_bindings_fn = int (*)(sqlite3_stmt*);
using sqlite3_bind_text_fn = int (*)(sqlite3_stmt*, int, const char*, int, sqlite3_destructor_type);
using sqlite3_bind_int_fn = int (*)(sqlite3_stmt*, int, int);
using sqlite3_bind_int64_fn = int (*)(sqlite3_stmt*, int, sqlite3_int64);
using sqlite3_bind_null_fn = int (*)(sqlite3_stmt*, int);
using sqlite3_column_int_fn = int (*)(sqlite3_stmt*, int);
using sqlite3_column_int64_fn = sqlite3_int64 (*)(sqlite3_stmt*, int);
using sqlite3_column_text_fn = const unsigned char* (*)(sqlite3_stmt*, int);
using sqlite3_busy_timeout_fn = int (*)(sqlite3*, int);
using sqlite3_free_fn = void (*)(void*);

struct SqliteApi {
  HMODULE module = nullptr;
  sqlite3_initialize_fn initialize = nullptr;
  sqlite3_open_v2_fn open_v2 = nullptr;
  sqlite3_close_fn close = nullptr;
  sqlite3_exec_fn exec = nullptr;
  sqlite3_errmsg_fn errmsg = nullptr;
  sqlite3_prepare_v2_fn prepare_v2 = nullptr;
  sqlite3_step_fn step = nullptr;
  sqlite3_finalize_fn finalize = nullptr;
  sqlite3_reset_fn reset = nullptr;
  sqlite3_clear_bindings_fn clear_bindings = nullptr;
  sqlite3_bind_text_fn bind_text = nullptr;
  sqlite3_bind_int_fn bind_int = nullptr;
  sqlite3_bind_int64_fn bind_int64 = nullptr;
  sqlite3_bind_null_fn bind_null = nullptr;
  sqlite3_column_int_fn column_int = nullptr;
  sqlite3_column_int64_fn column_int64 = nullptr;
  sqlite3_column_text_fn column_text = nullptr;
  sqlite3_busy_timeout_fn busy_timeout = nullptr;
  sqlite3_free_fn free = nullptr;

  template <typename T>
  static T loadSymbol(HMODULE module, const char* name) {
    return reinterpret_cast<T>(GetProcAddress(module, name));
  }

  bool load() {
    if (module != nullptr) {
      return true;
    }

    const std::filesystem::path candidates[] = {
        std::filesystem::path("sqlite3.dll"),
        std::filesystem::current_path() / "sqlite3.dll",
        std::filesystem::path(R"(C:\Users\pawci\.platformio\python3\DLLs\sqlite3.dll)"),
        std::filesystem::path(R"(C:\Users\pawci\.cache\codex-runtimes\codex-primary-runtime\dependencies\python\DLLs\sqlite3.dll)"),
        std::filesystem::path(R"(C:\Users\pawci\AppData\Local\Programs\KiCad\10.0\bin\sqlite3.dll)"),
        std::filesystem::path(R"(C:\Program Files\Blender Foundation\Blender 5.1\5.1\python\DLLs\sqlite3.dll)"),
        std::filesystem::path(R"(C:\Program Files\Blender Foundation\Blender 5.1\python\DLLs\sqlite3.dll)")};

    for (const auto& candidate : candidates) {
      module = LoadLibraryW(candidate.wstring().c_str());
      if (module != nullptr) {
        break;
      }
    }

    if (module == nullptr) {
      return false;
    }

    initialize = loadSymbol<sqlite3_initialize_fn>(module, "sqlite3_initialize");
    open_v2 = loadSymbol<sqlite3_open_v2_fn>(module, "sqlite3_open_v2");
    close = loadSymbol<sqlite3_close_fn>(module, "sqlite3_close");
    exec = loadSymbol<sqlite3_exec_fn>(module, "sqlite3_exec");
    errmsg = loadSymbol<sqlite3_errmsg_fn>(module, "sqlite3_errmsg");
    prepare_v2 = loadSymbol<sqlite3_prepare_v2_fn>(module, "sqlite3_prepare_v2");
    step = loadSymbol<sqlite3_step_fn>(module, "sqlite3_step");
    finalize = loadSymbol<sqlite3_finalize_fn>(module, "sqlite3_finalize");
    reset = loadSymbol<sqlite3_reset_fn>(module, "sqlite3_reset");
    clear_bindings = loadSymbol<sqlite3_clear_bindings_fn>(module, "sqlite3_clear_bindings");
    bind_text = loadSymbol<sqlite3_bind_text_fn>(module, "sqlite3_bind_text");
    bind_int = loadSymbol<sqlite3_bind_int_fn>(module, "sqlite3_bind_int");
    bind_int64 = loadSymbol<sqlite3_bind_int64_fn>(module, "sqlite3_bind_int64");
    bind_null = loadSymbol<sqlite3_bind_null_fn>(module, "sqlite3_bind_null");
    column_int = loadSymbol<sqlite3_column_int_fn>(module, "sqlite3_column_int");
    column_int64 = loadSymbol<sqlite3_column_int64_fn>(module, "sqlite3_column_int64");
    column_text = loadSymbol<sqlite3_column_text_fn>(module, "sqlite3_column_text");
    busy_timeout = loadSymbol<sqlite3_busy_timeout_fn>(module, "sqlite3_busy_timeout");
    free = loadSymbol<sqlite3_free_fn>(module, "sqlite3_free");

    if (!(initialize && open_v2 && close && exec && errmsg && prepare_v2 && step && finalize && reset &&
          clear_bindings && bind_text && bind_int && bind_int64 && bind_null && column_int && column_int64 &&
          column_text && busy_timeout && free)) {
      return false;
    }

    return initialize() == SQLITE_OK;
  }

  ~SqliteApi() {
    if (module != nullptr) {
      FreeLibrary(module);
    }
  }
};

SqliteApi& sqliteApi() {
  static SqliteApi api;
  static const bool loaded = api.load();
  (void)loaded;
  return api;
}

struct SqliteConnection {
  sqlite3* db = nullptr;
  ~SqliteConnection() {
    if (db != nullptr) {
      sqliteApi().close(db);
    }
  }
};

struct SqliteStatement {
  sqlite3_stmt* stmt = nullptr;
  ~SqliteStatement() {
    if (stmt != nullptr) {
      sqliteApi().finalize(stmt);
    }
  }
};

std::string sqliteText(sqlite3_stmt* stmt, int column) {
  const auto* text = sqliteApi().column_text(stmt, column);
  return text == nullptr ? std::string() : reinterpret_cast<const char*>(text);
}

bool openDatabase(const std::filesystem::path& path, SqliteConnection& connection) {
  const auto& api = sqliteApi();
  if (api.open_v2 == nullptr) {
    return false;
  }

  if (api.open_v2(path.string().c_str(), &connection.db, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE, nullptr) != SQLITE_OK) {
    connection.db = nullptr;
    return false;
  }

  api.busy_timeout(connection.db, 3000);
  return true;
}

bool execSql(SqliteConnection& connection, const std::string& sql) {
  char* error = nullptr;
  const auto rc = sqliteApi().exec(connection.db, sql.c_str(), nullptr, nullptr, &error);
  if (error != nullptr) {
    sqliteApi().free(error);
  }
  return rc == SQLITE_OK;
}

bool tableExists(SqliteConnection& connection, const std::string& tableName) {
  SqliteStatement statement;
  const std::string sql = "SELECT name FROM sqlite_master WHERE type='table' AND name=? LIMIT 1";
  if (sqliteApi().prepare_v2(connection.db, sql.c_str(), -1, &statement.stmt, nullptr) != SQLITE_OK) {
    return false;
  }
  sqliteApi().bind_text(statement.stmt, 1, tableName.c_str(), -1, SQLITE_TRANSIENT);
  return sqliteApi().step(statement.stmt) == SQLITE_ROW;
}

std::string joinParametersForDb(const std::vector<Parameter>& parameters) {
  std::vector<std::string> serialized;
  serialized.reserve(parameters.size());
  for (const auto& parameter : parameters) {
    serialized.push_back(parameter.name + "=" + parameter.value);
  }
  return join(serialized, ';');
}

std::vector<Parameter> parseParametersFromDb(const std::string& value) {
  std::vector<Parameter> parameters;
  for (const auto& entry : split(value, ';')) {
    const auto equalsPos = entry.find('=');
    if (equalsPos == std::string::npos) {
      continue;
    }
    parameters.push_back({trim(entry.substr(0, equalsPos)), trim(entry.substr(equalsPos + 1))});
  }
  return parameters;
}

std::string joinTagsForDb(const std::vector<std::string>& tags) {
  return join(tags, '|');
}

std::vector<std::string> parseTagsFromDb(const std::string& value) {
  return split(value, '|');
}

InventoryItem legacyRowToItem(sqlite3_stmt* stmt) {
  InventoryItem item;
  const auto legacyId = sqliteApi().column_int64(stmt, 0);
  const auto partNumber = sqliteText(stmt, 1);
  const auto partNumberNormalized = sqliteText(stmt, 2);
  const auto quantity = sqliteApi().column_int(stmt, 3);
  const auto location = sqliteText(stmt, 4);
  const auto manufacturer = sqliteText(stmt, 5);
  const auto packageName = sqliteText(stmt, 6);
  const auto description = sqliteText(stmt, 7);
  const auto notes = sqliteText(stmt, 8);
  const auto updatedAt = sqliteText(stmt, 10);
  const auto digikeyPartNumber = sqliteText(stmt, 11);
  const auto category = sqliteText(stmt, 12);
  const auto subcategory = sqliteText(stmt, 13);
  const auto productUrl = sqliteText(stmt, 14);
  const auto datasheetUrl = sqliteText(stmt, 15);
  const auto searchText = sqliteText(stmt, 18);
  const auto enrichmentStatus = sqliteText(stmt, 19);
  const auto inventoryArea = sqliteText(stmt, 22);
  const auto reorderOverride = sqliteApi().column_int(stmt, 23);
  const auto hardwareType = sqliteText(stmt, 24);
  const auto hardwareSize = sqliteText(stmt, 25);
  const auto hardwareLength = sqliteText(stmt, 26);
  const auto filamentMaterial = sqliteText(stmt, 28);
  const auto filamentColor = sqliteText(stmt, 29);
  const auto filamentDiameterMm = sqliteText(stmt, 30);

  item.id = std::to_string(legacyId);
  item.partName = partNumber.empty() ? description : partNumber;
  item.manufacturer = manufacturer;
  item.category = category.empty() ? subcategory : (subcategory.empty() ? category : category + " / " + subcategory);
  item.quantity = quantity;
  item.reorderThreshold = reorderOverride >= 0 ? reorderOverride : 0;
  item.location = location;
  if (!inventoryArea.empty()) {
    item.tags.push_back(inventoryArea);
  }
  if (!packageName.empty()) {
    item.tags.push_back(packageName);
  }
  if (!subcategory.empty()) {
    item.tags.push_back(subcategory);
  }
  if (!packageName.empty()) {
    item.parameters.push_back({"Package", packageName});
  }
  if (!inventoryArea.empty()) {
    item.parameters.push_back({"Inventory Area", inventoryArea});
  }
  if (!hardwareType.empty()) {
    item.parameters.push_back({"Hardware Type", hardwareType});
  }
  if (!hardwareSize.empty()) {
    item.parameters.push_back({"Hardware Size", hardwareSize});
  }
  if (!hardwareLength.empty()) {
    item.parameters.push_back({"Hardware Length", hardwareLength});
  }
  if (!filamentMaterial.empty()) {
    item.parameters.push_back({"Filament Material", filamentMaterial});
  }
  if (!filamentColor.empty()) {
    item.parameters.push_back({"Filament Color", filamentColor});
  }
  if (!filamentDiameterMm.empty()) {
    item.parameters.push_back({"Filament Diameter", filamentDiameterMm});
  }
  item.notes = description.empty() ? notes : (notes.empty() ? description : description + " | " + notes);
  item.digikeyPartNumber = digikeyPartNumber;
  item.datasheetUrl = datasheetUrl;
  item.productUrl = productUrl;
  item.syncStatus = enrichmentStatus.empty() ? "needs_metadata" : toLower(enrichmentStatus);
  item.sku = partNumberNormalized.empty() ? partNumber : partNumberNormalized;
  item.lastUpdated = nowEpoch();
  if (!updatedAt.empty()) {
    item.lastUpdated = nowEpoch();
  }
  if (!searchText.empty() && item.notes.empty()) {
    item.notes = searchText;
  }
  return item;
}

bool createHimsTable(SqliteConnection& connection) {
  return execSql(connection, R"SQL(
    CREATE TABLE IF NOT EXISTS hims_items (
      id TEXT PRIMARY KEY,
      part_name TEXT NOT NULL,
      manufacturer TEXT NOT NULL,
      category TEXT NOT NULL,
      quantity INTEGER NOT NULL,
      reorder_threshold INTEGER NOT NULL,
      location TEXT NOT NULL,
      tags TEXT NOT NULL,
      parameters TEXT NOT NULL,
      notes TEXT NOT NULL,
      digikey_part_number TEXT NOT NULL,
      datasheet_url TEXT NOT NULL,
      product_url TEXT NOT NULL,
      sync_status TEXT NOT NULL,
      sku TEXT NOT NULL,
      last_updated INTEGER NOT NULL
    )
  )SQL");
}

bool loadItemsFromHimsTable(SqliteConnection& connection, std::vector<InventoryItem>& items) {
  SqliteStatement statement;
  const char* sql = R"SQL(
    SELECT id, part_name, manufacturer, category, quantity, reorder_threshold, location,
           tags, parameters, notes, digikey_part_number, datasheet_url, product_url,
           sync_status, sku, last_updated
    FROM hims_items
    ORDER BY part_name COLLATE NOCASE ASC
  )SQL";

  if (sqliteApi().prepare_v2(connection.db, sql, -1, &statement.stmt, nullptr) != SQLITE_OK) {
    return false;
  }

  while (sqliteApi().step(statement.stmt) == SQLITE_ROW) {
    InventoryItem item;
    item.id = sqliteText(statement.stmt, 0);
    item.partName = sqliteText(statement.stmt, 1);
    item.manufacturer = sqliteText(statement.stmt, 2);
    item.category = sqliteText(statement.stmt, 3);
    item.quantity = sqliteApi().column_int(statement.stmt, 4);
    item.reorderThreshold = sqliteApi().column_int(statement.stmt, 5);
    item.location = sqliteText(statement.stmt, 6);
    item.tags = parseTagsFromDb(sqliteText(statement.stmt, 7));
    item.parameters = parseParametersFromDb(sqliteText(statement.stmt, 8));
    item.notes = sqliteText(statement.stmt, 9);
    item.digikeyPartNumber = sqliteText(statement.stmt, 10);
    item.datasheetUrl = sqliteText(statement.stmt, 11);
    item.productUrl = sqliteText(statement.stmt, 12);
    item.syncStatus = sqliteText(statement.stmt, 13);
    item.sku = sqliteText(statement.stmt, 14);
    item.lastUpdated = static_cast<std::time_t>(sqliteApi().column_int64(statement.stmt, 15));
    items.push_back(std::move(item));
  }

  return true;
}

bool importLegacyItems(SqliteConnection& connection, std::vector<InventoryItem>& items) {
  SqliteStatement statement;
  const char* sql = R"SQL(
    SELECT id, part_number, part_number_normalized, quantity, location, manufacturer, package, description,
           notes, created_at, updated_at, digikey_part_number, category, subcategory, product_url, datasheet_url,
           image_url, specs_json, search_text, enrichment_status, enrichment_error, last_enriched_at,
           inventory_area, reorder_point_override, hardware_type, hardware_size, hardware_length,
           hardware_material_finish, filament_material, filament_color, filament_diameter_mm,
           filament_spool_weight_g, filament_remaining_weight_g
    FROM items
    ORDER BY part_number COLLATE NOCASE ASC
  )SQL";

  if (sqliteApi().prepare_v2(connection.db, sql, -1, &statement.stmt, nullptr) != SQLITE_OK) {
    return false;
  }

  while (sqliteApi().step(statement.stmt) == SQLITE_ROW) {
    items.push_back(legacyRowToItem(statement.stmt));
  }

  return true;
}

bool writeItemsToHimsTable(SqliteConnection& connection, const std::vector<InventoryItem>& items) {
  if (!createHimsTable(connection)) {
    return false;
  }

  if (!execSql(connection, "BEGIN IMMEDIATE TRANSACTION")) {
    return false;
  }
  if (!execSql(connection, "DELETE FROM hims_items")) {
    execSql(connection, "ROLLBACK");
    return false;
  }

  SqliteStatement statement;
  const char* sql = R"SQL(
    INSERT OR REPLACE INTO hims_items (
      id, part_name, manufacturer, category, quantity, reorder_threshold, location,
      tags, parameters, notes, digikey_part_number, datasheet_url, product_url,
      sync_status, sku, last_updated
    ) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
  )SQL";

  if (sqliteApi().prepare_v2(connection.db, sql, -1, &statement.stmt, nullptr) != SQLITE_OK) {
    execSql(connection, "ROLLBACK");
    return false;
  }

  for (const auto& item : items) {
    sqliteApi().bind_text(statement.stmt, 1, item.id.c_str(), -1, SQLITE_TRANSIENT);
    sqliteApi().bind_text(statement.stmt, 2, item.partName.c_str(), -1, SQLITE_TRANSIENT);
    sqliteApi().bind_text(statement.stmt, 3, item.manufacturer.c_str(), -1, SQLITE_TRANSIENT);
    sqliteApi().bind_text(statement.stmt, 4, item.category.c_str(), -1, SQLITE_TRANSIENT);
    sqliteApi().bind_int(statement.stmt, 5, item.quantity);
    sqliteApi().bind_int(statement.stmt, 6, item.reorderThreshold);
    sqliteApi().bind_text(statement.stmt, 7, item.location.c_str(), -1, SQLITE_TRANSIENT);
    const auto tags = joinTagsForDb(item.tags);
    const auto parameters = joinParametersForDb(item.parameters);
    sqliteApi().bind_text(statement.stmt, 8, tags.c_str(), -1, SQLITE_TRANSIENT);
    sqliteApi().bind_text(statement.stmt, 9, parameters.c_str(), -1, SQLITE_TRANSIENT);
    sqliteApi().bind_text(statement.stmt, 10, item.notes.c_str(), -1, SQLITE_TRANSIENT);
    sqliteApi().bind_text(statement.stmt, 11, item.digikeyPartNumber.c_str(), -1, SQLITE_TRANSIENT);
    sqliteApi().bind_text(statement.stmt, 12, item.datasheetUrl.c_str(), -1, SQLITE_TRANSIENT);
    sqliteApi().bind_text(statement.stmt, 13, item.productUrl.c_str(), -1, SQLITE_TRANSIENT);
    sqliteApi().bind_text(statement.stmt, 14, item.syncStatus.c_str(), -1, SQLITE_TRANSIENT);
    sqliteApi().bind_text(statement.stmt, 15, item.sku.c_str(), -1, SQLITE_TRANSIENT);
    sqliteApi().bind_int64(statement.stmt, 16, static_cast<sqlite3_int64>(item.lastUpdated));

    if (sqliteApi().step(statement.stmt) != SQLITE_DONE) {
      execSql(connection, "ROLLBACK");
      return false;
    }

    sqliteApi().reset(statement.stmt);
    sqliteApi().clear_bindings(statement.stmt);
  }

  if (!execSql(connection, "COMMIT")) {
    execSql(connection, "ROLLBACK");
    return false;
  }

  return true;
}
#endif

bool InventoryItem::lowStock() const {
  return quantity <= reorderThreshold;
}

bool InventoryItem::hasMissingMetadata() const {
  return partName.empty() || manufacturer.empty() || category.empty() || digikeyPartNumber.empty() ||
         datasheetUrl.empty() || productUrl.empty();
}

std::string InventoryItem::searchableText() const {
  std::ostringstream out;
  out << partName << ' ' << manufacturer << ' ' << category << ' ' << location << ' ' << notes << ' '
      << digikeyPartNumber << ' ' << datasheetUrl << ' ' << productUrl << ' ' << sku << ' ' << syncStatus;

  for (const auto& tag : tags) {
    out << ' ' << tag;
  }

  for (const auto& parameter : parameters) {
    out << ' ' << parameter.name << ':' << parameter.value;
  }

  return toLower(out.str());
}

std::string trim(const std::string& value) {
  const auto begin = std::find_if_not(value.begin(), value.end(), [](unsigned char ch) {
    return std::isspace(ch) != 0;
  });
  const auto end = std::find_if_not(value.rbegin(), value.rend(), [](unsigned char ch) {
    return std::isspace(ch) != 0;
  }).base();

  if (begin >= end) {
    return {};
  }

  return std::string(begin, end);
}

std::string toLower(std::string value) {
  std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
    return static_cast<char>(std::tolower(ch));
  });
  return value;
}

std::string nowTimestampString(std::time_t value) {
  std::tm tm{};
#ifdef _WIN32
  localtime_s(&tm, &value);
#else
  localtime_r(&value, &tm);
#endif
  char buffer[32];
  std::strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M", &tm);
  return buffer;
}

std::string makeId() {
  const auto stamp = std::chrono::system_clock::now().time_since_epoch().count();
  std::mt19937_64 rng(static_cast<std::mt19937_64::result_type>(stamp));
  std::uniform_int_distribution<unsigned long long> dist;
  std::ostringstream out;
  out << std::hex << std::setw(10) << std::setfill('0') << (stamp & 0xfffffffffULL) << '-'
      << std::setw(10) << std::setfill('0') << (dist(rng) & 0xfffffffffULL);
  return out.str();
}

std::string join(const std::vector<std::string>& values, char delimiter) {
  std::ostringstream out;
  for (std::size_t index = 0; index < values.size(); ++index) {
    if (index > 0) {
      out << delimiter;
    }
    out << values[index];
  }
  return out.str();
}

std::vector<std::string> split(const std::string& value, char delimiter) {
  std::vector<std::string> values;
  std::string current;
  std::istringstream input(value);

  while (std::getline(input, current, delimiter)) {
    current = trim(current);
    if (!current.empty()) {
      values.push_back(current);
    }
  }

  return values;
}

std::vector<std::string> tokenizeQuery(const std::string& query) {
  return splitTokensRespectingQuotes(trim(query));
}

bool containsInsensitive(std::string_view haystack, std::string_view needle) {
  if (needle.empty()) {
    return true;
  }

  std::string loweredHaystack(haystack);
  std::string loweredNeedle(needle);
  loweredHaystack = toLower(std::move(loweredHaystack));
  loweredNeedle = toLower(std::move(loweredNeedle));
  return loweredHaystack.find(loweredNeedle) != std::string::npos;
}

bool matchesQuery(const InventoryItem& item, const std::string& query) {
  const auto tokens = tokenizeQuery(query);
  if (tokens.empty()) {
    return true;
  }

  for (const auto& rawToken : tokens) {
    const auto token = toLower(rawToken);

    if (tokenMatchesQuantity(item, token)) {
      continue;
    }

    if (token.rfind("cat:", 0) == 0 || token.rfind("category:", 0) == 0) {
      const auto value = token.substr(token.find(':') + 1);
      if (tokenMatchesCategory(item, value)) {
        continue;
      }
      return false;
    }

    if (token.rfind("mfg:", 0) == 0 || token.rfind("manufacturer:", 0) == 0) {
      const auto value = token.substr(token.find(':') + 1);
      if (tokenMatchesField(item.manufacturer, value)) {
        continue;
      }
      return false;
    }

    if (token.rfind("name:", 0) == 0 || token.rfind("part:", 0) == 0) {
      const auto value = token.substr(token.find(':') + 1);
      if (tokenMatchesField(item.partName, value)) {
        continue;
      }
      return false;
    }

    if (token.rfind("loc:", 0) == 0 || token.rfind("location:", 0) == 0) {
      const auto value = token.substr(token.find(':') + 1);
      if (tokenMatchesField(item.location, value)) {
        continue;
      }
      return false;
    }

    if (token.rfind("sku:", 0) == 0) {
      const auto value = token.substr(4);
      if (tokenMatchesField(item.sku, value)) {
        continue;
      }
      return false;
    }

    if (token.rfind("dg:", 0) == 0 || token.rfind("digikey:", 0) == 0) {
      const auto value = token.substr(token.find(':') + 1);
      if (tokenMatchesField(item.digikeyPartNumber, value)) {
        continue;
      }
      return false;
    }

    if (token.rfind("tag:", 0) == 0) {
      const auto value = token.substr(4);
      const auto matched = std::any_of(item.tags.begin(), item.tags.end(), [&](const std::string& tag) {
        return containsInsensitive(tag, value);
      });
      if (matched) {
        continue;
      }
      return false;
    }

    if (token.rfind("param:", 0) == 0) {
      const auto value = token.substr(6);
      if (tokenMatchesParameter(item, value)) {
        continue;
      }
      return false;
    }

    if (token.rfind("status:", 0) == 0) {
      const auto value = token.substr(7);
      if (tokenMatchesStatus(item, value)) {
        continue;
      }
      return false;
    }

    if (containsInsensitive(item.searchableText(), token)) {
      continue;
    }

    return false;
  }

  return true;
}

std::vector<std::size_t> filterItems(const std::vector<InventoryItem>& items, const std::string& query) {
  std::vector<std::size_t> indices;
  for (std::size_t index = 0; index < items.size(); ++index) {
    if (matchesQuery(items[index], query)) {
      indices.push_back(index);
    }
  }
  return indices;
}

Summary summarize(const std::vector<InventoryItem>& items) {
  Summary summary;
  summary.itemCount = items.size();

  for (const auto& item : items) {
    summary.totalUnits += static_cast<std::size_t>(std::max(item.quantity, 0));
    if (item.lowStock()) {
      ++summary.lowStockCount;
    }
    if (item.hasMissingMetadata()) {
      ++summary.missingMetadataCount;
    }
    if (toLower(item.syncStatus) != "synced") {
      ++summary.unsyncedCount;
    }
  }

  return summary;
}

std::vector<InventoryItem> seedInventory() {
  std::vector<InventoryItem> items;

  items.push_back({
      "res-0603-10k",
      "10k Resistor 0603",
      "Yageo",
      "Resistors",
      180,
      50,
      "Shelf A3",
      {"0603", "1%", "rohs"},
      {{"Resistance", "10k Ohm"}, {"Power", "0.1W"}, {"Package", "0603"}},
      "General purpose pull-up and divider resistor.",
      "311-10.0KHRCT-ND",
      "https://www.digikey.com/en/products/detail/yageo/RC0603FR-0710KL/729604",
      "https://www.digikey.com/en/products/detail/yageo/RC0603FR-0710KL/729604",
      "synced",
      "RC0603FR-0710KL",
      nowEpoch(),
  });

  items.push_back({
      "cap-0805-1uf",
      "1uF Capacitor 0805",
      "Murata",
      "Capacitors",
      42,
      25,
      "Shelf A4",
      {"0805", "ceramic", "x7r"},
      {{"Capacitance", "1uF"}, {"Voltage", "50V"}, {"Package", "0805"}},
      "Decoupling capacitor for logic rails.",
      "490-1504-1-ND",
      "https://www.digikey.com/en/products/detail/murata-electronics/GRM21BR71H105KA12L/372513",
      "https://www.digikey.com/en/products/detail/murata-electronics/GRM21BR71H105KA12L/372513",
      "synced",
      "GRM21BR71H105KA12L",
      nowEpoch(),
  });

  items.push_back({
      "led-green-5mm",
      "Green LED 5mm",
      "Kingbright",
      "Indicators",
      12,
      20,
      "Shelf B1",
      {"through-hole", "indicator"},
      {{"Color", "Green"}, {"Package", "5mm"}, {"Forward Voltage", "2.1V"}},
      "Panel and status indication.",
      "754-1548-ND",
      "https://www.digikey.com/en/products/detail/kingbright/L-53GD/1747565",
      "https://www.digikey.com/en/products/detail/kingbright/L-53GD/1747565",
      "synced",
      "L-53GD",
      nowEpoch(),
  });

  items.push_back({
      "hdr-2x5",
      "2x5 Pin Header",
      "Samtec",
      "Connectors",
      8,
      15,
      "Bin C2",
      {"connector", "header"},
      {{"Rows", "2"}, {"Pins", "10"}, {"Pitch", "2.54mm"}},
      "Programming and debug adapter header.",
      "",
      "",
      "",
      "needs_metadata",
      "TSW-105-07-F-D",
      nowEpoch(),
  });

  items.push_back({
      "esp32-s3-module",
      "ESP32-S3 Module",
      "Espressif",
      "MCUs",
      4,
      10,
      "ESD Drawer",
      {"wifi", "bluetooth", "module"},
      {{"Core", "Xtensa LX7"}, {"Flash", "16MB"}, {"Package", "Module"}},
      "Used for integration prototypes and test rigs.",
      "1965-ESP32-S3-MODULE-ND",
      "https://www.digikey.com/en/products/detail/espressif-systems/ESP32-S3/15240400",
      "https://www.digikey.com/en/products/detail/espressif-systems/ESP32-S3/15240400",
      "synced",
      "ESP32-S3-WROOM-1",
      nowEpoch(),
  });

  return items;
}

std::vector<InventoryItem>& InventoryStore::items() {
  return items_;
}

const std::vector<InventoryItem>& InventoryStore::items() const {
  return items_;
}

bool InventoryStore::load(const std::filesystem::path& path) {
  items_.clear();
#ifdef _WIN32
  SqliteConnection connection;
  if (!openDatabase(path, connection)) {
    items_ = seedInventory();
    return false;
  }

  const bool hasHimsTable = tableExists(connection, "hims_items");
  const bool hasLegacyTable = tableExists(connection, "items");

  std::vector<InventoryItem> himsItems;
  bool loaded = false;
  if (hasHimsTable) {
    loaded = loadItemsFromHimsTable(connection, himsItems);
  }

  std::vector<InventoryItem> legacyItems;
  if (hasLegacyTable) {
    importLegacyItems(connection, legacyItems);
  }

  if (!legacyItems.empty() && (himsItems.empty() || legacyItems.size() > himsItems.size())) {
    items_ = std::move(legacyItems);
    loaded = true;
    writeItemsToHimsTable(connection, items_);
  } else if (loaded && !himsItems.empty()) {
    items_ = std::move(himsItems);
  } else if (!loaded || items_.empty()) {
    if (!himsItems.empty()) {
      items_ = std::move(himsItems);
      loaded = true;
    } else if (!legacyItems.empty()) {
      items_ = std::move(legacyItems);
      loaded = true;
      writeItemsToHimsTable(connection, items_);
    }
  }

  if (!loaded || items_.empty()) {
    items_ = seedInventory();
    writeItemsToHimsTable(connection, items_);
    return false;
  }

  return true;
#else
  std::ifstream file(path);
  if (!file) {
    items_ = seedInventory();
    return false;
  }

  std::string line;
  while (std::getline(file, line)) {
    line = trim(line);
    if (line.empty() || line.front() == '#') {
      continue;
    }

    InventoryItem item;
    if (deserializeItem(line, item)) {
      items_.push_back(std::move(item));
    }
  }

  if (items_.empty()) {
    items_ = seedInventory();
    return false;
  }

  return true;
#endif
}

bool InventoryStore::save(const std::filesystem::path& path) const {
#ifdef _WIN32
  SqliteConnection connection;
  if (!openDatabase(path, connection)) {
    return false;
  }

  return writeItemsToHimsTable(connection, items_);
#else
  std::filesystem::create_directories(path.parent_path());

  std::ofstream file(path, std::ios::trunc);
  if (!file) {
    return false;
  }

  file << "# HIMS inventory data\n";
  for (const auto& item : items_) {
    file << serializeItem(item) << '\n';
  }
  return true;
#endif
}

InventoryItem* InventoryStore::findById(const std::string& id) {
  const auto it = std::find_if(items_.begin(), items_.end(), [&](const InventoryItem& item) {
    return item.id == id;
  });
  return it == items_.end() ? nullptr : &(*it);
}

const InventoryItem* InventoryStore::findById(const std::string& id) const {
  const auto it = std::find_if(items_.begin(), items_.end(), [&](const InventoryItem& item) {
    return item.id == id;
  });
  return it == items_.end() ? nullptr : &(*it);
}

InventoryItem* InventoryStore::findByCode(const std::string& code) {
  const auto needle = toLower(trim(code));
  const auto it = std::find_if(items_.begin(), items_.end(), [&](const InventoryItem& item) {
    return toLower(item.id) == needle || toLower(item.sku) == needle || toLower(item.digikeyPartNumber) == needle ||
           containsInsensitive(item.productUrl, needle) || containsInsensitive(item.datasheetUrl, needle);
  });
  return it == items_.end() ? nullptr : &(*it);
}

const InventoryItem* InventoryStore::findByCode(const std::string& code) const {
  const auto needle = toLower(trim(code));
  const auto it = std::find_if(items_.begin(), items_.end(), [&](const InventoryItem& item) {
    return toLower(item.id) == needle || toLower(item.sku) == needle || toLower(item.digikeyPartNumber) == needle ||
           containsInsensitive(item.productUrl, needle) || containsInsensitive(item.datasheetUrl, needle);
  });
  return it == items_.end() ? nullptr : &(*it);
}

std::string serializeItem(const InventoryItem& item) {
  std::ostringstream out;
  std::vector<std::string> parameterEntries;
  parameterEntries.reserve(item.parameters.size());
  for (const auto& parameter : item.parameters) {
    parameterEntries.push_back(parameter.name + "=" + parameter.value);
  }

  out << std::quoted(item.id) << '\t' << std::quoted(item.partName) << '\t' << std::quoted(item.manufacturer)
      << '\t' << std::quoted(item.category) << '\t' << item.quantity << '\t' << item.reorderThreshold << '\t'
      << std::quoted(item.location) << '\t' << std::quoted(join(item.tags, '|')) << '\t'
      << std::quoted(join(parameterEntries, ';'))
      << '\t' << std::quoted(item.notes) << '\t' << std::quoted(item.digikeyPartNumber) << '\t'
      << std::quoted(item.datasheetUrl) << '\t' << std::quoted(item.productUrl) << '\t'
      << std::quoted(item.syncStatus) << '\t' << std::quoted(item.sku) << '\t' << item.lastUpdated;
  return out.str();
}

bool deserializeItem(const std::string& line, InventoryItem& item) {
  std::istringstream input(line);
  std::string tags;
  std::string parameters;
  if (!(input >> std::quoted(item.id) >> std::quoted(item.partName) >> std::quoted(item.manufacturer) >>
        std::quoted(item.category) >> item.quantity >> item.reorderThreshold >> std::quoted(item.location) >>
        std::quoted(tags) >> std::quoted(parameters) >> std::quoted(item.notes) >>
        std::quoted(item.digikeyPartNumber) >> std::quoted(item.datasheetUrl) >> std::quoted(item.productUrl) >>
        std::quoted(item.syncStatus) >> std::quoted(item.sku) >> item.lastUpdated)) {
    return false;
  }

  item.tags = split(tags, '|');
  item.parameters.clear();
  for (const auto& entry : split(parameters, ';')) {
    const auto equalsPos = entry.find('=');
    if (equalsPos == std::string::npos) {
      continue;
    }
    item.parameters.push_back({trim(entry.substr(0, equalsPos)), trim(entry.substr(equalsPos + 1))});
  }

  if (item.lastUpdated == 0) {
    item.lastUpdated = nowEpoch();
  }

  return true;
}

std::string serializeActivity(const ActivityEntry& entry) {
  std::ostringstream out;
  out << entry.timestamp << '\t' << std::quoted(entry.kind) << '\t' << std::quoted(entry.message);
  return out.str();
}

bool deserializeActivity(const std::string& line, ActivityEntry& entry) {
  std::istringstream input(line);
  if (!(input >> entry.timestamp >> std::quoted(entry.kind) >> std::quoted(entry.message))) {
    return false;
  }
  return true;
}

bool loadActivities(const std::filesystem::path& path, std::vector<ActivityEntry>& activities) {
  activities.clear();

  std::ifstream file(path);
  if (!file) {
    return false;
  }

  std::string line;
  while (std::getline(file, line)) {
    line = trim(line);
    if (line.empty() || line.front() == '#') {
      continue;
    }

    ActivityEntry entry;
    if (deserializeActivity(line, entry)) {
      activities.push_back(std::move(entry));
    }
  }

  return true;
}

bool saveActivities(const std::filesystem::path& path, const std::vector<ActivityEntry>& activities) {
  std::filesystem::create_directories(path.parent_path());

  std::ofstream file(path, std::ios::trunc);
  if (!file) {
    return false;
  }

  file << "# HIMS activity log\n";
  for (const auto& entry : activities) {
    file << serializeActivity(entry) << '\n';
  }
  return true;
}

void appendActivity(std::vector<ActivityEntry>& activities, const ActivityEntry& entry, std::size_t maxEntries) {
  activities.push_back(entry);
  if (activities.size() > maxEntries) {
    activities.erase(activities.begin(), activities.begin() + static_cast<std::ptrdiff_t>(activities.size() - maxEntries));
  }
}

ActivityEntry makeActivity(std::string kind, std::string message) {
  return {nowEpoch(), std::move(kind), std::move(message)};
}

ScanResolution resolveScanCode(InventoryStore& store, const std::string& rawCode) {
  const auto code = trim(rawCode);
  if (code.empty()) {
    return {false, false, {}, "Empty scan code"};
  }

  if (auto* existing = store.findByCode(code)) {
    return {true, false, existing->id, "Matched existing item"};
  }

  InventoryItem item;
  item.id = sanitizeIdPart(code) + "-" + makeId().substr(0, 8);
  item.partName = "Scanned DigiKey Item";
  item.manufacturer = "Unknown";
  item.category = "Unsorted";
  item.quantity = 0;
  item.reorderThreshold = 0;
  item.location = "Scan Inbox";
  item.tags = {"scanned"};
  item.notes = "Created from a DigiKey code scan.";
  item.digikeyPartNumber = code;
  item.syncStatus = "needs_metadata";
  item.sku = code;
  item.lastUpdated = nowEpoch();
  store.items().push_back(std::move(item));
  return {true, true, store.items().back().id, "Created a placeholder item from the scanned code"};
}

}  // namespace hims
