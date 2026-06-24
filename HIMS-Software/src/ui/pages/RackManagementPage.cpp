// HIMS - Hardware Inventory Management System
// HIMS Rack management page rendering and keyboard handling.

#include "App.h"

#include "ui/shared/AppUiShared.h"

#include <algorithm>
#include <cctype>
#include <sstream>
#include <string>
#include <vector>

#include <ftxui/component/screen_interactive.hpp>

namespace hims {

using namespace std;

namespace {

ftxui::Element rackFixedCell(const string& text, int width, ftxui::Color color, bool rightAlign = false) {
  auto content = rightAlign
                     ? ftxui::hbox({ftxui::filler(), styledText(ellipsize(text, static_cast<size_t>(max(0, width))), color)})
                     : ftxui::hbox({styledText(ellipsize(text, static_cast<size_t>(max(0, width))), color), ftxui::filler()});
  return content | ftxui::size(ftxui::WIDTH, ftxui::EQUAL, width);
}

string assignmentLabel(RackAssignmentMode mode) {
  if (mode == RackAssignmentMode::Manual) return "manual";
  if (mode == RackAssignmentMode::Unassigned) return "unassigned";
  return "automatic";
}

string packageSummary(const InventoryItem& item) {
  for (const auto& parameter : item.parameters) {
    if (parameterLabelMatches(parameter.name, "Package") || parameterLabelMatches(parameter.name, "Package / Case")) {
      return parameter.value;
    }
  }
  return "-";
}

}  // namespace

ftxui::Element App::renderRackManagementUi() const {
  const auto* activeScreen = ftxui::ScreenInteractive::Active();
  const int screenWidth = activeScreen != nullptr ? activeScreen->dimx() : 120;
  const bool compact = screenWidth < 118;
  const int listWidth = compact ? 30 : 34;
  const int detailWidth = compact ? 34 : 42;
  const int gridWidth = max(42, screenWidth - listWidth - detailWidth - 4);
  const auto rackIndices = sortedRackIndices();
  const auto* rack = selectedRack();
  const auto selectedSlot = selectedRackSlot();
  const auto* selectedSlotItem = selectedRackItem();

  ftxui::Elements rackRows;
  rackRows.push_back(ftxui::hbox({
      rackFixedCell("Rack", 6, uiMutedColor()),
      rackFixedCell("Type", max(8, listWidth - 18), uiMutedColor()),
      rackFixedCell("Used", 7, uiMutedColor(), true),
  }) | ftxui::bgcolor(uiPanelLeftBg()));
  if (rackIndices.empty()) {
    rackRows.push_back(fullLine(rackFilter_.empty() ? "No racks yet." : "No racks match filter.", uiMutedColor(),
                                uiPanelLeftBg()));
  } else {
    for (size_t visible = 0; visible < rackIndices.size(); ++visible) {
      const auto& candidate = store_.racks()[rackIndices[visible]];
      const bool selected = visible == min(rackSelection_, rackIndices.size() - 1);
      const auto bg = selected ? uiRowSelectedBg() : (visible % 2 == 0 ? uiRowDarkBg() : uiRowLightBg());
      const auto fg = selected ? uiTitleColor() : uiMutedColor();
      const auto occupied = rackOccupiedSlotCount(store_, candidate);
      rackRows.push_back(ftxui::hbox({
          rackFixedCell(" " + candidate.code, 6, fg),
          rackFixedCell(candidate.componentType, max(8, listWidth - 18), selected ? uiTitleColor() : uiLabelColor()),
          rackFixedCell(to_string(occupied) + "/25", 7, occupied >= 25 ? uiWarnColor() : uiSuccessColor(), true),
      }) | ftxui::bgcolor(bg));
    }
  }

  ftxui::Elements gridRows;
  if (rack == nullptr) {
    gridRows.push_back(fullLine("Eligible inventory will create racks automatically.", uiMutedColor(), uiPanelRightBg()));
  } else {
    gridRows.push_back(fullLine(rack->code + "  " + rack->componentType + "  " +
                                    to_string(rackOccupiedSlotCount(store_, *rack)) + "/25 occupied",
                                uiAccentColor(), uiPanelRightBg()));
    if (!rackFilter_.empty()) {
      gridRows.push_back(fullLine("Filter: " + rackFilter_, uiWarnColor(), uiPanelRightBg()));
    }
    gridRows.push_back(ftxui::separator());
    const int slotWidth = max(7, (gridWidth - 8) / 5);
    for (int row = 0; row < 5; ++row) {
      ftxui::Elements rowCells;
      for (int column = 0; column < 5; ++column) {
        const auto slot = rackSlotLabel(row, column);
        const auto* item = itemAtRackSlot(store_, rack->id, slot);
        const bool selected = row == rackRow_ && column == rackColumn_;
        const bool movingSource = item != nullptr && item->id == movingRackItemId_;
        const auto bg = movingSource ? ftxui::Color::RGB(74, 48, 20)
                        : selected ? uiRowSelectedBg()
                                   : (item == nullptr ? uiRowDarkBg() : uiRowLightBg());
        const auto titleColor = selected ? uiTitleColor() : (item == nullptr ? uiMutedColor() : uiAccentColor());
        const auto itemText = item == nullptr ? string("[ empty ]") : ellipsize(item->partName, static_cast<size_t>(max(4, slotWidth - 2)));
        auto cell = ftxui::vbox({
                        ftxui::hbox({styledText(slot, titleColor) | ftxui::bold, ftxui::filler()}),
                        ftxui::hbox({styledText(itemText, item == nullptr ? uiDimColor() : uiTitleColor()), ftxui::filler()}),
                    }) |
                    ftxui::bgcolor(bg) | ftxui::size(ftxui::WIDTH, ftxui::EQUAL, slotWidth);
        if (selected) {
          cell = cell | ftxui::select;
        }
        rowCells.push_back(cell);
        if (column < 4) {
          rowCells.push_back(ftxui::separator() | ftxui::color(uiDimColor()));
        }
      }
      gridRows.push_back(ftxui::hbox(move(rowCells)));
      if (row < 4) {
        gridRows.push_back(ftxui::separator());
      }
    }
  }

  ftxui::Elements detailRows;
  detailRows.push_back(fullLine("Selected slot", uiAccentColor(), uiPanelRightBg()));
  if (rack == nullptr) {
    detailRows.push_back(detailFieldLine({"Status: ", "No racks", uiWarnColor(), uiTitleColor()}, detailWidth - 2));
    detailRows.push_back(ftxui::paragraphAlignLeft("Add or import an eligible small component to create racks automatically.") |
                         ftxui::color(uiMutedColor()));
  } else {
    detailRows.push_back(detailFieldLine({"Rack: ", rack->code, uiLabelColor(), uiTitleColor()}, detailWidth - 2));
    detailRows.push_back(detailFieldLine({"Slot: ", selectedSlot, uiLabelColor(), uiTitleColor()}, detailWidth - 2));
    detailRows.push_back(detailFieldLine({"Type: ", rack->componentType, uiLabelColor(), uiTitleColor()}, detailWidth - 2));
    detailRows.push_back(ftxui::separator());
    if (selectedSlotItem == nullptr) {
      detailRows.push_back(fullLine("EMPTY SLOT", uiMutedColor(), uiRowDarkBg()));
      detailRows.push_back(ftxui::paragraphAlignLeft(movingRackItemId_.empty()
                                                         ? "Press Space on an occupied slot to start moving a part."
                                                         : "Press Space here to place the moving part.") |
                           ftxui::color(movingRackItemId_.empty() ? uiMutedColor() : uiAccentColor()));
    } else {
      detailRows.push_back(fullLine("HIMS RACK: " + rack->code + "-" + selectedSlot, uiTitleColor(), uiRowSelectedBg()));
      detailRows.push_back(detailFieldLine({"Part: ", selectedSlotItem->partName, uiLabelColor(), uiTitleColor()}, detailWidth - 2));
      detailRows.push_back(detailFieldLine({"Category: ", displayCategory(selectedSlotItem->category), uiLabelColor(), uiTitleColor()},
                                          detailWidth - 2));
      detailRows.push_back(detailFieldLine({"Package: ", packageSummary(*selectedSlotItem), uiLabelColor(), uiTitleColor()},
                                          detailWidth - 2));
      detailRows.push_back(detailFieldLine({"Mode: ", assignmentLabel(selectedSlotItem->rackAssignment), uiLabelColor(),
                                           selectedSlotItem->rackAssignment == RackAssignmentMode::Manual ? uiWarnColor()
                                                                                                         : uiSuccessColor()},
                                          detailWidth - 2));
      detailRows.push_back(detailFieldLine({"Qty: ", to_string(selectedSlotItem->quantity), uiLabelColor(), uiTitleColor()},
                                          detailWidth - 2));
    }
    if (!movingRackItemId_.empty()) {
      detailRows.push_back(ftxui::separator());
      const auto* moving = store_.findById(movingRackItemId_);
      detailRows.push_back(fullLine("MOVING: " + (moving == nullptr ? string("missing item") : ellipsize(moving->partName, 28)),
                                    uiWarnColor(), ftxui::Color::RGB(54, 38, 18)));
      detailRows.push_back(styledText("From " + movingRackSource_, uiMutedColor()));
    }
  }
  detailRows.push_back(ftxui::separator());
  detailRows.push_back(fullLine("Actions", uiAccentColor(), uiPanelRightBg()));
  detailRows.push_back(styledText("Space move/place  p part label  P rack label", uiMutedColor()));
  detailRows.push_back(styledText("Enter detail  u unassign  a AUTO", uiMutedColor()));
  detailRows.push_back(styledText("r rename  t type  c create  x delete empty", uiMutedColor()));
  detailRows.push_back(styledText("g jump  f filter  / stock search  Esc back", uiMutedColor()));

  auto rackPanel = panel("HIMS Racks", move(rackRows), uiAccentColor()) | ftxui::bgcolor(uiPanelLeftBg()) |
                   ftxui::size(ftxui::WIDTH, ftxui::EQUAL, listWidth);
  auto gridPanel = panel("5x5 Rack Grid", move(gridRows), uiAccentColor()) | ftxui::bgcolor(uiPanelRightBg()) |
                   ftxui::size(ftxui::WIDTH, ftxui::EQUAL, gridWidth) | ftxui::flex;
  auto detailPanel = panel("Slot detail", move(detailRows), uiAccentColor()) | ftxui::bgcolor(uiPanelRightBg()) |
                     ftxui::size(ftxui::WIDTH, ftxui::EQUAL, detailWidth);

  if (compact) {
    return ftxui::vbox({
        ftxui::hbox({rackPanel, ftxui::separator(), detailPanel}),
        ftxui::separator(),
        gridPanel,
    });
  }

  return ftxui::hbox({
      rackPanel,
      ftxui::separator() | ftxui::color(uiDimColor()),
      gridPanel,
      ftxui::separator() | ftxui::color(uiDimColor()),
      detailPanel,
  });
}

void App::handleRackManagementKey(const KeyEvent& key) {
  if (key.type == KeyType::CtrlZ) {
    undoLastInventoryChange();
    syncRackSelection();
    return;
  }

  if (key.type == KeyType::Tab || key.type == KeyType::Escape) {
    changePage(Page::Dashboard);
    return;
  }

  if (key.type == KeyType::Enter) {
    const auto* item = selectedRackItem();
    if (item == nullptr) {
      setMessage("No part in this slot", 2);
      return;
    }
    const auto it = find_if(store_.items().begin(), store_.items().end(), [&](const InventoryItem& candidate) {
      return candidate.id == item->id;
    });
    if (it != store_.items().end()) {
      selectedPosition_ = static_cast<size_t>(distance(store_.items().begin(), it));
      syncSelectionToFilter();
      changePage(Page::Detail);
    }
    return;
  }

  if (key.type == KeyType::Up) {
    moveRackSlot(-1, 0);
  } else if (key.type == KeyType::Down) {
    moveRackSlot(1, 0);
  } else if (key.type == KeyType::Left) {
    moveRackSlot(0, -1);
  } else if (key.type == KeyType::Right) {
    moveRackSlot(0, 1);
  } else if (key.type == KeyType::Character) {
    if (key.ch == 'P') {
      printSelectedRackLabel();
      return;
    }
    const auto ch = tolower(static_cast<unsigned char>(key.ch));
    switch (ch) {
      case 'q':
        running_ = false;
        break;
      case '/':
        startSearch();
        break;
      case 'f':
        beginRackFilter();
        break;
      case 'g':
        inputBuffer_.clear();
        inputMode_ = InputMode::RackJump;
        setMessage("Enter rack code like R3", 3);
        break;
      case '[':
        moveRackPage(-1);
        break;
      case ']':
        moveRackPage(1);
        break;
      case 'h':
        moveRackSlot(0, -1);
        break;
      case 'j':
        moveRackSlot(1, 0);
        break;
      case 'k':
        moveRackSlot(-1, 0);
        break;
      case 'l':
        moveRackSlot(0, 1);
        break;
      case ' ':
        beginOrCompleteRackMove();
        break;
      case 'p':
        printSelectedRackPartLabel();
        break;
      case 'u':
        unassignSelectedRackItem();
        break;
      case 'a':
        autoAssignSelectedRackItem();
        break;
      case 'r':
        if (const auto* rack = selectedRack()) {
          inputBuffer_ = rack->code;
          inputMode_ = InputMode::RackRename;
          setMessage("Enter a unique rack code like R12", 3);
        }
        break;
      case 't':
        if (const auto* rack = selectedRack()) {
          inputBuffer_ = rack->componentType;
          inputMode_ = InputMode::RackType;
          setMessage("Edit this rack's dedicated component type", 3);
        }
        break;
      case 'c':
        inputBuffer_.clear();
        inputMode_ = InputMode::RackCreate;
        setMessage("Create a new empty rack by type", 3);
        break;
      case 'x':
        deleteSelectedRack();
        break;
      default:
        break;
    }
  }
}

}  // namespace hims
