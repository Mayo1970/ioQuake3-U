# ioQuake3-U

A **native Wii U** (Cafe OS / [`wut`](https://github.com/devkitPro/wut) / GX2) port of
[ioQuake3](https://github.com/ioquake/ioq3). Using a custom GX2 rendering backend built on top of the
stock `renderergl1` frontend, and launches from the Wii U Menu via
[Aroma](https://aroma.foryour.cafe/).

This is a **separate project** from the [Wii port](https://github.com/Mayo1970/ioquake3-wii) (libogc,
`.dol`, runs under the Wii Virtual Console) — they share only platform-agnostic
engine/game code. Native mode unlocks what Wii structurally cannot: ~1 GB of RAM
instead of an 88 MB budget, and three 1.24 GHz Espresso cores instead of one
729 MHz Broadway core.

## Status

- Boots, loads maps, and plays with bots — world geometry, lightmaps, and
  entity/model rendering (weapons, players, items) all render through the stock
  `renderergl1` pipeline driving a native GX2 backend
- Five build flavors: **ioQuake3-U** (Q3A), **Team Arena-U**, **Open Arena-U**,
  **Quake3Classic-U** (Dreamcast-protocol crossplay), and **Elite Force-U** —
  each its own `.wuhb` with its own name and icon
- GamePad (VPAD), Pro Controller / Classic Controller (KPAD), and Wii Remote +
  Nunchuk (KPAD) input, with tunable stick deadzone/response curve and DRC
  touchscreen look
- GamePad rumble on own weapon fire, own pain, and hit feedback
- Default player name is taken from your Wii U Mii nickname on first boot

## Prerequisites

Build from the **devkitPro MSYS2 shell**, but invoke `make` from **PowerShell**
(see [Building](#building) — plain MSYS2 bash breaks `gcc.exe` on this setup).

1. Install [devkitPro](https://github.com/devkitPro/installer/releases/latest).
2. Select `devkitPPC` and `wut` (Wii U development).
3. Install the PowerPC portlibs used for JPEG/zlib decoding:

```bash
pacman -S ppc-zlib ppc-libjpeg-turbo
```

No sibling clone, no patch step — the engine source is vendored directly under
`code/`.

## Building

```powershell
$env:DEVKITPRO = "C:/devkitPro"
$env:DEVKITPPC = "C:/devkitPro/devkitPPC"
$env:WUT_ROOT  = "C:/devkitPro/wut"
$env:PATH = "C:\devkitpro\devkitPPC\bin;C:\devkitpro\tools\bin;C:\devkitpro\msys2\usr\bin;$env:PATH"

C:\devkitpro\msys2\usr\bin\make.exe -f Makefile.client all              # ioQuake3-U
C:\devkitpro\msys2\usr\bin\make.exe -f Makefile.client TA=1 ta          # Team Arena-U
C:\devkitpro\msys2\usr\bin\make.exe -f Makefile.client OA=1 oa          # Open Arena-U
C:\devkitpro\msys2\usr\bin\make.exe -f Makefile.client CLASSIC=1 classic # Quake3Classic-U
C:\devkitpro\msys2\usr\bin\make.exe -f Makefile.client EF=1 ef          # Elite Force-U
C:\devkitpro\msys2\usr\bin\make.exe -f Makefile.client all-flavors      # all five
C:\devkitpro\msys2\usr\bin\make.exe -f Makefile.client clean
```
Each flavor builds into its own `build_client*` directory, so switching between
them never requires `make clean`.

## Installing

Copy each `.wuhb` anywhere under `sd:/wiiu/apps/` on the SD card — Aroma scans
that tree recursively and lists each flavor as its own tile with its own name
and icon.

**You need the original game data** — where to get it and the exact SD card
layout for each of the five flavors (ioQuake3-U, Team Arena-U, Open Arena-U,
Quake3Classic-U, Elite Force-U) is covered in
**[INSTALLATION.md](INSTALLATION.md)**.

Game data, logs, configs, and the qkey all live under `sd:/quake3/` — separate
from wherever the `.wuhb` itself is installed.

## Controls

All available input methods are active at once — GamePad, Pro Controller,
Classic Controller, and Wii Remote + Nunchuk can be freely mixed.

### GamePad / Pro Controller / Classic Controller

| Input | Action |
|---|---|
| Left stick | Move (forward/back + strafe) |
| Right stick | Look (yaw + pitch) |
| **ZR** | Fire |
| **ZL** | Zoom |
| **A** | Jump |
| **B** | Crouch |
| **X** | Next weapon |
| **Y** | Previous weapon / toggle console |
| **L** | Walk |
| **R** | Use item |
| **Left stick click / Right stick click / Minus** | Scoreboard |
| **D-pad** | Weapon prev/next |
| **Plus** | Menu (Escape) |
| **DRC touchscreen** | Swipe to look (works in menus too as a cursor) |
| **Left stick click + X** | Toggle rumble on/off |
| **Plus + Minus (held)** | Quit to the Wii U Menu (or HBL, if launched from there) |

Menu navigation uses the left stick or D-pad, **A** to confirm, **B**/**Plus**
to back out.

The GamePad's motor rumbles automatically on your own weapon fire, own pain,
and hit feedback. Toggle it in-game with **left stick click + X**.

### Wii Remote + Nunchuk

Sync a Wii Remote through the Wii U's system Bluetooth pairing the same way
you'd pair one for a Wii U game with Wii Remote support, then attach a
Nunchuk. A USB-powered Wii Sensor Bar is required for pointer aim — without
one, the Wii Remote still works for buttons and the Nunchuk stick, it just
can't point.

| Input | Action |
|---|---|
| Nunchuk stick | Move (forward/back + strafe) |
| IR pointer (needs a Sensor Bar) | Look (yaw + pitch), plus an instant fine-aim offset on top |
| **B** (underside trigger) | Fire |
| **A** | Jump |
| **Z** | Crouch |
| **C** | Use item |
| **1** | Zoom |
| **2** | Walk |
| **D-pad** | Weapon prev/next |
| **Minus** | Scoreboard |
| **Plus** | Menu (Escape) |
| **Home + Minus (held)** | Quit to the Wii U Menu (or HBL, if launched from there) |

The IR pointer uses a dual-layer aim model ported from the vWii sibling
port: holding the pointer off-center turns the view continuously (like a
stick held toward the edge), while the pointer's exact position also applies
an instant fine-aim offset on top — so it both turns like a controller and
points like a light gun. Tune it with `wiimote_pointer_deadzone`,
`wiimote_pointer_sensitivity`, `wiimote_pointer_maxdelta`,
`wiimote_pointer_yawrange`, and `wiimote_pointer_pitchrange`.

## Credits

- **[ioQuake3](https://github.com/ioquake/ioq3)** — the upstream engine this port is based on.
- **[wut](https://github.com/devkitPro/wut)** / devkitPro — the Wii U homebrew toolchain (WHBGfx, GX2, coreinit headers) this port builds against.

---

## AI disclosure

Parts of this port were developed with the assistance of **Claude** (Anthropic). AI was used for code generation, debugging, porting guidance, and documentation. All AI-generated code was reviewed and tested on hardware before inclusion.

---

## License

ioQuake3 is GPLv2. This port layer is also GPLv2. See `LICENSE`.
