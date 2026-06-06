#include "core/Inventory.h"

#include <cstdlib>
#include <cassert>
#include <filesystem>
#include <iostream>

using namespace hims;

int main() {
  {
    auto items = seedInventory();
    assert(!items.empty());

    const auto filtered = filterItems(items, "cat:resistors qty>100");
    assert(filtered.size() == 1);
    assert(items[filtered[0]].id == "res-0603-10k");

    const auto tagFiltered = filterItems(items, "tag:module param:Flash=16MB");
    assert(tagFiltered.size() == 1);
    assert(items[tagFiltered[0]].id == "esp32-s3-module");
  }

  {
    auto items = seedInventory();
    InventoryStore store;
    store.items() = items;

    const auto resolution = resolveScanCode(store, "311-10.0KHRCT-ND");
    assert(resolution.matched);
    assert(!resolution.created);
    assert(resolution.itemId == "res-0603-10k");

    const auto created = resolveScanCode(store, "new-digikey-code");
    assert(created.matched);
    assert(created.created);
    assert(store.findById(created.itemId) != nullptr);
  }

  {
    InventoryItem item;
    item.id = "abc";
    item.partName = "Test Part";
    item.manufacturer = "Acme";
    item.category = "Test";
    item.quantity = 2;
    item.reorderThreshold = 1;
    item.location = "Bin 1";
    item.tags = {"alpha", "beta"};
    item.parameters = {{"Voltage", "5V"}, {"Package", "0805"}};
    item.notes = "Roundtrip test";
    item.digikeyPartNumber = "123";
    item.datasheetUrl = "https://example.com/datasheet";
    item.productUrl = "https://example.com/product";
    item.syncStatus = "synced";
    item.sku = "SKU-1";
    item.lastUpdated = 1710000000;

    const auto line = serializeItem(item);
    InventoryItem restored;
    assert(deserializeItem(line, restored));
    assert(restored.partName == item.partName);
    assert(restored.parameters.size() == 2);
    assert(restored.tags.size() == 2);
  }

  {
    const char* profile = std::getenv("USERPROFILE");
    if (profile != nullptr && *profile != '\0') {
      const auto dbPath = std::filesystem::path(profile) / "Documents" / "HIMS" / "inventory.db";
      if (std::filesystem::exists(dbPath)) {
        InventoryStore store;
        const auto loaded = store.load(dbPath);
        if (!loaded) {
          return 1;
        }
        if (store.items().size() <= 5) {
          return 1;
        }
      }
    }
  }

  std::cout << "HIMS core tests passed\n";
  return 0;
}
