// HIMS - Hardware Inventory Management System
// Detail page rendering and keyboard handling.

#include "App.h"

#include "ui/shared/AppUiShared.h"

#include <algorithm>
#include <cctype>
#include <sstream>
#include <string>
#include <vector>

namespace hims {

using namespace std;

ftxui::Element App::renderDetailUi() const {
  ftxui::Elements leftRows;
  ftxui::Elements rightRows;

  if (const auto* item = selectedItem()) {
    const auto electricalFields = electricalFieldsForItem(*item);
    leftRows.push_back(fullLine("Core details", uiAccentColor(), uiPanelLeftBg()));
    const auto coreFields = detailCoreFields(*item, rackLocation(*item, store_.racks()));
    for (size_t index = 0; index < coreFields.size(); ++index) {
      if (index == 0) {
        leftRows.push_back(fullLine(coreFields[index].label + coreFields[index].value, uiTitleColor(), uiRowSelectedBg()));
      } else {
        leftRows.push_back(detailFieldLine(coreFields[index], 40));
      }
    }
    if (!electricalFields.empty()) {
      leftRows.push_back(ftxui::separator());
      leftRows.push_back(fullLine("Electrical parameters", uiAccentColor(), uiPanelLeftBg()));
      for (const auto& field : electricalFields) {
        leftRows.push_back(detailFieldLine(field, 40));
      }
    }
    if (!item->tags.empty()) {
      leftRows.push_back(ftxui::separator());
      leftRows.push_back(detailFieldLine({"Tags: ", renderTags(item->tags), uiLabelColor(), uiTitleColor()}, 40));
    }

    rightRows.push_back(fullLine("Metadata", uiAccentColor(), uiPanelRightBg()));
    rightRows.push_back(bulletLine("DigiKey: ", renderUrl(item->digikeyPartNumber), uiLinkColor(), uiTitleColor()));
    rightRows.push_back(bulletLine("Datasheet: ", renderUrl(item->datasheetUrl), uiLinkColor(), uiTitleColor()));
    rightRows.push_back(bulletLine("Product: ", renderUrl(item->productUrl), uiLinkColor(), uiTitleColor()));
    rightRows.push_back(bulletLine("SKU: ", renderUrl(item->sku), uiLabelColor(), uiTitleColor()));
    rightRows.push_back(bulletLine("Status: ", item->syncStatus, uiSuccessColor(), uiTitleColor()));
    rightRows.push_back(bulletLine("Updated: ", nowTimestampString(item->lastUpdated), uiMutedColor(), uiTitleColor()));
    rightRows.push_back(ftxui::separator());
    rightRows.push_back(fullLine("Notes", uiWarnColor(), uiPanelRightBg()));
    rightRows.push_back(ftxui::paragraphAlignLeft(item->notes) | ftxui::color(uiMutedColor()));
  } else {
    leftRows.push_back(fullLine("No item selected.", uiMutedColor(), uiPanelLeftBg()));
    rightRows.push_back(fullLine("Press Esc to return to stock.", uiMutedColor(), uiPanelRightBg()));
  }

  auto leftPanel = panel("Item detail", move(leftRows), uiAccentColor()) | ftxui::bgcolor(uiPanelLeftBg()) | ftxui::flex;
  auto rightPanel = panel("Links and notes", move(rightRows), uiAccentColor()) | ftxui::bgcolor(uiPanelRightBg()) | ftxui::flex;

  return ftxui::hbox({
      leftPanel,
      ftxui::separator(),
      rightPanel,
  });
}

void App::handleDetailKey(const KeyEvent& key) {
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
    switch (ch) {
      case 'e':
        beginEditCurrentItem(false);
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
      case 's':
        openUrl(scannerUrl());
        setMessage("Opened scanner page in the default browser", 3);
        break;
      case 'p':
        printSelectedLabel();
        break;
      case 'j':
        moveSelection(1);
        break;
      case 'k':
        moveSelection(-1);
        break;
      case '/':
        startSearch();
        break;
      default:
        break;
    }
    return;
  }

  if (key.type == KeyType::Escape) {
    changePage(Page::Stock);
  }
}

void App::renderDetail(ostringstream& out, const ConsoleSize& size) {
  const int contentRows = max(8, size.rows - 6);

  out << kColorAccent << "Item Detail" << kColorReset << "  ";
  out << kColorMuted << "Full record and links" << kColorReset << '\n';
  out << string(max(0, size.columns), '-') << '\n';

  if (size.columns < 96) {
    vector<string> lines;
    if (const auto* item = selectedItem()) {
      const auto electricalFields = electricalFieldsForItem(*item);
      lines.push_back(styleText("Core details", kColorAccent));
      for (const auto& field : detailCoreFields(*item, rackLocation(*item, store_.racks()))) {
        lines.push_back(styleText(field.label + field.value, kColorTitle));
      }
      if (!electricalFields.empty()) {
        lines.push_back("");
        lines.push_back(styleText("Electrical parameters", kColorAccent));
        for (const auto& field : electricalFields) {
          lines.push_back(styleText(field.label + field.value, kColorTitle));
        }
      }
      if (!item->tags.empty()) {
        lines.push_back("");
        lines.push_back(styleText("Tags: " + renderTags(item->tags), kColorLabel));
      }
      lines.push_back("");
      lines.push_back(styleText("Metadata", kColorAccent));
      lines.push_back(styleText("  DigiKey: " + renderUrl(item->digikeyPartNumber), kColorLink));
      lines.push_back(styleText("  Datasheet: " + renderUrl(item->datasheetUrl), kColorLink));
      lines.push_back(styleText("  Product: " + renderUrl(item->productUrl), kColorLink));
      lines.push_back(styleText("  SKU: " + renderUrl(item->sku), kColorLabel));
      lines.push_back(styleText("  Status: " + item->syncStatus, kColorSuccess));
      lines.push_back(styleText("  Updated: " + nowTimestampString(item->lastUpdated), kColorMuted));
      lines.push_back("");
      lines.push_back(styleText("Notes", kColorWarn));
      appendWrappedStyled(lines, item->notes, max(50, size.columns - 4), kColorMuted);
    } else {
      lines.push_back(styleText("No item selected.", kColorMuted));
    }
    renderColumns(out, lines, {}, max(50, size.columns - 4), 0, contentRows);
    return;
  }

  const int rightWidth = clamp(size.columns / 3, 42, 58);
  const int leftWidth = max(40, size.columns - rightWidth - 4);

  vector<string> leftLines;
  vector<string> rightLines;

  if (const auto* item = selectedItem()) {
    const auto electricalFields = electricalFieldsForItem(*item);
    leftLines.push_back("Core details");
    for (const auto& field : detailCoreFields(*item, rackLocation(*item, store_.racks()))) {
      leftLines.push_back(field.label + field.value);
    }
    if (!electricalFields.empty()) {
      leftLines.push_back("");
      leftLines.push_back("Electrical parameters");
      for (const auto& field : electricalFields) {
        leftLines.push_back(field.label + field.value);
      }
    }
    if (!item->tags.empty()) {
      leftLines.push_back("");
      leftLines.push_back("Tags: " + renderTags(item->tags));
    }

    rightLines.push_back("Metadata");
    rightLines.push_back("DigiKey: " + renderUrl(item->digikeyPartNumber));
    rightLines.push_back("Datasheet: " + renderUrl(item->datasheetUrl));
    rightLines.push_back("Product: " + renderUrl(item->productUrl));
    rightLines.push_back("SKU: " + renderUrl(item->sku));
    rightLines.push_back("Status: " + item->syncStatus);
    rightLines.push_back("Updated: " + nowTimestampString(item->lastUpdated));
    rightLines.push_back("");
    rightLines.push_back("Notes");
    appendWrapped(rightLines, item->notes, rightWidth);
  } else {
    leftLines.push_back("No item selected.");
    rightLines.push_back("Press Esc to return to stock.");
  }

  renderColumns(out, leftLines, rightLines, leftWidth, rightWidth, contentRows);
}

}  // namespace hims

