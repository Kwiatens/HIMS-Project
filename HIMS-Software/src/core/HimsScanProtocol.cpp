// HIMS - Hardware Inventory Management System
// HIMS Scan R1 protocol types, validation, persistence, and stock mutation rules.

#include "core/HimsScanProtocol.h"
#include "core/InventoryInternals.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <fstream>
#include <limits>
#include <optional>
#include <random>
#include <sstream>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <bcrypt.h>
#pragma comment(lib, "Bcrypt.lib")
#endif

namespace hims {

using namespace std;

namespace {

string jsonEscape(const string& value) {
  ostringstream out;
  for (const char ch : value) {
    switch (ch) {
      case '\\': out << "\\\\"; break;
      case '"': out << "\\\""; break;
      case '\n': out << "\\n"; break;
      case '\r': out << "\\r"; break;
      case '\t': out << "\\t"; break;
      default: out << ch; break;
    }
  }
  return out.str();
}

optional<string> jsonString(const string& body, const string& key) {
  const auto marker = string("\"") + key + "\"";
  auto position = body.find(marker);
  if (position == string::npos) return nullopt;
  position = body.find(':', position + marker.size());
  if (position == string::npos) return nullopt;
  position = body.find('"', position + 1);
  if (position == string::npos) return nullopt;
  string value;
  bool escaped = false;
  for (++position; position < body.size(); ++position) {
    const char ch = body[position];
    if (escaped) {
      switch (ch) {
        case 'n': value.push_back('\n'); break;
        case 'r': value.push_back('\r'); break;
        case 't': value.push_back('\t'); break;
        default: value.push_back(ch); break;
      }
      escaped = false;
    } else if (ch == '\\') {
      escaped = true;
    } else if (ch == '"') {
      return value;
    } else {
      value.push_back(ch);
    }
  }
  return nullopt;
}

optional<int> jsonInt(const string& body, const string& key) {
  const auto marker = string("\"") + key + "\"";
  auto position = body.find(marker);
  if (position == string::npos) return nullopt;
  position = body.find(':', position + marker.size());
  if (position == string::npos) return nullopt;
  ++position;
  while (position < body.size() && isspace(static_cast<unsigned char>(body[position]))) ++position;
  const auto begin = position;
  if (position < body.size() && (body[position] == '-' || body[position] == '+')) ++position;
  while (position < body.size() && isdigit(static_cast<unsigned char>(body[position]))) ++position;
  if (position == begin || (position == begin + 1 && (body[begin] == '-' || body[begin] == '+'))) return nullopt;
  try {
    const auto parsed = stoll(body.substr(begin, position - begin));
    if (parsed < numeric_limits<int>::min() || parsed > numeric_limits<int>::max()) return nullopt;
    return static_cast<int>(parsed);
  } catch (...) {
    return nullopt;
  }
}

string hexToken(const array<unsigned char, 32>& bytes) {
  static constexpr char kHex[] = "0123456789abcdef";
  string result;
  result.reserve(bytes.size() * 2);
  for (const auto byte : bytes) {
    result.push_back(kHex[(byte >> 4U) & 0x0fU]);
    result.push_back(kHex[byte & 0x0fU]);
  }
  return result;
}

bool looksLikeSupportedHimsScanCode(const string& code) {
  const auto trimmed = trim(code);
  return !trimmed.empty() &&
         all_of(trimmed.begin(), trimmed.end(), [](unsigned char ch) { return isdigit(ch) != 0; });
}

}  // namespace

bool HimsScanConfig::paired() const {
  return !trim(token).empty() && token.size() >= 32;
}

bool loadHimsScanConfig(const filesystem::path& path, HimsScanConfig& config) {
  ifstream input(path);
  if (!input) return false;
  HimsScanConfig loaded;
  string line;
  while (getline(input, line)) {
    const auto separator = line.find('=');
    if (separator == string::npos) continue;
    const auto key = trim(line.substr(0, separator));
    const auto value = trim(line.substr(separator + 1));
    if (key == "device_id") loaded.deviceId = value;
    else if (key == "token") loaded.token = value;
    else if (key == "fallback_host" || key == "server_host") loaded.fallbackHost = value;
    else if (key == "fallback_port" || key == "server_port") {
      try {
        const auto port = stoul(value);
        if (port <= 65535) loaded.fallbackPort = static_cast<uint16_t>(port);
      } catch (...) {
      }
    }
  }
  config = move(loaded);
  return true;
}

bool saveHimsScanConfig(const filesystem::path& path, const HimsScanConfig& config) {
  error_code error;
  filesystem::create_directories(path.parent_path(), error);
  ofstream output(path, ios::trunc);
  if (!output) return false;
  output << "device_id=" << config.deviceId << '\n'
         << "token=" << config.token << '\n'
         << "fallback_host=" << config.fallbackHost << '\n'
         << "fallback_port=" << config.fallbackPort << '\n';
  return static_cast<bool>(output);
}

string generateHimsScanToken() {
  array<unsigned char, 32> bytes{};
#ifdef _WIN32
  if (BCryptGenRandom(nullptr, bytes.data(), static_cast<ULONG>(bytes.size()), BCRYPT_USE_SYSTEM_PREFERRED_RNG) == 0) {
    return hexToken(bytes);
  }
#endif
  random_device source;
  for (auto& byte : bytes) byte = static_cast<unsigned char>(source());
  return hexToken(bytes);
}

bool parseQuantityRequestJson(const string& body, DeviceQuantityRequest& request, string& error) {
  const auto deviceId = jsonString(body, "deviceId");
  const auto requestId = jsonString(body, "requestId");
  const auto code = jsonString(body, "code");
  const auto delta = jsonInt(body, "delta");
  if (!deviceId || trim(*deviceId).empty() || !requestId || trim(*requestId).empty() || !code || !delta) {
    error = "Missing or invalid deviceId, requestId, code, or delta";
    return false;
  }
  if (requestId->size() > 96 || deviceId->size() > 96 || code->size() > 128) {
    error = "Request field is too long";
    return false;
  }
  if (*delta == 0 || *delta < -999999 || *delta > 999999) {
    error = "Delta must be between -999999 and 999999 and cannot be zero";
    return false;
  }
  request = {*deviceId, *requestId, *code, *delta};
  return true;
}

bool parseStatusReportJson(const string& body, DeviceStatusReport& report, string& error) {
  const auto deviceId = jsonString(body, "deviceId");
  const auto version = jsonString(body, "firmwareVersion");
  const auto rssi = jsonInt(body, "rssi");
  if (!deviceId || trim(*deviceId).empty() || !version || !rssi) {
    error = "Missing or invalid deviceId, firmwareVersion, or rssi";
    return false;
  }
  report = {*deviceId, *version, *rssi};
  return true;
}

DeviceQuantityResult applyDeviceQuantity(InventoryStore& store, const DeviceQuantityRequest& request) {
  DeviceQuantityResult result;
  result.requestedDelta = request.delta;
  const auto code = trim(request.code);
  if (!looksLikeSupportedHimsScanCode(code)) {
    result.httpStatus = 400;
    result.error = "Only numeric machine codes can change stock";
    return result;
  }

  auto& items = store.items();
  const auto itemIt = find_if(items.begin(), items.end(), [&](const InventoryItem& item) {
    return matchesMachineCode(item.machineCode, code);
  });

  if (itemIt == items.end()) {
    result.httpStatus = 404;
    result.error = "Unknown machine code";
    return result;
  }

  auto* item = &*itemIt;
  const auto oldQuantity = item->quantity;
  const long long candidate = static_cast<long long>(oldQuantity) + request.delta;
  const auto newQuantity = static_cast<int>(clamp<long long>(candidate, 0, numeric_limits<int>::max()));
  item->quantity = newQuantity;
  item->lastUpdated = time(nullptr);
  result.httpStatus = 200;
  result.ok = true;
  result.item = item->partName;
  result.appliedDelta = newQuantity - oldQuantity;
  result.quantity = newQuantity;
  return result;
}

DeviceQuantityResult applyDeviceQuantityCached(InventoryStore& store, const DeviceQuantityRequest& request,
                                               unordered_map<string, DeviceQuantityResult>& cache,
                                               deque<string>& order, size_t maxEntries) {
  const auto cached = cache.find(request.requestId);
  if (cached != cache.end()) {
    return cached->second;
  }

  const auto result = applyDeviceQuantity(store, request);
  cache[request.requestId] = result;
  order.push_back(request.requestId);
  while (order.size() > maxEntries) {
    cache.erase(order.front());
    order.pop_front();
  }
  return result;
}

string quantityResultJson(const DeviceQuantityResult& result) {
  ostringstream out;
  out << "{\"ok\":" << (result.ok ? "true" : "false");
  if (result.ok) {
    out << ",\"item\":\"" << jsonEscape(result.item) << "\""
        << ",\"requestedDelta\":" << result.requestedDelta
        << ",\"appliedDelta\":" << result.appliedDelta
        << ",\"quantity\":" << result.quantity;
  } else {
    out << ",\"error\":\"" << jsonEscape(result.error) << "\"";
  }
  out << '}';
  return out.str();
}

string statusResultJson(bool ok, const string& error) {
  return ok ? string("{\"ok\":true}")
            : string("{\"ok\":false,\"error\":\"") + jsonEscape(error) + "\"}";
}

}  // namespace hims
