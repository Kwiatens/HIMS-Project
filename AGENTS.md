# HIMS Agent Notes

## What This Software Is

HIMS is a lightweight, keyboard-first Hardware Inventory Management System built in C++ as a terminal application.

The goals are:

- fast navigation with minimal clutter
- instant search and filtering
- a clear dashboard for system health and alerts
- a split-pane stock browser with item details on the same screen
- a local companion web page for mobile DigiKey 2D code scanning

## Main Behaviors

- The dashboard is the landing page.
- The stock browser shows part name, category, and quantity in the list, with full details for the selected item.
- Search should work by keyword, category, tag, parameter, location, SKU, status, and quantity filters.
- Item details should include DigiKey links, datasheet links, and metadata sync status.
- The scanner page should run locally and pass scanned codes back to the terminal app.

## Design Principles

- Prefer speed over visual complexity.
- Keep interaction keyboard-first.
- Keep the UI industrial, clean, and readable.
- Avoid adding unnecessary layers or frameworks.
- Favor clear data flow and simple state handling.

## Data And Files

- Inventory data is stored locally in `Documents/HIMS/inventory.db`.
- Activity history is stored locally in `Documents/HIMS/activity.tsv`.
- On first launch, the app copies the existing OG inventory database if it is available, so the inventory can be reused without rescanning.

## Build And Run

Typical local workflow:

```powershell
cmake -S . -B build
cmake --build build
.\run.ps1
```

Tests:

```powershell
.\build\Debug\hims_tests.exe
```

## Required Agent Workflow

After each finished change, always:

1. Build the software.
2. Execute the software in a visible desktop window.
3. Verify the change behaves as intended.

If the change affects core inventory behavior, also run the test executable.

If the app is already running and the linker cannot overwrite the executable, close the running instance first and then rebuild.

## Test Environment Notes

- During verification, the local shell runner can occasionally fail before command startup with `windows sandbox: spawn setup refresh`.
- If that happens, rerun the build or test command through the escalated shell path instead of spending time on the failing local runner.
- When debugging SQLite/database behavior, prefer a direct test binary run or a small external probe against `Documents/HIMS/inventory.db` so failures are easier to isolate.

## When Editing

- Keep the codebase lightweight.
- Preserve the existing keyboard shortcuts unless a change explicitly improves them.
- Prefer incremental changes that keep the terminal responsive.
- If you touch rendering code, verify that the screen does not flicker and that redraws are only triggered when needed.
