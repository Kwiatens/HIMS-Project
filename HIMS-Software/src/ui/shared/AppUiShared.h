// HIMS - Hardware Inventory Management System
// Shared terminal UI formatting and inventory detail helpers.

#pragma once

#include "core/Inventory.h"

#include <ftxui/component/component.hpp>
#include <ftxui/dom/elements.hpp>

#include <initializer_list>
#include <optional>
#include <sstream>
#include <string>
#include <vector>

namespace hims {

using std::filesystem::path;
using std::initializer_list;
using std::nullopt;
using std::optional;
using std::ostringstream;
using std::size_t;
using std::string;
using std::vector;

inline constexpr const char* kColorReset = "\x1b[0m";
inline constexpr const char* kColorTitle = "\x1b[38;5;215m";
inline constexpr const char* kColorAccent = "\x1b[38;5;208m";
inline constexpr const char* kColorInfo = "\x1b[38;5;250m";
inline constexpr const char* kColorSuccess = "\x1b[38;5;252m";
inline constexpr const char* kColorLink = "\x1b[38;5;221m";
inline constexpr const char* kColorLabel = "\x1b[38;5;246m";
inline constexpr const char* kColorMuted = "\x1b[38;5;244m";
inline constexpr const char* kColorWarn = "\x1b[38;5;214m";
inline constexpr const char* kColorDanger = "\x1b[38;5;167m";
inline constexpr const char* kColorDim = "\x1b[38;5;239m";
inline constexpr const char* kColorSelect = "\x1b[48;5;238m";
inline constexpr const char* kBgPanelLeft = "\x1b[48;5;22m";
inline constexpr const char* kBgPanelRight = "\x1b[48;5;24m";
inline constexpr const char* kBgRowDark = "\x1b[48;5;18m";
inline constexpr const char* kBgRowLight = "\x1b[48;5;28m";
inline constexpr const char* kBgRowSelected = "\x1b[48;5;58m";

struct DetailField {
  string label;
  string value;
  ftxui::Color labelColor;
  ftxui::Color valueColor;
};

ftxui::Color uiTitleColor();
ftxui::Color uiAccentColor();
ftxui::Color uiInfoColor();
ftxui::Color uiSuccessColor();
ftxui::Color uiLinkColor();
ftxui::Color uiLabelColor();
ftxui::Color uiWarnColor();
ftxui::Color uiDangerColor();
ftxui::Color uiMutedColor();
ftxui::Color uiDimColor();
ftxui::Color uiPanelLeftBg();
ftxui::Color uiPanelRightBg();
ftxui::Color uiRowDarkBg();
ftxui::Color uiRowLightBg();
ftxui::Color uiRowSelectedBg();

string padRight(string value, int width);
string styleText(const string& text, const char* fg = nullptr, const char* bg = nullptr);
string styleCell(const string& text, int width, const char* fg = nullptr, const char* bg = nullptr);

ftxui::Element styledText(const string& text, optional<ftxui::Color> fg = nullopt,
                          optional<ftxui::Color> bg = nullopt);
ftxui::Element fullLine(const string& text, optional<ftxui::Color> fg = nullopt,
                        optional<ftxui::Color> bg = nullopt);
ftxui::Element bulletLine(const string& label, const string& value, ftxui::Color labelColor,
                          ftxui::Color valueColor);
ftxui::Element panel(const string& title, ftxui::Elements body, optional<ftxui::Color> titleColor = nullopt,
                     optional<ftxui::Color> borderColor = nullopt);
ftxui::Element footerField(const string& title, const string& body, ftxui::Color titleColor, ftxui::Color bodyColor,
                           ftxui::Color background, bool flashing = false);
ftxui::Element statusCueChip(const string& label, bool active, bool flashing, ftxui::Color fg,
                             ftxui::Color activeBg, ftxui::Color flashingBg, ftxui::Color inactiveBg);
ftxui::Element quantityBadge(int quantity, bool selected = false);

string displayCategory(const string& category);
string ellipsize(const string& value, size_t maxLength);
vector<string> splitLines(const string& text);
vector<string> wrapText(const string& text, int width);
void renderColumns(ostringstream& out, const vector<string>& leftLines,
                   const vector<string>& rightLines, int leftWidth, int rightWidth, int maxRows,
                   int gap = 4, const char* leftFg = nullptr, const char* leftBg = nullptr,
                   const char* rightFg = nullptr, const char* rightBg = nullptr);
void appendWrapped(vector<string>& lines, const string& text, int width);
void appendWrappedStyled(vector<string>& lines, const string& text, int width, const char* fg);

string joinTags(const vector<string>& tags);
string renderTags(const vector<string>& tags);
string renderParameters(const vector<Parameter>& parameters);
string renderUrl(const string& url);

string normalizeKey(string value);
bool categoryContains(const InventoryItem& item, initializer_list<const char*> needles);
bool parameterLabelMatches(const string& lhs, const string& rhs);
bool looksLikePackagingValue(const string& value);
const Parameter* findParameter(const vector<Parameter>& parameters, initializer_list<const char*> names);
optional<string> parameterValue(const InventoryItem& item, initializer_list<const char*> names);
string prettyLabel(const string& label);

vector<DetailField> electricalFieldsForItem(const InventoryItem& item);
vector<DetailField> stockPreviewFields(const InventoryItem& item, string rack = {});
vector<DetailField> detailCoreFields(const InventoryItem& item, string rack = {});
ftxui::Element detailFieldLine(const DetailField& field, int width);

}  // namespace hims

