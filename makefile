NAME   = FNAE
SRCDIR = src
GFXDIR = assets/sprites

EXTRA_C_SOURCES = \
    assets/sprites/global_palette.c \
    assets/sprites/Cam1.c  \
    assets/sprites/Cam2.c  \
    assets/sprites/Cam3.c  \
    assets/sprites/Cam4.c  \
    assets/sprites/Cam5.c  \
    assets/sprites/Cam6.c  \
    assets/sprites/Cam7.c  \
    assets/sprites/Cam8.c  \
    assets/sprites/Cam9.c  \
    assets/sprites/Cam10.c \
    assets/sprites/Cam11.c \
    assets/sprites/map_layout.c \
    assets/sprites/Warningheavy.c \
    assets/sprites/Warninglight.c \
    assets/sprites/cutscene.c \
    assets/sprites/enemyep1.c \
    assets/sprites/enemyep4.c \
    assets/sprites/ep1.c   \
    assets/sprites/ep4.c   \
    assets/sprites/exp2.c  \
    assets/sprites/fa3.c   \
    assets/sprites/front.c \
    assets/sprites/goldenstephen.c \
    assets/sprites/jump.c  \
    assets/sprites/jumptrump.c \
    assets/sprites/menubackground.c \
    assets/sprites/mrstephen.c \
    assets/sprites/original.c \
    assets/sprites/scaryep.c \
    assets/sprites/scaryhawk.c \
    assets/sprites/scaryhawking.c \
    assets/sprites/scarytrump.c \
    assets/sprites/star.c  \
    assets/sprites/trump.c \
    assets/sprites/trump2.c \
    assets/sprites/trump3.c \
    assets/sprites/trump4.c \
    assets/sprites/trump5.c \
    assets/sprites/winscreen.c

CFLAGS = -Wall -Wextra -Os -Iassets/sprites -Isrc

include $(shell cedev-config --makefile)