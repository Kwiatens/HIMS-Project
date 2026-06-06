# HIMS - Hardware Inventory Managment System Software

HIMS is a lightweight, open-source, terminal based Hardware Inventory Management System. It keeps track of all the hardware parts you own, alerts you when a part is running out/is out of stock, includes a label printer automation for 'HIMS Scan R1', which allows rapid and easy changes to the part quantity without needing to constantly type stuff on the PC. 

It is ideal for people who design/assemble PCBs or hardware projects.

## What it does

- Shows a dashboard with inventory health, low-stock alerts, and recent activity.
- Browses stock in a split-pane master/detail layout.
- Filters instantly by keyword, category, tag, parameter, quantity, SKU, status, or location.
- Lets you adjust stock, edit item records, and open DigiKey or datasheet links.
- Hosts a local scanner page for mobile DigiKey 2D code intake.

## Build

```bash
cmake -S . -B build
cmake --build build
```

Or build and launch it in a visible desktop window:

```bash
.\run.ps1
```

If you want to launch the executable directly, it will usually be in `build\Debug\hims.exe`.

## Controls

- `Tab` or `1`: dashboard to stock browser
- `j` / `k` or arrows: move selection
- `Enter`: open item detail
- `/`: search/filter
- `e`: edit selected item
- `n`: create a new item
- `+` / `-`: adjust quantity
- `d`: open datasheet
- `o`: open product page
- `g`: open DigiKey search
- `s`: open the local scanner page
- `Esc`: go back
- `q`: quit

## Tests

```bash
cmake --build build --target hims_tests
build\Debug\hims_tests.exe
```
