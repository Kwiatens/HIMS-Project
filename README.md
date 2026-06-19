# HIMS - Hardware Inventory Managment System
**This project is WiP (Work-in-Progress) do not rely on it yet, expect many bugs and unfinished features!**
<img width="1917" height="998" alt="image" src="https://github.com/user-attachments/assets/3727ea66-8c75-4ca9-92cf-aeaf053a65de" />

HIMS is a lightweight, open-source, terminal based Hardware Inventory Management System. It keeps track of all the hardware parts you own, alerts you when a part is running out/is out of stock, includes a label printer automation, and a open-source device called 'HIMS Scan' that you can build yourself - it allows rapid and effortless changes to the part quantity without needing to constantly input the changes on your computer.

The system is ideal for people who design/assemble PCBs or hardware projects and don't want to fiddle around with slow GUI based software that needs a computer to input data.

## Features
- Keeps track of all your parts and their quantity.
- Alerts you with low/out of stock warnings.
- Features a DigiKey API integration to display all the parameters about a part.
- Order import (CSV) from DigiKey.
- ZPL Label printer integration for custom 'HIMS labels' for use with 'HIMS Scan' device.
- Filters instantly by keyword, category, tag, parameter, quantity, SKU, status, or location.
- Lets you adjust stock, edit item records, and open store or datasheet links.
- Hosts a local scanner page for mobile DigiKey 2D code intake if you haven't build a 'HIMS Scan'

## Future plans
- KiCAD integration
- Integration with more component stores.

## Build

```bash
cmake -S . -B build
cmake --build build
```

If you want to launch the executable directly, it will usually be in `build\Debug\hims.exe`.

## Tests

```bash
cmake --build build --target hims_tests
build\Debug\hims_tests.exe
```

## Legal
The HIMS name, logo, and official product branding are not licensed under the open-source licenses in this repository. They remain trademarks/branding assets of the project owner and may not be used to imply official endorsement or origin.

Copyright © 2026 Paweł Kwiatkowski. All rights reserved.
