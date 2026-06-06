#pragma once

#include <string>
#include <vector>

namespace hims {

struct ConsoleSize {
  int columns = 120;
  int rows = 40;
};

enum class KeyType {
  Character,
  Enter,
  Escape,
  Backspace,
  Tab,
  Up,
  Down,
  Left,
  Right,
  Home,
  End,
  PageUp,
  PageDown,
  Delete,
  Unknown,
};

struct KeyEvent {
  KeyType type = KeyType::Unknown;
  char ch = '\0';
};

class ConsoleSession {
 public:
  ConsoleSession();
  ~ConsoleSession();

  void restore();

  ConsoleSession(const ConsoleSession&) = delete;
  ConsoleSession& operator=(const ConsoleSession&) = delete;

 private:
  bool active_ = false;
  bool alternateScreen_ = false;
};

ConsoleSize consoleSize();
void clearConsole();
void hideCursor();
void showCursor();
void setConsoleTitle(const std::string& title);
bool openUrl(const std::string& url);
std::vector<std::string> localAddresses();
std::vector<KeyEvent> pollKeys();

}  // namespace hims
