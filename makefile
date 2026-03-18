# ============================================================
# Makefile — Five Nights at Epstein (TI-84 CE)
# CE C/C++ Toolchain: https://ce-programming.github.io/toolchain/
#
# Usage:
#   make          — build bin/FNAE.8xp
#   make run      — send to connected calculator via TIConnect CE
#   make clean    — remove build artefacts
# ============================================================

NAME    = FNAE
VERSION = 0.1.0

# Source files
SOURCES = src/main.c       \
          src/game.c       \
          src/game_state.c \
          src/enemy_ai.c   \
          src/camera_system.c \
          src/ui_manager.c \
          src/input_handler.c \
          assets/sprites/gfx.c   # convimg output

# Include paths
INCDIRS = src assets/sprites

# Extra compiler flags
CFLAGS  = -Wall -Wextra -Os

# CE toolchain boilerplate (do not change below this line)
include $(shell cedev-config --makefile)
