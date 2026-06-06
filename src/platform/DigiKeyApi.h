#pragma once

#include "core/Inventory.h"

#include <filesystem>
#include <ctime>
#include <optional>
#include <string>
#include <vector>

namespace hims {

struct DigiKeyConfig {
  std::string clientId;
  std::string clientSecret;
  std::string accountId;
  std::string site = "US";
  std::string language = "en";
  std::string currency = "USD";

  bool valid() const;
};

struct DigiKeyProductDetails {
  std::string lookupKey;
  std::string manufacturerName;
  std::string manufacturerPartNumber;
  std::string productDescription;
  std::string detailedDescription;
  std::string productUrl;
  std::string datasheetUrl;
  std::string packagingType;
  std::string packageName;
  std::string rohsStatus;
  std::string leadStatus;
  std::string productStatus;
  std::string manufacturerLeadWeeks;
  std::string quantityAvailable;
  std::string unitPrice;
  std::vector<Parameter> parameters;
};

bool loadEnvironmentFile(const std::filesystem::path& path);
DigiKeyConfig loadDigiKeyConfig();

class DigiKeyApiClient {
 public:
  explicit DigiKeyApiClient(DigiKeyConfig config);

  std::optional<DigiKeyProductDetails> fetchProductDetails(const std::string& productNumber,
                                                           std::string* error = nullptr);

 private:
  bool ensureAccessToken(std::string* error);
  std::optional<std::string> requestToken(std::string* error);
  std::optional<std::string> requestProductDetails(const std::string& productNumber, std::string* error,
                                                   const std::string& manufacturerId = "");
  std::optional<std::string> requestKeywordSearch(const std::string& keywords, std::string* error);

  DigiKeyConfig config_;
  std::string accessToken_;
  std::time_t tokenExpiresAt_ = 0;
};

}  // namespace hims
