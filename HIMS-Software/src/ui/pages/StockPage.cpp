// HIMS - Hardware Inventory Management System
// Stock browser page rendering and keyboard handling.

#include "App.h"

#include "ui/shared/AppUiShared.h"

#include <algorithm>
#include <cctype>
#include <ctime>
#include <sstream>
#include <string>
#include <vector>

#include <ftxui/component/screen_interactive.hpp>

namespace hims {

using namespace std;

ftxui::Element App::renderStockUi() const {
  const auto filtered = filteredIndices();
  const auto* activeScreen = ftxui::ScreenInteractive::Active();
  const int screenWidth = activeScreen != nullptr ? activeScreen->dimx() : 120;
  const int detailOuterWidth = clamp(screenWidth / 3, 42, 60);
  const int listOuterWidth = max(42, screenWidth - detailOuterWidth - 1);
  const int listInnerWidth = max(20, listOuterWidth - 2);
  const int detailInnerWidth = max(20, detailOuterWidth - 2);
  const int qtyWidth = 10;
  int partWidth = clamp(listInnerWidth / 3, 22, 34);
  int categoryWidth = listInnerWidth - partWidth - qtyWidth - 2;
  if (categoryWidth < 12) {
    categoryWidth = 12;
    partWidth = max(18, listInnerWidth - categoryWidth - qtyWidth - 2);
  }
  if (partWidth < 18) {
    partWidth = 18;
  }

  auto fixedCell = [](const string& text, int width, ftxui::Color color) {
    return ftxui::hbox({
               styledText(ellipsize(text, static_cast<size_t>(max(width, 0))), color),
               ftxui::filler(),
           }) |
           ftxui::size(ftxui::WIDTH, ftxui::EQUAL, width);
  };

  ftxui::Elements listRows;
  listRows.push_back(ftxui::hbox({
                         fixedCell("Part", partWidth, uiMutedColor()),
                         ftxui::separator() | ftxui::color(uiDimColor()),
                         fixedCell("Category", categoryWidth, uiMutedColor()),
                         ftxui::filler(),
                         ftxui::hbox({
                             ftxui::filler(),
                             styledText("Qty", uiMutedColor()),
                         }) |
                             ftxui::size(ftxui::WIDTH, ftxui::EQUAL, qtyWidth),
                     }) |
                     ftxui::bgcolor(uiPanelLeftBg()));

  if (filtered.empty()) {
    listRows.push_back(fullLine("No items match \"" + searchQuery_ + "\".", uiMutedColor(), uiPanelLeftBg()));
  } else {
    for (size_t index = 0; index < filtered.size(); ++index) {
      const auto& item = store_.items()[filtered[index]];
      const bool selected = index == selectedPosition_;
      const bool lowStock = item.lowStock();
      const auto bg = selected ? uiRowSelectedBg() : (index % 2 == 0 ? uiRowDarkBg() : uiRowLightBg());
      const auto fg = selected ? uiTitleColor() : (lowStock ? uiWarnColor() : uiMutedColor());
      const auto category = displayCategory(item.category);
      auto row = ftxui::hbox({
                     fixedCell("  " + item.partName, partWidth, fg),
                     ftxui::separator() | ftxui::color(uiDimColor()),
                     fixedCell(category, categoryWidth, selected ? uiTitleColor() : uiLabelColor()),
                     ftxui::filler(),
                     ftxui::hbox({
                         ftxui::filler(),
                         quantityBadge(item.quantity, selected),
                     }) |
                         ftxui::size(ftxui::WIDTH, ftxui::EQUAL, qtyWidth),
                 }) |
                 ftxui::bgcolor(bg);
      if (selected) {
        row = row | ftxui::select;
      }
      listRows.push_back(row);
    }
  }

  ftxui::Elements detailRows;
  if (const auto* item = selectedItem()) {
    detailRows.push_back(fullLine("Part summary", uiAccentColor(), uiPanelRightBg()));
    const auto electricalFields = electricalFieldsForItem(*item);
    const auto previewFields = stockPreviewFields(*item, rackLocation(*item, store_.racks()));
    for (size_t index = 0; index < previewFields.size(); ++index) {
      const auto& field = previewFields[index];
      if (index == 0) {
        detailRows.push_back(fullLine(field.label + field.value, uiTitleColor(), uiRowSelectedBg()));
        continue;
      }
      if (index == 4 && !electricalFields.empty()) {
        detailRows.push_back(ftxui::separator());
        detailRows.push_back(fullLine("Electrical parameters", uiAccentColor(), uiPanelRightBg()));
      }
      detailRows.push_back(detailFieldLine(field, detailInnerWidth));
    }
  } else {
    detailRows.push_back(fullLine("No item selected.", uiMutedColor(), uiPanelRightBg()));
  }

  auto listContent = ftxui::vbox(move(listRows)) | ftxui::yframe | ftxui::vscroll_indicator;
  auto listPanel = ftxui::window(styledText("Items", uiAccentColor()), move(listContent)) |
                   ftxui::bgcolor(uiPanelLeftBg()) | ftxui::size(ftxui::WIDTH, ftxui::EQUAL, listOuterWidth);
  auto detailPanel = panel("Detail", move(detailRows), uiAccentColor()) | ftxui::bgcolor(uiPanelRightBg()) |
                     ftxui::size(ftxui::WIDTH, ftxui::EQUAL, detailOuterWidth);

  auto page = ftxui::hbox({
      listPanel,
      ftxui::separator() | ftxui::color(uiDimColor()),
      detailPanel,
  });

  if (!deleteConfirmationActive()) {
    return page;
  }

  const auto* item = store_.findById(deleteConfirmationItemId_);
  const auto itemLabel = item == nullptr ? string("this part") : item->partName;
  const auto secondsLeft = deleteConfirmationSecondsLeft();
  const int popupWidth = max(48, min(screenWidth - 12, 72));

  ftxui::Elements popupRows;
  popupRows.push_back(ftxui::paragraphAlignLeft("Are you sure you want to delete this part from the database?") |
                      ftxui::color(uiTitleColor()));
  popupRows.push_back(ftxui::separator());
  popupRows.push_back(ftxui::paragraphAlignLeft("Selected: " + itemLabel) | ftxui::color(uiWarnColor()));
  popupRows.push_back(ftxui::paragraphAlignLeft("Press Enter to confirm after the timer unlocks.") |
                      ftxui::color(secondsLeft == 0 ? uiTitleColor() : uiMutedColor()));
  popupRows.push_back(ftxui::paragraphAlignLeft("Press Esc to cancel.") | ftxui::color(uiMutedColor()));
  popupRows.push_back(ftxui::paragraphAlignLeft(to_string(secondsLeft) + " second" +
                                                (secondsLeft == 1 ? string() : string("s")) + " remaining") |
                      ftxui::color(uiWarnColor()));

  auto popup = ftxui::window(styledText("Delete item", uiDangerColor()),
                             ftxui::vbox(move(popupRows)) | ftxui::bgcolor(uiPanelRightBg()) |
                                 ftxui::size(ftxui::WIDTH, ftxui::LESS_THAN, popupWidth)) |
               ftxui::color(uiDangerColor());

  auto overlay = ftxui::vbox({
      ftxui::filler(),
      ftxui::hbox({
          ftxui::filler(),
          popup,
          ftxui::filler(),
      }),
      ftxui::filler(),
  });

  return ftxui::dbox({
      page,
      overlay,
  });
}

void App::handleStockKey(const KeyEvent& key) {
  if (key.type == KeyType::CtrlZ) {
    undoLastInventoryChange();
    return;
  }
  if (key.type == KeyType::Character) {
    const auto ch = tolower(static_cast<unsigned char>(key.ch));
    if (ch == 'u') {
      undoLastInventoryChange();
      return;
    }
    if (ch == 'h') {
      chooseHimsFolder();
      return;
    }
    if (ch == 'q') {
      running_ = false;
      return;
    }
  }

  if (deleteConfirmationActive()) {
    if (key.type == KeyType::Enter) {
      confirmDeleteSelectedItem();
      return;
    }
    if (key.type == KeyType::Escape) {
      cancelDeleteConfirmation();
      return;
    }

    cancelDeleteConfirmation();
  } else if (key.type == KeyType::Backspace) {
    return;
  }

  if (key.type == KeyType::CtrlBackspace) {
    armDeleteConfirmation();
    return;
  }

  if (key.type == KeyType::Tab) {
    changePage(Page::Dashboard);
    return;
  }

  if (key.type == KeyType::Character) {
    const auto ch = tolower(static_cast<unsigned char>(key.ch));
    switch (ch) {
      case 'j':
        moveSelection(1);
        break;
      case 'k':
        moveSelection(-1);
        break;
      case '/':
        startSearch();
        break;
      case 'e':
        beginEditCurrentItem(false);
        break;
      case 'n':
        beginEditCurrentItem(true);
        break;
      case 'm':
        openRackManagement();
        break;
      case '+':
        adjustQuantity(1);
        break;
      case '-':
        adjustQuantity(-1);
        break;
      case 'd':
        if (const auto* item = selectedItem()) {
          openCurrentUrl(item->datasheetUrl, "datasheet");
        }
        break;
      case 'o':
        if (const auto* item = selectedItem()) {
          openCurrentUrl(item->productUrl, "product");
        }
        break;
      case 'g':
        if (const auto* item = selectedItem()) {
          const auto digiKeySearch = item->digikeyPartNumber.empty()
                                         ? string()
                                         : "https://www.digikey.com/en/products/result?keywords=" + item->digikeyPartNumber;
          openCurrentUrl(digiKeySearch, "DigiKey");
        }
        break;
      case 'r':
        store_.load(inventoryPath_);
        loadInventoryHistory(inventoryPath_, inventoryHistory_);
        if (inventoryHistory_.empty()) {
          appendInventoryHistory(inventoryHistory_, makeInventoryHistoryPoint(store_.items()));
        }
        saveInventoryHistory(inventoryPath_, inventoryHistory_);
        syncSelectionToFilter();
        setMessage("Inventory refreshed", 2);
        break;
      case 's':
        openUrl(scannerUrl());
        setMessage("Opened scanner page in the default browser", 3);
        break;
      case 'p':
        printSelectedLabel();
        break;
      default:
        break;
    }
    return;
  }

  if (key.type == KeyType::Up) {
    moveSelection(-1);
  } else if (key.type == KeyType::Down) {
    moveSelection(1);
  } else if (key.type == KeyType::PageUp) {
    moveSelection(-10);
  } else if (key.type == KeyType::PageDown) {
    moveSelection(10);
  } else if (key.type == KeyType::Home) {
    selectedPosition_ = 0;
    syncSelectionToFilter();
  } else if (key.type == KeyType::End) {
    const auto filtered = filteredIndices();
    if (!filtered.empty()) {
      selectedPosition_ = filtered.size() - 1;
      syncSelectionToFilter();
    }
  } else if (key.type == KeyType::Enter) {
    openSelectedDetail();
  } else if (key.type == KeyType::Escape) {
    changePage(Page::Dashboard);
  }
}

void App::renderStock(ostringstream& out, const ConsoleSize& size) {
  const auto filtered = filteredIndices();
  const int contentRows = max(8, size.rows - 6);

  out << kColorAccent << "Stock Browser" << kColorReset << "  ";
  out << kColorMuted << "Searchable inventory and details" << kColorReset << '\n';
  out << string(max(0, size.columns), '-') << '\n';

  if (size.columns < 96) {
    vector<string> lines;
    lines.push_back(styleText("Items", kColorAccent));
    if (filtered.empty()) {
      lines.push_back(styleText("  No items match \"" + searchQuery_ + "\".", kColorMuted));
    } else {
      for (size_t index = 0; index < filtered.size() && lines.size() < 18; ++index) {
        const auto& item = store_.items()[filtered[index]];
        const auto prefix = index == selectedPosition_ ? "> " : "  ";
        lines.push_back(styleText(prefix + item.partName + "  [" + displayCategory(item.category) + "]", index == selectedPosition_ ? kColorTitle : kColorMuted));
        lines.push_back(styleText("   qty " + to_string(item.quantity), item.lowStock() ? kColorWarn : kColorSuccess));
      }
    }
    lines.push_back("");
    lines.push_back(styleText("Detail", kColorAccent));
    if (const auto* item = selectedItem()) {
      const auto previewFields = stockPreviewFields(*item, rackLocation(*item, store_.racks()));
      for (size_t index = 0; index < previewFields.size() && lines.size() < 38; ++index) {
        lines.push_back(styleText(previewFields[index].label + previewFields[index].value, kColorTitle));
      }
    } else {
      lines.push_back(styleText("  No item selected.", kColorMuted));
    }
    renderColumns(out, lines, {}, max(50, size.columns - 4), 0, contentRows);
    return;
  }

  const int listWidth = max(40, size.columns / 2 - 2);
  const int detailWidth = max(36, size.columns - listWidth - 4);

  vector<string> leftLines;
  leftLines.push_back("Part");
  if (filtered.empty()) {
    leftLines.push_back("No items match \"" + searchQuery_ + "\".");
  } else {
    for (size_t index = 0; index < filtered.size(); ++index) {
      const auto& item = store_.items()[filtered[index]];
      const auto prefix = index == selectedPosition_ ? "> " : "  ";
      leftLines.push_back(prefix + item.partName + "  [" + displayCategory(item.category) + "]  qty " +
                          to_string(item.quantity));
    }
  }

  vector<string> rightLines;
  if (const auto* item = selectedItem()) {
    rightLines.push_back("Part summary");
    const auto electricalFields = electricalFieldsForItem(*item);
    const auto previewFields = stockPreviewFields(*item, rackLocation(*item, store_.racks()));
    for (size_t index = 0; index < previewFields.size(); ++index) {
      if (index == 4 && !electricalFields.empty()) {
        rightLines.push_back("");
        rightLines.push_back("Electrical parameters");
      }
      rightLines.push_back(previewFields[index].label + previewFields[index].value);
    }
  } else {
    rightLines.push_back("No item selected.");
  }

  renderColumns(out, leftLines, rightLines, listWidth, detailWidth, contentRows);
}

}  // namespace hims

