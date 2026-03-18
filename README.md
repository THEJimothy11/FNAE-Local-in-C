# Five Nights at Epstein — TI-84 CE Port

A port of [FNAE](https://github.com/WitheredFOT/FNAE-Local) to the TI-84 Plus CE calculator.

## Project status

| Module | Status |
|---|---|
| `game_state.c` | ⏳ Stub — write next |
| `game.c` | ✅ Complete |
| `enemy_ai.c` | ✅ Complete |
| `camera_system.c` | ⏳ Stub — write next |
| `ui_manager.c` | ✅ Complete |
| `input_handler.c` | ⏳ Stub — write next |
| `main.c` | ✅ Complete |
| Sprites / convimg | ⏳ Need image resizing |

## Building

### Prerequisites

1. Install the [CE C/C++ Toolchain](https://ce-programming.github.io/toolchain/)
2. Install [convimg](https://github.com/mateoconlechuga/convimg) for sprite conversion

### Steps

```bash
# 1. Resize all source images to fit 320x240 (see assets/convimg.yaml)
#    Use any image editor or ImageMagick:
mogrify -resize 320x240\> assets/images/*.png

# 2. Convert sprites
cd assets && convimg && cd ..

# 3. Build
make

# 4. Transfer to calculator
#    Copy bin/FNAE.8xp via TI-Connect CE or TiLP
```

## Controls (in-game)

| Action | Key |
|---|---|
| Look left | `[Left]` |
| Look right | `[Right]` |
| Open/close camera | `[2nd]` |
| Open control panel | `[Alpha]` |
| Navigate panel | `[Up]` / `[Down]` |
| Confirm / select | `[Enter]` |
| Close panel | `[Del]` |
| Quit to menu | `[Clear]` (from main menu only) |

## What changed from the web version

- **No audio** — TI-84 CE has no speaker
- **No video** — cutscenes replaced with text fade-ins
- **Images** — all downscaled to 320×240, 8bpp indexed colour
- **Mouse/touch** — replaced entirely with keypad
- **localStorage** — replaced with a calculator appvar stored in archive
- **setTimeout/setInterval** — replaced with hardware timer tick accumulators

## Repo structure

```
FNAE-CE/
├── src/
│   ├── main.c
│   ├── game.c / game.h
│   ├── game_state.c / game_state.h
│   ├── enemy_ai.c / enemy_ai.h
│   ├── camera_system.c / camera_system.h
│   ├── ui_manager.c / ui_manager.h
│   └── input_handler.c / input_handler.h
├── assets/
│   ├── images/          ← source PNGs (resize before convimg)
│   ├── sprites/         ← convimg output (generated, gitignored)
│   └── convimg.yaml
├── makefile
└── README.md
```

Add `assets/sprites/` to `.gitignore` — it's generated output.
