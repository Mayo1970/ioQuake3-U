# Installation

This port produces **five separate Wii U homebrew apps** (`.wuhb`) from one
source tree. Each is a standalone Aroma tile with its own name and icon —
install only the ones you want. None of them include the copyrighted game
data; you provide your own `.pk3` files, bought legally, and copy them to
your SD card.

## What each build is

| Build | What it is | Notable features |
|---|---|---|
| **ioQuake3-U** | Quake III Arena | Full gameplay, bots, online/LAN multiplayer, mods via `fs_game` |
| **Team Arena-U** | Quake III: *Team Arena* | Adds TA's extra weapons/game modes on top of ioQuake3-U |
| **Open Arena-U** | Free/open Q3A-compatible game | Same engine features, uses free OA game data — no retail purchase needed |
| **Quake3Classic-U** | Q3A speaking the original Dreamcast-era protocol | Crossplay with Sega Dreamcast community servers; pak0–pak2 only, no mod support |
| **Elite Force-U** | Star Trek Voyager: Elite Force multiplayer | Retail EF holomatch gameplay |

## Where to get the required files

Buy the base game(s) legally, then copy the `.pk3` files from your
install/disc into the paths below.

- **Quake III Arena** (needed for ioQuake3-U, Team Arena-U, and Quake3Classic-U): [Here](https://www.gog.com/en/game/quake_iii_arena)
- **Open Arena** [Here](https://openarena.ws/)
- **Star Trek Voyager: Elite Force** (needed for Elite Force-U only): [Here](https://www.gog.com/en/game/star_trek_voyager_elite_force)
- **Dreamcast community map pack**: [Here](https://lvlworld.com/download/id:999)

---

## Common layout

Aroma scans `sd:/wiiu/apps/` recursively, so every build's `.wuhb` can live
anywhere under that tree — one folder per flavor, or all of them side by
side. All builds share one game-data root, `sd:/quake3/`, so reinstalling or
rebuilding a `.wuhb` never touches your paks or configs.

```
sd:/wiiu/apps/<anything>/ioquake3_*_wiiu.wuhb
sd:/quake3/<game dir(s) below>
```

Game data, logs, configs, and the `qkey` all live under `sd:/quake3/` —
completely separate from wherever the `.wuhb` itself is installed.

---

## ioQuake3-U (Q3A)

Copy your Quake III Arena `baseq3` paks:

```
sd:/quake3/baseq3/pak0.pk3 … pak8.pk3
```

## Team Arena-U

Needs **both** Q3A's `baseq3` and Team Arena's `missionpack`:

```
sd:/quake3/baseq3/pak0.pk3 … pak8.pk3
sd:/quake3/missionpack/pak0.pk3 … pak3.pk3
```

## Open Arena-U

Copy Open Arena's paks — no `baseq3` needed:

```
sd:/quake3/baseoa/pak0.pk3 …
```

## Quake3Classic-U (Dreamcast crossplay)

Only `pak0`–`pak2` are loaded (byte-identical to the Dreamcast data files);
higher paks are ignored:

```
sd:/quake3/baseq3/pak0.pk3
sd:/quake3/baseq3/pak1.pk3
sd:/quake3/baseq3/pak2.pk3
```

The community map pack (`zpack-classic.pk3`) is written to this same
`baseq3/` folder automatically on first launch — nothing to copy by hand.

## Elite Force-U

Copy your Elite Force retail `baseEF` paks:

```
sd:/quake3/baseEF/pak0.pk3 …
```

---

## Installing

1. Set up the [Aroma](https://aroma.foryour.cafe/) homebrew environment on
   your Wii U if you haven't already (Homebrew Launcher / entrypoint of your
   choice, Aroma installed as the environment).
2. Get each flavor's `.wuhb` — either grab a prebuilt one from the project's
   [Releases](https://github.com/Mayo1970/ioQuake3-U/releases) page, or build
   it yourself (see [README.md](README.md#building)).
3. Copy the `.wuhb` file(s) to a folder under `sd:/wiiu/apps/` on your SD
   card (e.g. `sd:/wiiu/apps/ioquake3-u/`).
4. Copy the matching game data from the sections above into `sd:/quake3/`.
5. Insert the SD card, boot into the Wii U Menu, and launch Aroma. Each
   installed flavor shows up as its own tile with its own name and icon.

## Troubleshooting

- **A build misbehaves after switching between builds** (wrong game name,
  missing intro video, stale settings, etc.) — delete that build's config
  file on the SD card:

  | Build | Config file |
  |---|---|
  | ioQuake3-U | `sd:/quake3/baseq3/q3config.cfg` |
  | Team Arena-U | `sd:/quake3/missionpack/q3config.cfg` |
  | Open Arena-U | `sd:/quake3/baseoa/oaconfig.cfg` |
  | Quake3Classic-U | `sd:/quake3/baseq3/q3config.cfg` |
  | Elite Force-U | `sd:/quake3/baseEF/efconfig.cfg` |

  Note that **ioQuake3-U and Quake3Classic-U share the same config file**
  (both run out of `baseq3/`) — a setting saved from one carries over to the
  other. That's expected, not a bug.
- **"pak0.pk3 not found" on boot** — the game data isn't where the flavor
  expects it; re-check the per-flavor paths above (Open Arena-U and Elite
  Force-U check for their own `pak0.pk3` before starting and print a clear
  error to the boot log if it's missing).
- **Nothing shows up in Aroma** — make sure the `.wuhb` is actually under
  `sd:/wiiu/apps/` (any subfolder is fine) and not just loose in the SD
  card's root.
