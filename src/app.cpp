#include "App.h"

#include "platform/DigiKeyApi.h"

#include <ftxui/component/component.hpp>
#include <ftxui/component/screen_interactive.hpp>
#include <ftxui/dom/elements.hpp>

#include <algorithm>
#include <chrono>
#include <cctype>
#include <cstdlib>
#include <initializer_list>
#include <iomanip>
#include <iostream>
#include <limits>
#include <optional>
#include <sstream>
#include <unordered_map>
#include <utility>
#include <thread>

namespace hims {

namespace {

constexpr const char* kColorReset = "\x1b[0m";
constexpr const char* kColorTitle = "\x1b[38;5;81m";
constexpr const char* kColorAccent = "\x1b[38;5;49m";
constexpr const char* kColorInfo = "\x1b[38;5;117m";
constexpr const char* kColorSuccess = "\x1b[38;5;114m";
constexpr const char* kColorLink = "\x1b[38;5;87m";
constexpr const char* kColorLabel = "\x1b[38;5;111m";
constexpr const char* kColorMuted = "\x1b[38;5;243m";
constexpr const char* kColorWarn = "\x1b[38;5;214m";
constexpr const char* kColorDanger = "\x1b[38;5;203m";
constexpr const char* kColorDim = "\x1b[38;5;245m";
constexpr const char* kColorSelect = "\x1b[48;5;236m";
constexpr const char* kBgPanelLeft = "\x1b[48;5;233m";
constexpr const char* kBgPanelRight = "\x1b[48;5;235m";
constexpr const char* kBgRowDark = "\x1b[48;5;232m";
constexpr const char* kBgRowLight = "\x1b[48;5;236m";
constexpr const char* kBgRowSelected = "\x1b[48;5;24m";

std::string padRight(std::string value, int width);

std::string styleText(const std::string& text, const char* fg = nullptr, const char* bg = nullptr) {
  std::ostringstream out;
  if (bg != nullptr) {
    out << bg;
  }
  if (fg != nullptr) {
    out << fg;
  }
  out << text << kColorReset;
  return out.str();
}

std::string styleCell(const std::string& text, int width, const char* fg = nullptr, const char* bg = nullptr) {
  return styleText(padRight(text, width), fg, bg);
}

ftxui::Color uiTitleColor() {
  return ftxui::Color::RGB(102, 204, 255);
}

ftxui::Color uiAccentColor() {
  return ftxui::Color::RGB(80, 220, 170);
}

ftxui::Color uiInfoColor() {
  return ftxui::Color::RGB(120, 180, 255);
}

ftxui::Color uiSuccessColor() {
  return ftxui::Color::RGB(115, 220, 140);
}

ftxui::Color uiLinkColor() {
  return ftxui::Color::RGB(98, 200, 255);
}

ftxui::Color uiLabelColor() {
  return ftxui::Color::RGB(170, 160, 255);
}

ftxui::Color uiWarnColor() {
  return ftxui::Color::RGB(255, 190, 80);
}

ftxui::Color uiDangerColor() {
  return ftxui::Color::RGB(255, 110, 120);
}

ftxui::Color uiMutedColor() {
  return ftxui::Color::RGB(180, 180, 180);
}

ftxui::Color uiDimColor() {
  return ftxui::Color::RGB(120, 120, 120);
}

ftxui::Color uiPanelLeftBg() {
  return ftxui::Color::RGB(24, 24, 24);
}

ftxui::Color uiPanelRightBg() {
  return ftxui::Color::RGB(30, 30, 30);
}

ftxui::Color uiRowDarkBg() {
  return ftxui::Color::RGB(20, 20, 20);
}

ftxui::Color uiRowLightBg() {
  return ftxui::Color::RGB(42, 42, 42);
}

ftxui::Color uiRowSelectedBg() {
  return ftxui::Color::RGB(25, 70, 110);
}

ftxui::Element styledText(const std::string& text, std::optional<ftxui::Color> fg = std::nullopt,
                          std::optional<ftxui::Color> bg = std::nullopt) {
  auto element = ftxui::text(text);
  if (fg) {
    element = element | ftxui::color(*fg);
  }
  if (bg) {
    element = element | ftxui::bgcolor(*bg);
  }
  return element;
}

ftxui::Element fullLine(const std::string& text, std::optional<ftxui::Color> fg = std::nullopt,
                        std::optional<ftxui::Color> bg = std::nullopt) {
  auto element = ftxui::hbox({ftxui::text(text), ftxui::filler()});
  if (fg) {
    element = element | ftxui::color(*fg);
  }
  if (bg) {
    element = element | ftxui::bgcolor(*bg);
  }
  return element;
}

ftxui::Element linesBlock(const std::vector<std::string>& lines, std::optional<ftxui::Color> fg = std::nullopt,
                          std::optional<ftxui::Color> bg = std::nullopt) {
  ftxui::Elements elements;
  elements.reserve(lines.size());
  for (const auto& line : lines) {
    elements.push_back(fullLine(line, fg, bg));
  }
  return ftxui::vbox(std::move(elements));
}

ftxui::Element bulletLine(const std::string& label, const std::string& value, ftxui::Color labelColor,
                          ftxui::Color valueColor) {
  return ftxui::hbox({
      styledText(label, labelColor),
      styledText(value, valueColor),
      ftxui::filler(),
  });
}

ftxui::Element panel(const std::string& title, ftxui::Elements body, std::optional<ftxui::Color> titleColor = std::nullopt,
                     std::optional<ftxui::Color> borderColor = std::nullopt) {
  auto titleElement = styledText(title, titleColor);
  auto element = ftxui::window(titleElement, ftxui::vbox(std::move(body)));
  if (borderColor) {
    element = element | ftxui::color(*borderColor);
  }
  return element;
}

ftxui::Element quantityBadge(int quantity, bool selected = false) {
  const auto fg = quantity <= 0 ? uiDangerColor() : (quantity <= 5 ? uiWarnColor() : uiSuccessColor());
  const auto bg = selected ? ftxui::Color::RGB(18, 18, 18)
                           : (quantity <= 0 ? ftxui::Color::RGB(52, 22, 22)
                                            : (quantity <= 5 ? ftxui::Color::RGB(58, 42, 14)
                                                             : ftxui::Color::RGB(18, 44, 28)));
  return ftxui::text(" QTY " + std::to_string(quantity) + " ") | ftxui::bold | ftxui::color(fg) | ftxui::bgcolor(bg);
}

std::string displayCategory(const std::string& category) {
  auto value = trim(category);
  const auto slash = value.find(" / ");
  if (slash != std::string::npos) {
    value = trim(value.substr(0, slash));
  }

  const auto openParen = value.find(" (");
  if (openParen != std::string::npos) {
    value = trim(value.substr(0, openParen));
  }

  return value;
}

std::string ellipsize(const std::string& value, std::size_t maxLength) {
  if (maxLength == 0 || value.size() <= maxLength) {
    return value;
  }
  if (maxLength <= 3) {
    return value.substr(0, maxLength);
  }
  return value.substr(0, maxLength - 3) + "...";
}

KeyEvent translateEvent(const ftxui::Event& event) {
  if (event == ftxui::Event::Return) {
    return {KeyType::Enter, '\0'};
  }
  if (event == ftxui::Event::Escape) {
    return {KeyType::Escape, '\0'};
  }
  if (event == ftxui::Event::Backspace) {
    return {KeyType::Backspace, '\0'};
  }
  if (event == ftxui::Event::Tab) {
    return {KeyType::Tab, '\0'};
  }
  if (event == ftxui::Event::ArrowUp) {
    return {KeyType::Up, '\0'};
  }
  if (event == ftxui::Event::ArrowDown) {
    return {KeyType::Down, '\0'};
  }
  if (event == ftxui::Event::ArrowLeft) {
    return {KeyType::Left, '\0'};
  }
  if (event == ftxui::Event::ArrowRight) {
    return {KeyType::Right, '\0'};
  }
  if (event == ftxui::Event::Home) {
    return {KeyType::Home, '\0'};
  }
  if (event == ftxui::Event::End) {
    return {KeyType::End, '\0'};
  }
  if (event == ftxui::Event::PageUp) {
    return {KeyType::PageUp, '\0'};
  }
  if (event == ftxui::Event::PageDown) {
    return {KeyType::PageDown, '\0'};
  }
  if (event == ftxui::Event::Delete) {
    return {KeyType::Delete, '\0'};
  }
  if (event.is_character() && !event.character().empty()) {
    return {KeyType::Character, event.character()[0]};
  }
  return {KeyType::Unknown, '\0'};
}

std::string padRight(std::string value, int width) {
  if (width <= 0) {
    return {};
  }
  if (static_cast<int>(value.size()) > width) {
    if (width <= 1) {
      return value.substr(0, width);
    }
    return value.substr(0, width - 3) + "...";
  }
  value.append(static_cast<std::size_t>(width - static_cast<int>(value.size())), ' ');
  return value;
}

std::vector<std::string> splitLines(const std::string& text) {
  std::vector<std::string> lines;
  std::istringstream input(text);
  std::string line;

  while (std::getline(input, line)) {
    if (!line.empty() && line.back() == '\r') {
      line.pop_back();
    }
    lines.push_back(std::move(line));
  }

  if (lines.empty()) {
    lines.push_back({});
  }

  return lines;
}

std::vector<std::string> wrapText(const std::string& text, int width) {
  std::vector<std::string> lines;
  if (width <= 0) {
    return lines;
  }

  std::istringstream words(text);
  std::string word;
  std::string line;

  while (words >> word) {
    if (static_cast<int>(line.size() + word.size() + 1) > width && !line.empty()) {
      lines.push_back(line);
      line.clear();
    }

    if (!line.empty()) {
      line.push_back(' ');
    }
    line += word;
  }

  if (!line.empty()) {
    lines.push_back(line);
  }

  if (lines.empty()) {
    lines.push_back({});
  }

  return lines;
}

void renderColumns(std::ostringstream& out, const std::vector<std::string>& leftLines,
                   const std::vector<std::string>& rightLines, int leftWidth, int rightWidth, int maxRows,
                   int gap = 4, const char* leftFg = nullptr, const char* leftBg = nullptr,
                   const char* rightFg = nullptr, const char* rightBg = nullptr) {
  const int rowCount = std::min(
      maxRows,
      std::max(static_cast<int>(leftLines.size()), static_cast<int>(rightLines.size())));

  for (int row = 0; row < rowCount; ++row) {
    const auto left = row < static_cast<int>(leftLines.size()) ? leftLines[static_cast<std::size_t>(row)] : "";
    const auto right = row < static_cast<int>(rightLines.size()) ? rightLines[static_cast<std::size_t>(row)] : "";
    out << styleCell(left, leftWidth, leftFg, leftBg) << std::string(static_cast<std::size_t>(gap), ' ')
        << styleCell(right, rightWidth, rightFg, rightBg) << '\n';
  }
}

void appendWrapped(std::vector<std::string>& lines, const std::string& text, int width) {
  const auto wrapped = wrapText(text, width);
  lines.insert(lines.end(), wrapped.begin(), wrapped.end());
}

void appendWrappedStyled(std::vector<std::string>& lines, const std::string& text, int width, const char* fg) {
  const auto wrapped = wrapText(text, width);
  for (const auto& line : wrapped) {
    lines.push_back(styleText(line, fg));
  }
}

std::string joinTags(const std::vector<std::string>& tags) {
  return join(tags, ',');
}

std::vector<std::string> splitFlexible(const std::string& text) {
  std::vector<std::string> values;
  std::string current;
  for (char ch : text) {
    if (ch == ',' || ch == ';' || ch == '\n') {
      current = trim(current);
      if (!current.empty()) {
        values.push_back(current);
      }
      current.clear();
    } else {
      current.push_back(ch);
    }
  }

  current = trim(current);
  if (!current.empty()) {
    values.push_back(current);
  }

  return values;
}

std::vector<Parameter> parseParameters(const std::string& text) {
  std::vector<Parameter> values;
  for (const auto& entry : splitFlexible(text)) {
    const auto equalsPos = entry.find('=');
    if (equalsPos == std::string::npos) {
      continue;
    }
    values.push_back({trim(entry.substr(0, equalsPos)), trim(entry.substr(equalsPos + 1))});
  }
  return values;
}

std::string renderTags(const std::vector<std::string>& tags) {
  if (tags.empty()) {
    return "-";
  }
  return ellipsize(join(tags, ','), 32);
}

std::string renderParameters(const std::vector<Parameter>& parameters) {
  if (parameters.empty()) {
    return "-";
  }

  std::ostringstream out;
  for (std::size_t index = 0; index < parameters.size(); ++index) {
    if (index > 0) {
      out << "; ";
    }
    out << parameters[index].name << '=' << parameters[index].value;
  }
  return out.str();
}

std::string renderUrl(const std::string& url) {
  if (url.empty()) {
    return "-";
  }
  return ellipsize(url, 32);
}

struct DetailField {
  std::string label;
  std::string value;
  ftxui::Color labelColor;
  ftxui::Color valueColor;
};

std::string normalizeKey(std::string value) {
  std::string normalized;
  normalized.reserve(value.size());
  for (unsigned char ch : value) {
    if (std::isalnum(ch)) {
      normalized.push_back(static_cast<char>(std::tolower(ch)));
    }
  }
  return normalized;
}

bool categoryContains(const InventoryItem& item, std::initializer_list<const char*> needles) {
  const auto category = normalizeKey(displayCategory(item.category));
  for (const auto* needle : needles) {
    const auto normalizedNeedle = normalizeKey(needle);
    if (category.find(normalizedNeedle) != std::string::npos || normalizedNeedle.find(category) != std::string::npos) {
      return true;
    }
  }
  return false;
}

bool parameterLabelMatches(const std::string& lhs, const std::string& rhs) {
  const auto normalizedLhs = normalizeKey(lhs);
  const auto normalizedRhs = normalizeKey(rhs);
  return normalizedLhs == normalizedRhs || normalizedLhs.find(normalizedRhs) != std::string::npos ||
         normalizedRhs.find(normalizedLhs) != std::string::npos;
}

bool looksLikePackagingValue(const std::string& value) {
  const auto normalized = normalizeKey(value);
  if (normalized.empty()) {
    return false;
  }

  static const std::initializer_list<const char*> kPackagingTokens = {
      "tapeandreel", "cuttape", "digireel", "tube", "tray", "bulk", "bag", "strip", "ammo", "box", "loose",
      "pack", "reel"};
  return std::any_of(kPackagingTokens.begin(), kPackagingTokens.end(), [&](const char* token) {
    return normalized == token || normalized.find(token) != std::string::npos;
  });
}

const Parameter* findParameter(const std::vector<Parameter>& parameters, std::initializer_list<const char*> names) {
  for (const auto* name : names) {
    for (const auto& parameter : parameters) {
      if (parameterLabelMatches(parameter.name, name)) {
        return &parameter;
      }
    }
  }
  return nullptr;
}

std::optional<std::string> parameterValue(const InventoryItem& item, std::initializer_list<const char*> names) {
  if (const auto* parameter = findParameter(item.parameters, names); parameter != nullptr) {
    const auto value = trim(parameter->value);
    if (!value.empty() && !looksLikePackagingValue(value)) {
      return value;
    }
  }
  return std::nullopt;
}

std::string prettyLabel(const std::string& label) {
  const auto trimmed = trim(label);
  if (!trimmed.empty()) {
    return trimmed;
  }
  return "Value";
}

std::vector<DetailField> electricalFieldsForItem(const InventoryItem& item) {
  std::vector<DetailField> fields;
  const auto addField = [&](const std::string& label, std::optional<std::string> value) {
    if (value && !trim(*value).empty()) {
      fields.push_back({label + ": ", *value, uiLabelColor(), uiTitleColor()});
    }
  };
  const auto addValueAndPackage = [&](const std::string& label, std::optional<std::string> value,
                                      std::optional<std::string> package) {
    addField(label, std::move(value));
    addField("Package", std::move(package));
  };
  const auto hasParameter = [&](std::initializer_list<const char*> names) {
    return findParameter(item.parameters, names) != nullptr;
  };

  const auto package = parameterValue(item, {"Package / Case", "Package Case", "Case / Package", "Case Package",
                                             "Supplier Device Package", "Device Package", "Package"});

  if (categoryContains(item, {"capacitor"})) {
    addValueAndPackage("Capacitance", parameterValue(item, {"Capacitance", "Value"}), package);
    addField("Operating Voltage", parameterValue(item, {"Operating Voltage", "Voltage", "Voltage - Rated", "Rated Voltage"}));
    addField("Tolerance", parameterValue(item, {"Tolerance"}));
    addField("Type", parameterValue(item, {"Type", "Dielectric", "Dielectric Type"}));
    addField("ESR", parameterValue(item, {"ESR", "ESR (Equivalent Series Resistance)"}));
    addField("Lifetime @ Temp.", parameterValue(item, {"Lifetime @ Temp.", "Lifetime"}));
    addField("Polarization", parameterValue(item, {"Polarization"}));
    addField("Operating Temperature", parameterValue(item, {"Operating Temperature"}));
    addField("Applications", parameterValue(item, {"Applications"}));
    addField("Size / Dimension", parameterValue(item, {"Size / Dimension"}));
    return fields;
  }

  if (categoryContains(item, {"resistor"})) {
    addValueAndPackage("Resistance", parameterValue(item, {"Resistance", "Value"}), package);
    addField("Power Dissipation",
             parameterValue(item, {"Power Dissipation", "Power (Watts)", "Power Rating", "Power", "Power - Max", "Watts"}));
    addField("Tolerance", parameterValue(item, {"Tolerance"}));
    addField("Composition", parameterValue(item, {"Composition"}));
    addField("Temperature Coefficient", parameterValue(item, {"Temperature Coefficient", "Tempco"}));
    addField("Operating Temperature", parameterValue(item, {"Operating Temperature"}));
    addField("Size / Dimension", parameterValue(item, {"Size / Dimension"}));
    addField("Height", parameterValue(item, {"Height - Seated (Max)", "Height"}));
    addField("Number of Terminations", parameterValue(item, {"Number of Terminations"}));
    return fields;
  }

  if (categoryContains(item, {"indicator", "led"})) {
    addValueAndPackage("Color", parameterValue(item, {"Color"}), package);
    addField("Forward Voltage", parameterValue(item, {"Forward Voltage", "Vf"}));
    addField("Wavelength", parameterValue(item, {"Wavelength"}));
    addField("Current", parameterValue(item, {"Current", "If", "Forward Current"}));
    addField("Type", parameterValue(item, {"Type"}));
    return fields;
  }

  if (categoryContains(item, {"integrated circuit", "integrated circuits"})) {
    if (hasParameter({"Memory Type", "Memory Format", "Memory Size"}) ||
        hasParameter({"Program Memory Size", "Program Memory Type"})) {
      addValueAndPackage("Memory", parameterValue(item, {"Memory Size", "Program Memory Size", "Memory"}), package);
      addField("Memory Type", parameterValue(item, {"Memory Type", "Memory Format"}));
      addField("Memory Interface", parameterValue(item, {"Memory Interface", "Interface"}));
      addField("Technology", parameterValue(item, {"Technology"}));
      addField("Clock", parameterValue(item, {"Clock Frequency", "Speed"}));
      addField("Operating Voltage", parameterValue(item, {"Voltage - Supply", "Voltage - Supply (Min/Max)", "Voltage - Supply (Min)", "Voltage - Supply (Max)"}));
      addField("Operating Temperature", parameterValue(item, {"Operating Temperature"}));
      addField("Mounting Type", parameterValue(item, {"Mounting Type"}));
      addField("DigiKey Programmable", parameterValue(item, {"DigiKey Programmable"}));
      return fields;
    }

    if (hasParameter({"Voltage - Input", "Voltage - Output", "Current - Output", "Output Type", "Voltage - Output (Min/Fixed)"})) {
      addValueAndPackage("Output Voltage",
                         parameterValue(item, {"Voltage - Output", "Voltage - Output (Min/Fixed)", "Voltage - Output (Max)", "Output Voltage"}),
                         package);
      addField("Input Voltage", parameterValue(item, {"Voltage - Input", "Voltage - Input (Min/Max)", "Vin"}));
      addField("Current", parameterValue(item, {"Current - Output", "Output Current", "Current"}));
      addField("Type", parameterValue(item, {"Type", "Output Type"}));
      addField("Operating Temperature", parameterValue(item, {"Operating Temperature"}));
      addField("Mounting Type", parameterValue(item, {"Mounting Type"}));
      return fields;
    }

    addValueAndPackage("Function", parameterValue(item, {"Function", "Type", "Memory Type"}), package);
    addField("Technology", parameterValue(item, {"Technology"}));
    addField("Interface", parameterValue(item, {"Interface", "Memory Interface"}));
    addField("Operating Voltage", parameterValue(item, {"Voltage - Supply", "Voltage - Supply (Min/Max)"}));
    addField("Operating Temperature", parameterValue(item, {"Operating Temperature"}));
    addField("Mounting Type", parameterValue(item, {"Mounting Type"}));
    addField("DigiKey Programmable", parameterValue(item, {"DigiKey Programmable"}));
    return fields;
  }

  if (categoryContains(item, {"inductor", "choke", "coil"})) {
    addValueAndPackage("Inductance", parameterValue(item, {"Inductance", "Value"}), package);
    addField("Current Rating", parameterValue(item, {"Current Rating", "Current Rating (Amps)", "Current"}));
    addField("DC Resistance", parameterValue(item, {"DC Resistance", "DC Resistance (DCR)", "DCR"}));
    addField("Saturation Current", parameterValue(item, {"Saturation Current", "Current - Saturation (Isat)"}));
    addField("Q @ Freq", parameterValue(item, {"Q @ Freq"}));
    addField("Frequency", parameterValue(item, {"Frequency - Self Resonant", "Frequency"}));
    addField("Shielding", parameterValue(item, {"Shielding"}));
    addField("Material", parameterValue(item, {"Material - Core"}));
    return fields;
  }

  if (categoryContains(item, {"diode", "rectifier", "schottky"})) {
    addValueAndPackage("Forward Voltage", parameterValue(item, {"Forward Voltage", "Voltage - Forward (Vf) (Max) @ If", "Vf"}), package);
    addField("Reverse Voltage", parameterValue(item, {"Reverse Voltage", "Voltage - DC Reverse (Vr) (Max)", "Peak Reverse Voltage", "Vr"}));
    addField("Current", parameterValue(item, {"Current", "Current - Average Rectified (Io)", "If", "Forward Current"}));
    addField("Technology", parameterValue(item, {"Technology"}));
    addField("Speed", parameterValue(item, {"Speed"}));
    addField("Reverse Leakage", parameterValue(item, {"Current - Reverse Leakage @ Vr"}));
    addField("Operating Temperature", parameterValue(item, {"Operating Temperature - Junction", "Operating Temperature"}));
    return fields;
  }

  if (categoryContains(item, {"transistor", "mosfet", "fet", "discrete semiconductor"})) {
    addValueAndPackage("Drain-Source Voltage",
                       parameterValue(item, {"Drain-Source Voltage", "Drain to Source Voltage (Vdss)", "Vds", "Vdss"}), package);
    addField("Current", parameterValue(item, {"Continuous Drain Current", "Current - Continuous Drain (Id) @ 25°C", "Current", "Id"}));
    addField("RDS On", parameterValue(item, {"Rds On", "Rds On (Max) @ Id, Vgs", "RDS(ON)"}));
    addField("Gate Charge", parameterValue(item, {"Gate Charge", "Gate Charge (Qg) (Max) @ Vgs"}));
    addField("Drive Voltage", parameterValue(item, {"Drive Voltage", "Drive Voltage (Max Rds On, Min Rds On)"}));
    addField("VGS", parameterValue(item, {"Vgs", "Vgs (Max)", "Vgs(th) (Max) @ Id"}));
    addField("Input Capacitance", parameterValue(item, {"Input Capacitance (Ciss) (Max) @ Vds", "Input Capacitance"}));
    addField("Power - Max", parameterValue(item, {"Power - Max"}));
    addField("Technology", parameterValue(item, {"Technology"}));
    return fields;
  }

  if (categoryContains(item, {"connector"})) {
    addValueAndPackage("Pins", parameterValue(item, {"Pins", "Number of Positions", "Pin Count"}), package);
    addField("Connector Type", parameterValue(item, {"Connector Type"}));
    addField("Contact Type", parameterValue(item, {"Contact Type"}));
    addField("Rows", parameterValue(item, {"Rows", "Number of Rows"}));
    addField("Pitch", parameterValue(item, {"Pitch", "Pitch - Mating"}));
    addField("Mounting Type", parameterValue(item, {"Mounting Type"}));
    addField("Termination", parameterValue(item, {"Termination"}));
    addField("Fastening Type", parameterValue(item, {"Fastening Type"}));
    addField("Shrouding", parameterValue(item, {"Shrouding"}));
    return fields;
  }

  if (categoryContains(item, {"mcu", "microcontroller"})) {
    addValueAndPackage("Flash", parameterValue(item, {"Flash"}), package);
    addField("Core", parameterValue(item, {"Core", "Core Processor"}));
    addField("Operating Voltage", parameterValue(item, {"Operating Voltage", "Voltage - Supply", "Voltage"}));
    addField("RAM", parameterValue(item, {"RAM", "Memory"}));
    addField("Clock", parameterValue(item, {"Clock Speed", "Speed"}));
    addField("I/O", parameterValue(item, {"Number of I/O"}));
    addField("Program Memory", parameterValue(item, {"Program Memory Size", "Program Memory Type"}));
    addField("EEPROM", parameterValue(item, {"EEPROM Size"}));
    addField("Connectivity", parameterValue(item, {"Connectivity"}));
    addField("Peripherals", parameterValue(item, {"Peripherals"}));
    return fields;
  }

  if (categoryContains(item, {"regulator", "voltage regulator", "power management"})) {
    addValueAndPackage("Output Voltage", parameterValue(item, {"Output Voltage", "Voltage - Output", "Vout"}), package);
    addField("Input Voltage", parameterValue(item, {"Voltage - Input", "Vin"}));
    addField("Current", parameterValue(item, {"Output Current", "Current - Output", "Iout"}));
    addField("Type", parameterValue(item, {"Type", "Output Type"}));
    return fields;
  }

  if (categoryContains(item, {"crystal", "oscillator", "resonator"})) {
    addValueAndPackage("Frequency", parameterValue(item, {"Frequency"}), package);
    addField("Load Capacitance", parameterValue(item, {"Load Capacitance"}));
    addField("ESR", parameterValue(item, {"ESR", "Equivalent Series Resistance"}));
    addField("Operating Mode", parameterValue(item, {"Operating Mode"}));
    addField("Operating Temperature", parameterValue(item, {"Operating Temperature"}));
    addField("Size / Dimension", parameterValue(item, {"Size / Dimension"}));
    return fields;
  }

  if (categoryContains(item, {"sensor", "temperature sensor", "pressure sensor"})) {
    addValueAndPackage("Type", parameterValue(item, {"Type"}), package);
    addField("Sensor Type", parameterValue(item, {"Sensor Type"}));
    addField("Output", parameterValue(item, {"Output", "Output Type"}));
    addField("Voltage - Supply", parameterValue(item, {"Voltage - Supply"}));
    addField("Resolution", parameterValue(item, {"Resolution"}));
    addField("Accuracy", parameterValue(item, {"Accuracy - Highest (Lowest)"}));
    addField("Operating Temperature", parameterValue(item, {"Operating Temperature"}));
    addField("Features", parameterValue(item, {"Features"}));
    return fields;
  }

  if (categoryContains(item, {"circuit protection", "fuse", "tvs", "transient voltage suppressor"})) {
    addValueAndPackage("Current Rating", parameterValue(item, {"Current Rating (Amps)", "Current Rating", "Current"}), package);
    addField("Voltage AC", parameterValue(item, {"Voltage Rating - AC"}));
    addField("Voltage DC", parameterValue(item, {"Voltage Rating - DC"}));
    addField("Reverse Standoff", parameterValue(item, {"Voltage - Reverse Standoff (Typ)"}));
    addField("Breakdown", parameterValue(item, {"Voltage - Breakdown (Min)"}));
    addField("Clamping", parameterValue(item, {"Voltage - Clamping (Max) @ Ipp"}));
    addField("Peak Pulse Current", parameterValue(item, {"Current - Peak Pulse (10/1000µs)"}));
    addField("Peak Pulse Power", parameterValue(item, {"Power - Peak Pulse"}));
    addField("Response Time", parameterValue(item, {"Response Time"}));
    addField("Operating Temperature", parameterValue(item, {"Operating Temperature"}));
    return fields;
  }

  std::optional<std::string> primaryValue;
  std::optional<std::string> primaryLabel;
  for (const auto& parameter : item.parameters) {
    const auto label = prettyLabel(parameter.name);
    if (normalizeKey(label) == "package") {
      continue;
    }
    if (!primaryValue) {
      primaryLabel = label;
      primaryValue = trim(parameter.value);
      continue;
    }
    addField(label, trim(parameter.value));
  }

  if (primaryValue && !primaryValue->empty()) {
    fields.insert(fields.begin(),
                  DetailField{primaryLabel.value_or("Value") + ": ", *primaryValue, uiLabelColor(), uiTitleColor()});
  } else if (package && !package->empty()) {
    fields.insert(fields.begin(), DetailField{"Package: ", *package, uiLabelColor(), uiTitleColor()});
  }

  return fields;
}

std::vector<DetailField> stockPreviewFields(const InventoryItem& item) {
  std::vector<DetailField> fields;
  fields.push_back({"Name: ", item.partName, uiInfoColor(), uiTitleColor()});
  fields.push_back({"Manufacturer: ", item.manufacturer, uiInfoColor(), uiTitleColor()});
  fields.push_back({"Quantity: ", std::to_string(item.quantity), uiSuccessColor(), uiTitleColor()});

  const auto electricalFields = electricalFieldsForItem(item);
  fields.insert(fields.end(), electricalFields.begin(), electricalFields.end());
  return fields;
}

std::vector<DetailField> detailCoreFields(const InventoryItem& item) {
  return {
      {"Name: ", item.partName, uiInfoColor(), uiTitleColor()},
      {"Manufacturer: ", item.manufacturer, uiInfoColor(), uiTitleColor()},
      {"Category: ", displayCategory(item.category), uiLabelColor(), uiTitleColor()},
      {"Quantity: ", std::to_string(item.quantity), uiSuccessColor(), uiTitleColor()},
      {"Threshold: ", std::to_string(item.reorderThreshold), uiWarnColor(), uiTitleColor()},
      {"Location: ", item.location, uiMutedColor(), uiTitleColor()},
  };
}

ftxui::Element detailFieldLine(const DetailField& field, int width) {
  const int valueWidth = std::max(0, width - static_cast<int>(field.label.size()) - 1);
  return ftxui::hbox({
             styledText(field.label, field.labelColor),
             styledText(ellipsize(field.value, static_cast<std::size_t>(valueWidth)), field.valueColor),
             ftxui::filler(),
         }) |
         ftxui::size(ftxui::WIDTH, ftxui::EQUAL, width);
}

bool upsertParameter(std::vector<Parameter>& parameters, const std::string& name, const std::string& value) {
  const auto trimmedValue = trim(value);
  if (trimmedValue.empty()) {
    return false;
  }

  for (auto& parameter : parameters) {
    if (parameterLabelMatches(parameter.name, name)) {
      if (parameter.name.empty()) {
        parameter.name = name;
      }
      const bool changed = parameter.value != trimmedValue;
      parameter.value = trimmedValue;
      return changed;
    }
  }

  parameters.push_back({name, trimmedValue});
  return true;
}

bool mergeDigiKeyMetadata(InventoryItem& item, const DigiKeyProductDetails& details) {
  bool changed = false;

  const auto normalizePackageLabels = [&]() {
    for (auto& parameter : item.parameters) {
      if (parameterLabelMatches(parameter.name, "Package") && looksLikePackagingValue(parameter.value)) {
        parameter.name = "Packaging";
        changed = true;
      }
    }
  };
  normalizePackageLabels();

  const auto assignIfUseful = [&](std::string& target, const std::string& value, bool replaceUnknown = false) {
    const auto trimmed = trim(value);
    if (trimmed.empty()) {
      return;
    }
    if (target.empty() || (replaceUnknown && (target == "Unknown" || target == "Scanned DigiKey Item"))) {
      target = trimmed;
      changed = true;
    }
  };

  if ((item.partName.empty() || item.partName == "Scanned DigiKey Item") && !trim(details.productDescription).empty()) {
    item.partName = trim(details.productDescription);
    changed = true;
  }

  assignIfUseful(item.manufacturer, details.manufacturerName, true);
  assignIfUseful(item.sku, details.manufacturerPartNumber);
  assignIfUseful(item.productUrl, details.productUrl);
  assignIfUseful(item.datasheetUrl, details.datasheetUrl);

  for (const auto& parameter : details.parameters) {
    if (upsertParameter(item.parameters, parameter.name, parameter.value)) {
      changed = true;
    }
  }

  if (!trim(details.packagingType).empty()) {
    if (upsertParameter(item.parameters, "Packaging", details.packagingType)) {
      changed = true;
    }
  }

  if (!details.packageName.empty()) {
    if (upsertParameter(item.parameters, "Package", details.packageName)) {
      changed = true;
    }
  }

  if (item.syncStatus != "synced") {
    item.syncStatus = "synced";
    changed = true;
  }

  if (changed) {
    item.lastUpdated = std::time(nullptr);
  }

  return changed;
}

std::filesystem::path documentsHimsPath() {
  if (const char* profile = std::getenv("USERPROFILE"); profile != nullptr && *profile != '\0') {
    return std::filesystem::path(profile) / "Documents" / "HIMS";
  }
  return std::filesystem::current_path() / "Documents" / "HIMS";
}

std::filesystem::path legacyDatabasePath() {
  const auto githubRoot = std::filesystem::current_path().parent_path().parent_path();
  return githubRoot / "Kwiatens Stock Management System" / "KwiatensStockManagementSystem" / "data" / "kwiatens-stock.db";
}

void copyDatabaseSidecar(const std::filesystem::path& sourceBase, const std::filesystem::path& destinationBase,
                         const std::string& suffix) {
  const auto source = std::filesystem::path(sourceBase.string() + suffix);
  const auto destination = std::filesystem::path(destinationBase.string() + suffix);
  std::error_code error;
  if (std::filesystem::exists(source, error)) {
    std::filesystem::copy_file(source, destination, std::filesystem::copy_options::overwrite_existing, error);
  }
}

void ensureInventoryDatabaseCopied(const std::filesystem::path& localBase) {
  std::error_code error;
  if (std::filesystem::exists(localBase, error)) {
    return;
  }

  const auto sourceBase = legacyDatabasePath();
  if (!std::filesystem::exists(sourceBase, error)) {
    return;
  }

  std::filesystem::create_directories(localBase.parent_path(), error);
  std::filesystem::copy_file(sourceBase, localBase, std::filesystem::copy_options::overwrite_existing, error);
  copyDatabaseSidecar(sourceBase, localBase, "-wal");
  copyDatabaseSidecar(sourceBase, localBase, "-shm");
}

std::filesystem::path locateDotEnvFile() {
  std::error_code error;
  auto current = std::filesystem::current_path();
  for (int depth = 0; depth < 8 && !current.empty(); ++depth) {
    const auto candidate = current / ".env";
    if (std::filesystem::exists(candidate, error)) {
      return candidate;
    }
    const auto parent = current.parent_path();
    if (parent == current) {
      break;
    }
    current = parent;
  }
  return {};
}

}  // namespace

App::App()
    : root_(std::filesystem::current_path()),
      dataPath_(documentsHimsPath()),
      inventoryPath_(dataPath_ / "inventory.db"),
      activityPath_(dataPath_ / "activity.tsv") {
  loadEnvironmentFile(locateDotEnvFile());
  ensureInventoryDatabaseCopied(inventoryPath_);
  loadState();

  if (!server_.start(8080, [this](const std::string& code) { pushScanCode(code); })) {
    setMessage("Scanner server failed to start; terminal still works", 5);
  } else {
    setMessage("Scanner ready at " + scannerUrl(), 5);
  }
}

void App::loadState() {
  store_.load(inventoryPath_);
  loadActivities(activityPath_, activities_);
  if (activities_.empty()) {
    activities_.push_back(makeActivity("system", "Inventory loaded"));
    activities_.push_back(makeActivity("system", "Terminal dashboard initialized"));
  }

  const auto config = loadDigiKeyConfig();
  if (config.valid()) {
    DigiKeyApiClient client(std::move(config));
    std::size_t refreshedCount = 0;
    std::size_t attemptedCount = 0;
    for (auto& item : store_.items()) {
      std::string lookup = !item.digikeyPartNumber.empty() ? item.digikeyPartNumber : item.sku;
      if (lookup.empty() && !item.manufacturer.empty() && !item.partName.empty()) {
        lookup = item.manufacturer + " " + item.partName;
      }
      if (lookup.empty()) {
        lookup = item.partName;
      }
      if (trim(lookup).empty()) {
        continue;
      }

      ++attemptedCount;
      std::string error;
      if (const auto details = client.fetchProductDetails(lookup, &error); details.has_value()) {
        if (mergeDigiKeyMetadata(item, *details)) {
          ++refreshedCount;
        }
      }
    }

    if (attemptedCount > 0) {
      activities_.push_back(makeActivity("sync", "DigiKey metadata refreshed for " + std::to_string(refreshedCount) +
                                                   " of " + std::to_string(attemptedCount) + " parts"));
    }
  }

  server_.setRecentActivity(activities_);
  saveState();
}

void App::saveState() {
  store_.save(inventoryPath_);
  saveActivities(activityPath_, activities_);
}

ftxui::Element App::renderUi() const {
  ftxui::Elements body;
  body.push_back(fullLine("HIMS Terminal  " + summaryLine(), uiTitleColor(), uiPanelLeftBg()));
  body.push_back(ftxui::separator());
  body.push_back(renderPageUi() | ftxui::flex);
  body.push_back(ftxui::separator());
  body.push_back(renderSearchBarUi());
  body.push_back(renderMessageUi());
  body.push_back(renderStatusBarUi());
  return ftxui::vbox(std::move(body));
}

ftxui::Element App::renderPageUi() const {
  switch (page_) {
    case Page::Dashboard:
      return renderDashboardUi();
    case Page::Stock:
      return renderStockUi();
    case Page::Detail:
      return renderDetailUi();
  }

  return ftxui::text("");
}

ftxui::Element App::renderDashboardUi() const {
  const auto summary = summarize(store_.items());

  ftxui::Elements leftBody;
  leftBody.push_back(fullLine("Summary", uiAccentColor(), uiPanelLeftBg()));
  leftBody.push_back(bulletLine("Parts: ", std::to_string(summary.itemCount), uiInfoColor(), uiTitleColor()));
  leftBody.push_back(bulletLine("Units: ", std::to_string(summary.totalUnits), uiSuccessColor(), uiTitleColor()));
  leftBody.push_back(bulletLine("Low stock: ", std::to_string(summary.lowStockCount), uiWarnColor(), uiTitleColor()));
  leftBody.push_back(
      bulletLine("Missing metadata: ", std::to_string(summary.missingMetadataCount), uiDangerColor(), uiTitleColor()));
  leftBody.push_back(bulletLine("Unsynced: ", std::to_string(summary.unsyncedCount), uiLabelColor(), uiTitleColor()));
  leftBody.push_back(ftxui::separator());
  leftBody.push_back(fullLine("Scanner", uiTitleColor(), uiPanelLeftBg()));
  leftBody.push_back(ftxui::paragraphAlignLeft(scannerUrl()) | ftxui::color(uiLinkColor()));
  leftBody.push_back(ftxui::separator());
  leftBody.push_back(fullLine("Inventory file", uiTitleColor(), uiPanelLeftBg()));
  leftBody.push_back(ftxui::paragraphAlignLeft(inventoryPath_.string()) | ftxui::color(uiInfoColor()));
  leftBody.push_back(ftxui::separator());
  leftBody.push_back(fullLine("Quick actions", uiAccentColor(), uiPanelLeftBg()));
  leftBody.push_back(
      ftxui::paragraphAlignLeft("1 Stock browser   2/s Scanner   3 Add item   4 Reload   / Search   q Quit") |
      ftxui::color(uiDimColor()));

  ftxui::Elements rightBody;
  rightBody.push_back(fullLine("Low-stock alerts", uiWarnColor(), uiPanelRightBg()));
  int alertCount = 0;
  for (const auto& item : store_.items()) {
    if (!item.lowStock()) {
      continue;
    }
    rightBody.push_back(fullLine("  - " + item.partName + " qty " + std::to_string(item.quantity) + " / " +
                                     std::to_string(item.reorderThreshold) + "  [" + item.category + "]",
                                 uiWarnColor(), uiRowLightBg()));
    if (++alertCount >= 6) {
      break;
    }
  }
  if (alertCount == 0) {
    rightBody.push_back(fullLine("  No low-stock items.", uiMutedColor(), uiPanelRightBg()));
  }

  rightBody.push_back(ftxui::separator());
  rightBody.push_back(fullLine("Recent activity", uiAccentColor(), uiPanelRightBg()));
  const auto activityCount = std::min<std::size_t>(activities_.size(), 8);
  for (std::size_t offset = 0; offset < activityCount; ++offset) {
    const auto& entry = activities_[activities_.size() - 1 - offset];
    const auto line = "  - " + nowTimestampString(entry.timestamp) + " | " + entry.kind + " | " + entry.message;
    ftxui::Color color = uiMutedColor();
    if (entry.kind.find("scan") != std::string::npos) {
      color = uiLinkColor();
    } else if (entry.kind.find("add") != std::string::npos) {
      color = uiSuccessColor();
    } else if (entry.kind.find("edit") != std::string::npos) {
      color = uiInfoColor();
    } else if (entry.kind == "system") {
      color = uiLabelColor();
    }
    rightBody.push_back(ftxui::paragraphAlignLeft(line) | ftxui::color(color));
  }
  if (activityCount == 0) {
    rightBody.push_back(fullLine("  No recent activity.", uiMutedColor(), uiPanelRightBg()));
  }

  auto leftPanel = panel("Overview", std::move(leftBody), uiAccentColor()) | ftxui::bgcolor(uiPanelLeftBg()) | ftxui::flex;
  auto rightPanel = panel("Activity", std::move(rightBody), uiWarnColor()) | ftxui::bgcolor(uiPanelRightBg()) | ftxui::flex;

  return ftxui::hbox({
      leftPanel,
      ftxui::separator(),
      rightPanel,
  });
}

ftxui::Element App::renderStockUi() const {
  const auto filtered = filteredIndices();
  const auto* activeScreen = ftxui::ScreenInteractive::Active();
  const int screenWidth = activeScreen != nullptr ? activeScreen->dimx() : 120;
  const int detailOuterWidth = std::clamp(screenWidth / 4, 38, 52);
  const int listOuterWidth = std::max(40, screenWidth - detailOuterWidth - 1);
  const int listInnerWidth = std::max(20, listOuterWidth - 2);
  const int detailInnerWidth = std::max(20, detailOuterWidth - 2);
  const int qtyWidth = 10;
  int partWidth = std::clamp(listInnerWidth / 3, 22, 34);
  int categoryWidth = listInnerWidth - partWidth - qtyWidth - 2;
  if (categoryWidth < 12) {
    categoryWidth = 12;
    partWidth = std::max(18, listInnerWidth - categoryWidth - qtyWidth - 2);
  }
  if (partWidth < 18) {
    partWidth = 18;
  }

  auto fixedCell = [](const std::string& text, int width, ftxui::Color color) {
    return ftxui::hbox({
               styledText(ellipsize(text, static_cast<std::size_t>(std::max(width, 0))), color),
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
    for (std::size_t index = 0; index < filtered.size(); ++index) {
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
    const auto previewFields = stockPreviewFields(*item);
    for (std::size_t index = 0; index < previewFields.size(); ++index) {
      const auto& field = previewFields[index];
      if (index == 3 && !electricalFields.empty()) {
        detailRows.push_back(ftxui::separator());
        detailRows.push_back(fullLine("Electrical parameters", uiAccentColor(), uiPanelRightBg()));
      }
      detailRows.push_back(detailFieldLine(field, detailInnerWidth));
    }
  } else {
    detailRows.push_back(fullLine("No item selected.", uiMutedColor(), uiPanelRightBg()));
  }

  auto listContent = ftxui::vbox(std::move(listRows)) | ftxui::yframe | ftxui::vscroll_indicator;
  auto listPanel = ftxui::window(styledText("Items", uiAccentColor()), std::move(listContent)) |
                   ftxui::bgcolor(uiPanelLeftBg()) | ftxui::size(ftxui::WIDTH, ftxui::EQUAL, listOuterWidth);
  auto detailPanel = panel("Detail", std::move(detailRows), uiAccentColor()) | ftxui::bgcolor(uiPanelRightBg()) |
                     ftxui::size(ftxui::WIDTH, ftxui::EQUAL, detailOuterWidth);

  return ftxui::hbox({
      listPanel,
      ftxui::separator() | ftxui::color(uiDimColor()),
      detailPanel,
  });
}

ftxui::Element App::renderDetailUi() const {
  ftxui::Elements leftRows;
  ftxui::Elements rightRows;

  if (const auto* item = selectedItem()) {
    const auto electricalFields = electricalFieldsForItem(*item);
    leftRows.push_back(fullLine("Core details", uiAccentColor(), uiPanelLeftBg()));
    for (const auto& field : detailCoreFields(*item)) {
      leftRows.push_back(detailFieldLine(field, 40));
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

  auto leftPanel = panel("Item detail", std::move(leftRows), uiAccentColor()) | ftxui::bgcolor(uiPanelLeftBg()) | ftxui::flex;
  auto rightPanel = panel("Links and notes", std::move(rightRows), uiAccentColor()) | ftxui::bgcolor(uiPanelRightBg()) | ftxui::flex;

  return ftxui::hbox({
      leftPanel,
      ftxui::separator(),
      rightPanel,
  });
}

ftxui::Element App::renderSearchBarUi() const {
  ftxui::Elements rows;
  if (inputMode_ == InputMode::Search) {
    rows.push_back(fullLine("Search  /" + inputBuffer_ + "_", uiLinkColor(), uiPanelLeftBg()));
  } else {
    rows.push_back(fullLine("Search  /" + searchQuery_, uiMutedColor(), uiPanelLeftBg()));
  }

  if (inputMode_ == InputMode::EditFieldMenu) {
    ftxui::Elements options;
    options.push_back(fullLine("Edit fields", uiAccentColor(), uiPanelLeftBg()));
    for (std::size_t index = 0; index < menuOptions_.size(); ++index) {
      const auto bg = static_cast<int>(index) == fieldMenuIndex_ ? uiRowSelectedBg() : (index % 2 == 0 ? uiRowDarkBg() : uiRowLightBg());
      auto option = fullLine("  [" + std::to_string(index < 9 ? index + 1 : 0) + "] " + menuOptions_[index].label,
                             static_cast<int>(index) == fieldMenuIndex_ ? uiTitleColor() : uiMutedColor(), bg);
      if (static_cast<int>(index) == fieldMenuIndex_) {
        option = option | ftxui::select;
      }
      options.push_back(option);
    }
    rows.push_back(panel("Edit fields", std::move(options), uiAccentColor()) | ftxui::bgcolor(uiPanelLeftBg()));
  } else if (inputMode_ == InputMode::EditValue) {
    rows.push_back(fullLine("Input  " + activePrompt() + inputBuffer_ + "_", uiLinkColor(), uiPanelLeftBg()));
  }

  return ftxui::vbox(std::move(rows));
}

ftxui::Element App::renderStatusBarUi() const {
  return fullLine(scannerUrl() + "  Dashboard: Tab/1  Stock: arrows/jk  Detail: Enter  Edit: e  New: n  Scanner: s  Quit: q",
                  uiLinkColor(), uiPanelRightBg());
}

ftxui::Element App::renderMessageUi() const {
  if (message_.empty()) {
    return ftxui::text("");
  }
  return fullLine(message_, uiAccentColor(), uiPanelLeftBg());
}

int App::run() {
  running_ = true;
  auto screen = ftxui::ScreenInteractive::Fullscreen();
  screen.TrackMouse();
  auto renderer = ftxui::Renderer([this] { return renderUi(); });
  auto component = ftxui::CatchEvent(renderer, [this, &screen](ftxui::Event event) {
    if (event == ftxui::Event::Custom) {
      processScans();
      clearMessageIfExpired();
      return true;
    }

    if (event.is_mouse()) {
      if (page_ == Page::Stock) {
        const auto& mouse = event.mouse();
        if (mouse.button == ftxui::Mouse::WheelUp) {
          moveSelection(-1);
          return true;
        }
        if (mouse.button == ftxui::Mouse::WheelDown) {
          moveSelection(1);
          return true;
        }
      }
      return false;
    }

    const auto key = translateEvent(event);
    if (key.type == KeyType::Unknown && event != ftxui::Event::Escape) {
      return false;
    }

    handleKey(key);
    if (!running_) {
      screen.ExitLoopClosure()();
    }
    return true;
  });

  std::thread ticker([this, &screen] {
    while (running_) {
      screen.PostEvent(ftxui::Event::Custom);
      std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
  });

  screen.Loop(component);
  running_ = false;
  if (ticker.joinable()) {
    ticker.join();
  }
  saveState();
  server_.stop();
  return 0;
}

void App::processInput() {
  for (const auto& key : pollKeys()) {
    handleKey(key);
  }
}

void App::handleKey(const KeyEvent& key) {
  switch (inputMode_) {
    case InputMode::Search:
      handleSearchKey(key);
      return;
    case InputMode::EditFieldMenu:
      handleEditMenuKey(key);
      return;
    case InputMode::EditValue:
      handleEditValueKey(key);
      return;
    case InputMode::None:
      break;
  }

  switch (page_) {
    case Page::Dashboard:
      handleDashboardKey(key);
      break;
    case Page::Stock:
      handleStockKey(key);
      break;
    case Page::Detail:
      handleDetailKey(key);
      break;
  }
}

void App::handleDashboardKey(const KeyEvent& key) {
  if (key.type == KeyType::Character) {
    switch (std::tolower(static_cast<unsigned char>(key.ch))) {
      case '1':
      case '\t':
        changePage(Page::Stock);
        break;
      case '2':
      case 's':
        openUrl(scannerUrl());
        setMessage("Opened scanner page in the default browser", 3);
        break;
      case '3':
        beginEditCurrentItem(true);
        break;
      case '4':
        store_.load(inventoryPath_);
        setMessage("Inventory reloaded", 2);
        break;
      case '/':
        startSearch();
        break;
      case 'q':
        running_ = false;
        break;
      default:
        break;
    }
    return;
  }

  if (key.type == KeyType::Enter || key.type == KeyType::Tab) {
    changePage(Page::Stock);
  }
}

void App::handleStockKey(const KeyEvent& key) {
  if (key.type == KeyType::Character) {
    const auto ch = std::tolower(static_cast<unsigned char>(key.ch));
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
                                         ? std::string()
                                         : "https://www.digikey.com/en/products/result?keywords=" + item->digikeyPartNumber;
          openCurrentUrl(digiKeySearch, "DigiKey");
        }
        break;
      case 'r':
        store_.load(inventoryPath_);
        syncSelectionToFilter();
        setMessage("Inventory refreshed", 2);
        break;
      case 's':
        openUrl(scannerUrl());
        setMessage("Opened scanner page in the default browser", 3);
        break;
      case '\t':
        changePage(Page::Dashboard);
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

void App::handleDetailKey(const KeyEvent& key) {
  if (key.type == KeyType::Character) {
    const auto ch = std::tolower(static_cast<unsigned char>(key.ch));
    switch (ch) {
      case 'e':
        beginEditCurrentItem(false);
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
                                         ? std::string()
                                         : "https://www.digikey.com/en/products/result?keywords=" + item->digikeyPartNumber;
          openCurrentUrl(digiKeySearch, "DigiKey");
        }
        break;
      case 's':
        openUrl(scannerUrl());
        setMessage("Opened scanner page in the default browser", 3);
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

void App::handleSearchKey(const KeyEvent& key) {
  if (key.type == KeyType::Character) {
    inputBuffer_.push_back(key.ch);
    dirty_ = true;
    return;
  }

  if (key.type == KeyType::Backspace) {
    if (!inputBuffer_.empty()) {
      inputBuffer_.pop_back();
      dirty_ = true;
    }
    return;
  }

  if (key.type == KeyType::Enter) {
    searchQuery_ = inputBuffer_;
    inputMode_ = InputMode::None;
    syncSelectionToFilter();
    setMessage(searchQuery_.empty() ? "Filter cleared" : "Filter applied", 2);
    return;
  }

  if (key.type == KeyType::Escape) {
    inputBuffer_ = searchQuery_;
    inputMode_ = InputMode::None;
    setMessage("Search cancelled", 2);
  }
}

void App::handleEditMenuKey(const KeyEvent& key) {
  if (key.type == KeyType::Character) {
    const auto ch = std::tolower(static_cast<unsigned char>(key.ch));
    if (ch >= '1' && ch <= '9') {
      fieldMenuIndex_ = ch - '1';
      if (fieldMenuIndex_ < static_cast<int>(menuOptions_.size())) {
        inputBuffer_ = currentFieldValue(menuOptions_[fieldMenuIndex_].field);
        inputMode_ = InputMode::EditValue;
        setMessage("Editing " + fieldLabel(menuOptions_[fieldMenuIndex_].field), 2);
      }
      return;
    }
    if (ch == '0' && menuOptions_.size() >= 10) {
      fieldMenuIndex_ = 9;
      inputBuffer_ = currentFieldValue(menuOptions_[fieldMenuIndex_].field);
      inputMode_ = InputMode::EditValue;
      setMessage("Editing " + fieldLabel(menuOptions_[fieldMenuIndex_].field), 2);
      return;
    }
    if (ch == 'q' || ch == 'c') {
      cancelInput();
    }
    return;
  }

  if (key.type == KeyType::Up) {
    fieldMenuIndex_ = std::max(0, fieldMenuIndex_ - 1);
    dirty_ = true;
  } else if (key.type == KeyType::Down) {
    fieldMenuIndex_ = std::min(fieldMenuIndex_ + 1, static_cast<int>(menuOptions_.size()) - 1);
    dirty_ = true;
  } else if (key.type == KeyType::Enter) {
    if (fieldMenuIndex_ >= 0 && fieldMenuIndex_ < static_cast<int>(menuOptions_.size())) {
      if (fieldMenuIndex_ == static_cast<int>(menuOptions_.size()) - 2) {
        saveWorkingCopy();
        return;
      }
      if (fieldMenuIndex_ == static_cast<int>(menuOptions_.size()) - 1) {
        cancelInput();
        return;
      }
      inputBuffer_ = currentFieldValue(menuOptions_[fieldMenuIndex_].field);
      inputMode_ = InputMode::EditValue;
      setMessage("Editing " + fieldLabel(menuOptions_[fieldMenuIndex_].field), 2);
    }
  } else if (key.type == KeyType::Escape) {
    cancelInput();
  }
}

void App::handleEditValueKey(const KeyEvent& key) {
  if (key.type == KeyType::Character) {
    inputBuffer_.push_back(key.ch);
    dirty_ = true;
    return;
  }

  if (key.type == KeyType::Backspace) {
    if (!inputBuffer_.empty()) {
      inputBuffer_.pop_back();
      dirty_ = true;
    }
    return;
  }

  if (key.type == KeyType::Enter) {
    if (fieldMenuIndex_ >= 0 && fieldMenuIndex_ < static_cast<int>(menuOptions_.size())) {
      commitEditField(menuOptions_[fieldMenuIndex_].field, inputBuffer_);
    }
    inputBuffer_.clear();
    inputMode_ = InputMode::EditFieldMenu;
    return;
  }

  if (key.type == KeyType::Escape) {
    inputBuffer_.clear();
    inputMode_ = InputMode::EditFieldMenu;
    setMessage("Edit cancelled", 2);
  }
}

void App::render() {
  const auto size = consoleSize();
  std::ostringstream out;
  out << "\x1b[?25l";
  out << kColorTitle << "HIMS Terminal" << kColorReset << "  ";
  out << kColorMuted << summaryLine() << kColorReset;
  out << "\n";
  out << std::string(std::max(0, size.columns), '=') << '\n';

  switch (page_) {
    case Page::Dashboard:
      renderDashboard(out, size);
      break;
    case Page::Stock:
      renderStock(out, size);
      break;
    case Page::Detail:
      renderDetail(out, size);
      break;
  }

  renderSearchBar(out, size);
  renderMessage(out, size);
  renderStatusBar(out, size);

  std::ostringstream screen;
  screen << "\x1b[H";

  int renderedLines = 0;
  const auto frame = out.str();
  for (char ch : frame) {
    screen << ch;
    if (ch == '\n') {
      screen << "\x1b[K";
      ++renderedLines;
    }
  }

  while (renderedLines < size.rows) {
    screen << "\x1b[K\n";
    ++renderedLines;
  }

  std::cout << screen.str() << std::flush;
}

void App::renderDashboard(std::ostringstream& out, const ConsoleSize& size) {
  const auto summary = summarize(store_.items());
  const int contentRows = std::max(6, size.rows - 6);

  out << kColorAccent << "Dashboard" << kColorReset << "  ";
  out << kColorMuted << "Overview, alerts, and activity" << kColorReset << '\n';
  out << std::string(std::max(0, size.columns), '-') << '\n';

  if (size.columns < 96) {
    std::vector<std::string> lines;
    lines.push_back(styleText("Summary", kColorAccent));
    lines.push_back(styleText("  Parts: " + std::to_string(summary.itemCount), kColorInfo));
    lines.push_back(styleText("  Units: " + std::to_string(summary.totalUnits), kColorSuccess));
    lines.push_back(styleText("  Low stock: " + std::to_string(summary.lowStockCount), kColorWarn));
    lines.push_back(styleText("  Missing metadata: " + std::to_string(summary.missingMetadataCount), kColorDanger));
    lines.push_back(styleText("  Unsynced: " + std::to_string(summary.unsyncedCount), kColorLabel));
    lines.push_back("");
    lines.push_back(styleText("Scanner", kColorTitle));
    appendWrappedStyled(lines, scannerUrl(), size.columns, kColorLink);
    lines.push_back("");
    lines.push_back(styleText("Inventory file", kColorTitle));
    appendWrappedStyled(lines, inventoryPath_.string(), size.columns, kColorInfo);
    lines.push_back("");
    lines.push_back(styleText("Low-stock alerts", kColorWarn));
    int alertCount = 0;
    for (const auto& item : store_.items()) {
      if (!item.lowStock()) {
        continue;
      }
      appendWrappedStyled(lines,
                          "  - " + item.partName + " qty " + std::to_string(item.quantity) + " / " +
                              std::to_string(item.reorderThreshold) + "  [" + item.category + "]",
                          size.columns, kColorWarn);
      if (++alertCount >= 4) {
        break;
      }
    }
    if (alertCount == 0) {
      lines.push_back(styleText("  No low-stock items.", kColorMuted));
    }

    lines.push_back("");
    lines.push_back(styleText("Recent activity", kColorAccent));
    const auto activityCount = std::min<std::size_t>(activities_.size(), 5);
    for (std::size_t offset = 0; offset < activityCount; ++offset) {
      const auto& entry = activities_[activities_.size() - 1 - offset];
      const char* activityColor = kColorMuted;
      if (entry.kind.find("scan") != std::string::npos) {
        activityColor = kColorLink;
      } else if (entry.kind.find("add") != std::string::npos) {
        activityColor = kColorSuccess;
      } else if (entry.kind.find("edit") != std::string::npos) {
        activityColor = kColorInfo;
      } else if (entry.kind == "system") {
        activityColor = kColorLabel;
      }
      appendWrappedStyled(lines,
                          "  - " + nowTimestampString(entry.timestamp) + " | " + entry.kind + " | " + entry.message,
                          size.columns, activityColor);
    }
    if (activityCount == 0) {
      lines.push_back(styleText("  No recent activity.", kColorMuted));
    }

    lines.push_back("");
    lines.push_back(styleText("Quick actions", kColorAccent));
    appendWrappedStyled(lines, "  [1] Stock browser   [2/s] Scanner   [3] Add item   [4] Reload   [/] Search   [q] Quit", size.columns,
                        kColorDim);

    if (static_cast<int>(lines.size()) > contentRows) {
      lines.resize(static_cast<std::size_t>(contentRows));
    }
    for (const auto& line : lines) {
      out << line << '\n';
    }
    return;
  }

  const int leftWidth = std::max(36, (size.columns - 4) / 2);
  const int rightWidth = std::max(36, size.columns - leftWidth - 4);

  std::vector<std::string> leftLines;
  std::vector<std::string> rightLines;

  leftLines.push_back("Summary");
  leftLines.push_back("  Parts: " + std::to_string(summary.itemCount));
  leftLines.push_back("  Units: " + std::to_string(summary.totalUnits));
  leftLines.push_back("  Low stock: " + std::to_string(summary.lowStockCount));
  leftLines.push_back("  Missing metadata: " + std::to_string(summary.missingMetadataCount));
  leftLines.push_back("  Unsynced: " + std::to_string(summary.unsyncedCount));
  leftLines.push_back("");
  leftLines.push_back("Scanner");
  appendWrapped(leftLines, scannerUrl(), leftWidth);
  leftLines.push_back("");
  leftLines.push_back("Inventory file");
  appendWrapped(leftLines, inventoryPath_.string(), leftWidth);
  leftLines.push_back("");
  leftLines.push_back("Quick actions");
  appendWrapped(leftLines, "1 Stock browser   2/s Scanner   3 Add item   4 Reload   / Search   q Quit", leftWidth);

  rightLines.push_back("Low-stock alerts");
  int alertCount = 0;
  for (const auto& item : store_.items()) {
    if (!item.lowStock()) {
      continue;
    }
    appendWrapped(rightLines,
                  "  - " + item.partName + " qty " + std::to_string(item.quantity) + " / " +
                      std::to_string(item.reorderThreshold) + "  [" + item.category + "]",
                  rightWidth);
    if (++alertCount >= 6) {
      break;
    }
  }
  if (alertCount == 0) {
    rightLines.push_back("  No low-stock items.");
  }

  rightLines.push_back("");
  rightLines.push_back("Recent activity");
  const auto activityCount = std::min<std::size_t>(activities_.size(), 8);
  for (std::size_t offset = 0; offset < activityCount; ++offset) {
    const auto& entry = activities_[activities_.size() - 1 - offset];
    appendWrapped(rightLines,
                  "  - " + nowTimestampString(entry.timestamp) + " | " + entry.kind + " | " + entry.message,
                  rightWidth);
  }
  if (activityCount == 0) {
    rightLines.push_back("  No recent activity.");
  }

  renderColumns(out, leftLines, rightLines, leftWidth, rightWidth, contentRows, 4, kColorInfo, kBgPanelLeft,
                kColorMuted, kBgPanelRight);
}

void App::renderStock(std::ostringstream& out, const ConsoleSize& size) {
  const auto filtered = filteredIndices();
  const int contentRows = std::max(6, size.rows - 6);

  out << kColorAccent << "Stock browser" << kColorReset << "  ";
  out << kColorMuted << "(j/k or arrows to move, Enter detail, e edit, n new, +/- adjust)" << kColorReset << '\n';
  out << std::string(std::max(0, size.columns), '-') << '\n';

  struct StockLine {
    std::string text;
    bool isHeader = false;
    bool isMessage = false;
    bool isSelected = false;
    bool isLowStock = false;
    bool isEven = false;
  };

  const bool compact = size.columns < 96;
  const int listHeight = compact ? std::max(4, contentRows / 2) : std::max(4, contentRows - 1);
  const int listWidth = compact ? std::max(0, size.columns) : std::max(36, (size.columns - 4) / 3);
  const int detailWidth = compact ? std::max(40, size.columns - 4) : std::max(40, size.columns - listWidth - 4);
  const int partWidth = std::max(12, listWidth - 21);

  std::vector<StockLine> listLines;
  listLines.push_back({"Items", true});
  listLines.push_back({"Part                          Category       Qty", true});

  if (filtered.empty()) {
    listLines.push_back({"No items match \"" + searchQuery_ + "\".", false, true});
  } else {
    std::size_t visibleStart = 0;
    if (selectedPosition_ >= static_cast<std::size_t>(listHeight)) {
      visibleStart = selectedPosition_ - static_cast<std::size_t>(listHeight) + 1;
    }
    if (selectedPosition_ < visibleStart) {
      visibleStart = selectedPosition_;
    }

    stockScroll_ = visibleStart;

    for (int row = 0; row < listHeight; ++row) {
      const auto pos = visibleStart + static_cast<std::size_t>(row);
      if (pos >= filtered.size()) {
        break;
      }

      const auto& item = store_.items()[filtered[pos]];
      std::ostringstream rowText;
      rowText << (pos == selectedPosition_ ? '>' : ' ') << ' ' << padRight(item.partName, partWidth) << "  "
              << padRight(item.category, 11) << ' ' << std::setw(4) << item.quantity;
      listLines.push_back({rowText.str(), false, false, pos == selectedPosition_, item.lowStock(), row % 2 == 0});
    }
  }

  std::vector<StockLine> detailLines;
  detailLines.push_back({"Detail", true});
  if (const auto* item = selectedItem()) {
    const auto lines = splitLines(itemDetailText(*item, detailWidth));
    for (const auto& line : lines) {
      detailLines.push_back({line});
    }
  } else {
    detailLines.push_back({"No selection.", false, true});
  }

  auto styleListLine = [&](const StockLine& line) {
    if (line.isHeader) {
      return styleCell(line.text, listWidth, kColorAccent, kBgPanelLeft);
    }
    if (line.isMessage) {
      return styleCell(line.text, listWidth, kColorMuted, kBgPanelLeft);
    }
    if (line.isSelected) {
      return styleCell(line.text, listWidth, kColorTitle, kBgRowSelected);
    }
    const char* fg = line.isLowStock ? kColorWarn : nullptr;
    const char* bg = line.isEven ? kBgRowDark : kBgRowLight;
    return styleCell(line.text, listWidth, fg, bg);
  };

  auto styleDetailLine = [&](const StockLine& line) {
    if (line.isHeader) {
      return styleCell(line.text, detailWidth, kColorAccent, kBgPanelRight);
    }
    if (line.isMessage) {
      return styleCell(line.text, detailWidth, kColorMuted, kBgPanelRight);
    }
    if (line.text.rfind("Quantity: ", 0) == 0) {
      return styleCell(line.text, detailWidth, kColorSuccess, kBgPanelRight);
    }
    if (line.text.find(": ") != std::string::npos) {
      return styleCell(line.text, detailWidth, kColorInfo, kBgPanelRight);
    }
    return styleCell(line.text, detailWidth, kColorMuted, kBgPanelRight);
  };

  if (compact) {
    for (const auto& line : listLines) {
      out << styleListLine(line) << '\n';
    }
    out << '\n';
    for (const auto& line : detailLines) {
      out << styleDetailLine(line) << '\n';
    }
    return;
  }

  const std::size_t maxRows = std::max(listLines.size(), detailLines.size());
  for (std::size_t row = 0; row < maxRows; ++row) {
    const auto left = row < listLines.size() ? styleListLine(listLines[row]) : styleCell("", listWidth, nullptr, kBgPanelLeft);
    const auto right =
        row < detailLines.size() ? styleDetailLine(detailLines[row]) : styleCell("", detailWidth, nullptr, kBgPanelRight);
    out << left << "    " << right << '\n';
  }
}

void App::renderDetail(std::ostringstream& out, const ConsoleSize& size) {
  const int contentRows = std::max(6, size.rows - 6);

  out << kColorAccent << "Item detail" << kColorReset << "  " << kColorMuted << "(Esc back to stock)" << kColorReset
      << '\n';
  out << std::string(std::max(0, size.columns), '-') << '\n';

  if (size.columns < 96) {
    if (const auto* item = selectedItem()) {
      std::vector<std::string> lines;
      const auto electricalFields = electricalFieldsForItem(*item);
      lines.push_back(styleText("Core details", kColorAccent));
      for (const auto& field : detailCoreFields(*item)) {
        lines.push_back(styleText("  " + field.label + field.value, kColorInfo));
      }
      if (!electricalFields.empty()) {
        lines.push_back("");
        lines.push_back(styleText("Electrical parameters", kColorAccent));
        for (const auto& field : electricalFields) {
          lines.push_back(styleText("  " + field.label + field.value, kColorInfo));
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
      appendWrappedStyled(lines, item->notes, std::max(50, size.columns - 4), kColorMuted);
      lines.push_back("");
      lines.push_back(styleText("Shortcuts", kColorAccent));
      appendWrappedStyled(lines, "e Edit   n New   + / - Quantity   Enter Open detail   / Search", std::max(50, size.columns - 4),
                          kColorDim);
      for (const auto& line : lines) {
        out << line << '\n';
      }
    } else {
      out << "No item selected.\n";
    }
    return;
  }

  const int rightWidth = std::max(46, std::min(size.columns - 4, static_cast<int>(size.columns * 0.42)));
  const int leftWidth = std::max(40, size.columns - rightWidth - 4);

  std::vector<std::string> leftLines;
  std::vector<std::string> rightLines;

  if (const auto* item = selectedItem()) {
    const auto electricalFields = electricalFieldsForItem(*item);
    leftLines.push_back("Core details");
    for (const auto& field : detailCoreFields(*item)) {
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
    rightLines.push_back("");
    rightLines.push_back("Shortcuts");
    appendWrapped(rightLines, "e Edit   n New   + / - Quantity   Enter Open detail   / Search", rightWidth);
  } else {
    leftLines.push_back("No item selected.");
    rightLines.push_back("Press Esc to return to stock.");
  }

  renderColumns(out, leftLines, rightLines, leftWidth, rightWidth, contentRows, 4, kColorInfo, kBgPanelLeft,
                kColorMuted, kBgPanelRight);
}

void App::renderSearchBar(std::ostringstream& out, const ConsoleSize&) {
  out << '\n' << kColorAccent << "Search" << kColorReset << "  ";
  if (inputMode_ == InputMode::Search) {
    out << kColorLink << '/' << inputBuffer_ << '_' << kColorReset;
  } else {
    out << kColorMuted << '/' << searchQuery_ << kColorReset;
  }
  out << '\n';

  if (inputMode_ == InputMode::EditFieldMenu) {
    out << kColorAccent << "Edit fields" << kColorReset << '\n';
    for (std::size_t index = 0; index < menuOptions_.size(); ++index) {
      if (static_cast<int>(index) == fieldMenuIndex_) {
        out << kColorSelect;
      }
      out << "  [" << (index < 9 ? std::to_string(index + 1) : "0") << "] " << menuOptions_[index].label
          << kColorReset << '\n';
    }
  } else if (inputMode_ == InputMode::EditValue) {
    out << kColorAccent << "Input" << kColorReset << "  " << kColorLabel << activePrompt() << kColorReset
        << kColorLink << inputBuffer_ << '_' << kColorReset << '\n';
  }
}

void App::renderStatusBar(std::ostringstream& out, const ConsoleSize& size) {
  out << std::string(size.columns, '-') << '\n';
  out << kColorLink << scannerUrl() << kColorReset << "  ";
  out << kColorDim << "Dashboard: Tab/1  Stock: arrows/jk  Detail: Enter  Edit: e  New: n  Scanner: s  Quit: q" << kColorReset << '\n';
}

void App::renderMessage(std::ostringstream& out, const ConsoleSize&) {
  if (!message_.empty()) {
    out << kColorAccent << message_ << kColorReset << '\n';
  }
}

void App::setMessage(std::string text, int seconds) {
  message_ = std::move(text);
  messageUntil_ = std::time(nullptr) + seconds;
  dirty_ = true;
}

bool App::messageVisible() const {
  return !message_.empty() && std::time(nullptr) <= messageUntil_;
}

void App::clearMessageIfExpired() {
  if (!messageVisible() && !message_.empty()) {
    message_.clear();
    dirty_ = true;
  }
}

void App::markDirty() {
  dirty_ = true;
}

std::vector<std::size_t> App::filteredIndices() const {
  return filterItems(store_.items(), searchQuery_);
}

std::size_t App::selectedIndex() const {
  const auto filtered = filteredIndices();
  if (filtered.empty()) {
    return std::numeric_limits<std::size_t>::max();
  }
  const auto position = std::min(selectedPosition_, filtered.size() - 1);
  return filtered[position];
}

InventoryItem* App::selectedItem() {
  const auto index = selectedIndex();
  if (index == std::numeric_limits<std::size_t>::max()) {
    return nullptr;
  }
  return &store_.items()[index];
}

const InventoryItem* App::selectedItem() const {
  const auto index = selectedIndex();
  if (index == std::numeric_limits<std::size_t>::max()) {
    return nullptr;
  }
  return &store_.items()[index];
}

void App::syncSelectionToFilter() {
  const auto filtered = filteredIndices();
  if (filtered.empty()) {
    selectedPosition_ = 0;
    return;
  }
  if (selectedPosition_ >= filtered.size()) {
    selectedPosition_ = filtered.size() - 1;
  }
  dirty_ = true;
}

void App::moveSelection(int delta) {
  const auto filtered = filteredIndices();
  if (filtered.empty()) {
    selectedPosition_ = 0;
    return;
  }

  const auto current = static_cast<int>(std::min(selectedPosition_, filtered.size() - 1));
  const auto next = std::clamp(current + delta, 0, static_cast<int>(filtered.size() - 1));
  selectedPosition_ = static_cast<std::size_t>(next);
  dirty_ = true;
}

void App::changePage(Page page) {
  page_ = page;
  inputMode_ = InputMode::None;
  dirty_ = true;
}

void App::openSelectedDetail() {
  if (selectedItem() != nullptr) {
    page_ = Page::Detail;
    dirty_ = true;
  }
}

void App::startSearch() {
  page_ = Page::Stock;
  inputMode_ = InputMode::Search;
  inputBuffer_ = searchQuery_;
  setMessage("Type a keyword, category, tag, parameter, or qty filter", 3);
}

void App::cancelInput() {
  inputMode_ = InputMode::None;
  inputBuffer_.clear();
  dirty_ = true;
}

void App::beginEditCurrentItem(bool createNew) {
  page_ = Page::Stock;
  workingCopy_ = {};
  if (createNew) {
    workingCopy_.isNew = true;
    workingCopy_.item.id = makeId();
    workingCopy_.item.partName = "New Part";
    workingCopy_.item.manufacturer = "Unknown";
    workingCopy_.item.category = "Unsorted";
    workingCopy_.item.location = "Unassigned";
    workingCopy_.item.syncStatus = "needs_metadata";
    workingCopy_.item.lastUpdated = std::time(nullptr);
    workingCopy_.originalIndex = store_.items().size();
  } else {
    const auto* current = selectedItem();
    if (current == nullptr) {
      setMessage("No item selected", 2);
      return;
    }
    workingCopy_.isNew = false;
    workingCopy_.item = *current;
    workingCopy_.originalIndex = selectedIndex();
  }

  menuOptions_ = fieldOptions();
  fieldMenuIndex_ = 0;
  inputMode_ = InputMode::EditFieldMenu;
  setMessage("Choose a field to edit", 3);
}

void App::openFieldMenu() {
  menuOptions_ = fieldOptions();
  fieldMenuIndex_ = 0;
  inputMode_ = InputMode::EditFieldMenu;
}

void App::commitEditField(EditField field, const std::string& value) {
  const auto trimmed = trim(value);
  bool valid = true;

  switch (field) {
    case EditField::PartName:
      workingCopy_.item.partName = trimmed;
      break;
    case EditField::Manufacturer:
      workingCopy_.item.manufacturer = trimmed;
      break;
    case EditField::Category:
      workingCopy_.item.category = trimmed;
      break;
    case EditField::Quantity:
      try {
        workingCopy_.item.quantity = std::max(0, std::stoi(trimmed));
      } catch (...) {
        valid = false;
      }
      break;
    case EditField::ReorderThreshold:
      try {
        workingCopy_.item.reorderThreshold = std::max(0, std::stoi(trimmed));
      } catch (...) {
        valid = false;
      }
      break;
    case EditField::Location:
      workingCopy_.item.location = trimmed;
      break;
    case EditField::Tags:
      workingCopy_.item.tags = splitFlexible(trimmed);
      break;
    case EditField::Parameters:
      workingCopy_.item.parameters = parseParameters(trimmed);
      break;
    case EditField::Notes:
      workingCopy_.item.notes = trimmed;
      break;
    case EditField::DigiKeyPart:
      workingCopy_.item.digikeyPartNumber = trimmed;
      break;
    case EditField::DatasheetUrl:
      workingCopy_.item.datasheetUrl = trimmed;
      break;
    case EditField::ProductUrl:
      workingCopy_.item.productUrl = trimmed;
      break;
    case EditField::Sku:
      workingCopy_.item.sku = trimmed;
      break;
    case EditField::SyncStatus:
      workingCopy_.item.syncStatus = toLower(trimmed);
      break;
  }

  if (!valid) {
    setMessage("Invalid numeric value", 3);
    return;
  }

  workingCopy_.item.lastUpdated = std::time(nullptr);
  setMessage(fieldLabel(field) + " updated", 2);
  inputBuffer_.clear();
  inputMode_ = InputMode::EditFieldMenu;
  dirty_ = true;
}

void App::saveWorkingCopy() {
  if (workingCopy_.isNew) {
    store_.items().push_back(workingCopy_.item);
    selectedPosition_ = store_.items().empty() ? 0 : store_.items().size() - 1;
  } else if (workingCopy_.originalIndex < store_.items().size()) {
    store_.items()[workingCopy_.originalIndex] = workingCopy_.item;
  }

  logActivity("edit", workingCopy_.item.partName + " updated");
  saveState();
  inputMode_ = InputMode::None;
  page_ = Page::Stock;
  syncSelectionToFilter();
  setMessage("Changes saved", 2);
}

void App::adjustQuantity(int delta) {
  auto* item = selectedItem();
  if (item == nullptr) {
    setMessage("No item selected", 2);
    return;
  }

  item->quantity = std::max(0, item->quantity + delta);
  item->lastUpdated = std::time(nullptr);
  logActivity(delta > 0 ? "stock" : "usage", item->partName + " quantity changed to " + std::to_string(item->quantity));
  saveState();
  setMessage(item->partName + " quantity is now " + std::to_string(item->quantity), 2);
  dirty_ = true;
}

void App::logActivity(const std::string& kind, const std::string& message) {
  appendActivity(activities_, makeActivity(kind, message));
  server_.setRecentActivity(activities_);
  saveActivities(activityPath_, activities_);
  dirty_ = true;
}

void App::pushScanCode(const std::string& code) {
  std::lock_guard<std::mutex> lock(scanMutex_);
  scanQueue_.push_back(code);
}

void App::processScans() {
  std::vector<std::string> pending;
  {
    std::lock_guard<std::mutex> lock(scanMutex_);
    pending.swap(scanQueue_);
  }

  for (const auto& code : pending) {
    const auto resolution = resolveScanCode(store_, code);
    if (resolution.matched) {
      if (resolution.created) {
        logActivity("scan", "Created item from code " + code);
      } else {
        logActivity("scan", "Matched existing item with code " + code);
      }

      if (const auto* item = store_.findById(resolution.itemId)) {
        const auto it = std::find_if(store_.items().begin(), store_.items().end(), [&](const InventoryItem& entry) {
          return entry.id == item->id;
        });
        if (it != store_.items().end()) {
          selectedPosition_ = static_cast<std::size_t>(std::distance(store_.items().begin(), it));
        }
      }

      saveState();
      changePage(Page::Detail);
      setMessage(resolution.message, 3);
    } else {
      setMessage("Scan ignored: " + resolution.message, 3);
    }
    syncSelectionToFilter();
  }
}

void App::openCurrentUrl(const std::string& url, const std::string& label) {
  if (trim(url).empty()) {
    setMessage("No " + label + " link stored for this item", 3);
    return;
  }
  if (openUrl(url)) {
    setMessage("Opened " + label + " link", 2);
  } else {
    setMessage("Unable to open " + label + " link", 3);
  }
}

std::string App::fieldLabel(EditField field) const {
  switch (field) {
    case EditField::PartName:
      return "Part name";
    case EditField::Manufacturer:
      return "Manufacturer";
    case EditField::Category:
      return "Category";
    case EditField::Quantity:
      return "Quantity";
    case EditField::ReorderThreshold:
      return "Reorder threshold";
    case EditField::Location:
      return "Location";
    case EditField::Tags:
      return "Tags";
    case EditField::Parameters:
      return "Parameters";
    case EditField::Notes:
      return "Notes";
    case EditField::DigiKeyPart:
      return "DigiKey part";
    case EditField::DatasheetUrl:
      return "Datasheet URL";
    case EditField::ProductUrl:
      return "Product URL";
    case EditField::Sku:
      return "SKU";
    case EditField::SyncStatus:
      return "Sync status";
  }
  return "Field";
}

std::string App::currentFieldValue(EditField field) const {
  const auto* item = workingCopy_.item.id.empty() && !workingCopy_.isNew ? selectedItem() : &workingCopy_.item;
  if (item == nullptr) {
    return {};
  }

  switch (field) {
    case EditField::PartName:
      return item->partName;
    case EditField::Manufacturer:
      return item->manufacturer;
    case EditField::Category:
      return item->category;
    case EditField::Quantity:
      return std::to_string(item->quantity);
    case EditField::ReorderThreshold:
      return std::to_string(item->reorderThreshold);
    case EditField::Location:
      return item->location;
    case EditField::Tags:
      return join(item->tags, ',');
    case EditField::Parameters: {
      std::ostringstream out;
      for (std::size_t index = 0; index < item->parameters.size(); ++index) {
        if (index > 0) {
          out << "; ";
        }
        out << item->parameters[index].name << '=' << item->parameters[index].value;
      }
      return out.str();
    }
    case EditField::Notes:
      return item->notes;
    case EditField::DigiKeyPart:
      return item->digikeyPartNumber;
    case EditField::DatasheetUrl:
      return item->datasheetUrl;
    case EditField::ProductUrl:
      return item->productUrl;
    case EditField::Sku:
      return item->sku;
    case EditField::SyncStatus:
      return item->syncStatus;
  }

  return {};
}

std::vector<App::FieldOption> App::fieldOptions() const {
  return {
      {"Part name", EditField::PartName},
      {"Manufacturer", EditField::Manufacturer},
      {"Category", EditField::Category},
      {"Quantity", EditField::Quantity},
      {"Reorder threshold", EditField::ReorderThreshold},
      {"Location", EditField::Location},
      {"Tags", EditField::Tags},
      {"Parameters", EditField::Parameters},
      {"Notes", EditField::Notes},
      {"DigiKey part", EditField::DigiKeyPart},
      {"Datasheet URL", EditField::DatasheetUrl},
      {"Product URL", EditField::ProductUrl},
      {"SKU", EditField::Sku},
      {"Sync status", EditField::SyncStatus},
      {"Save changes", EditField::PartName},
      {"Cancel", EditField::PartName},
  };
}

std::string App::itemDetailText(const InventoryItem& item, int width) const {
  std::ostringstream out;
  const auto fields = stockPreviewFields(item);
  for (const auto& field : fields) {
    const auto line = field.label + field.value;
    for (const auto& wrapped : wrapText(line, width)) {
      out << wrapped << '\n';
    }
  }

  return out.str();
}

std::string App::summaryLine() const {
  const auto summary = summarize(store_.items());
  std::ostringstream out;
  out << summary.itemCount << " items"
      << " | " << summary.totalUnits << " units"
      << " | " << summary.lowStockCount << " low"
      << " | " << summary.missingMetadataCount << " missing metadata"
      << " | " << summary.unsyncedCount << " unsynced";
  return out.str();
}

std::string App::scannerUrl() const {
  if (!server_.running() || server_.port() == 0) {
    return "scanner unavailable";
  }
  return server_.baseUrl() + "/";
}

std::string App::activePrompt() const {
  if (inputMode_ == InputMode::EditValue && fieldMenuIndex_ >= 0 && fieldMenuIndex_ < static_cast<int>(menuOptions_.size())) {
    return fieldLabel(menuOptions_[fieldMenuIndex_].field) + ": ";
  }
  return "";
}

}  // namespace hims
