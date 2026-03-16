#
# Makefile for C64 Vi Editor
# Requires cc65 toolchain: https://cc65.github.io/
# c1541 from VICE: https://vice-emu.sourceforge.io/
#
# Usage:
#   make          - build vi.prg and vi_disk.d64
#   make clean    - remove build artifacts
#   make run      - launch in VICE emulator (needs x64sc)
#

CC65_HOME ?= <PATH TO cc65>
CL65      = $(CC65_HOME)/bin/cl65
TARGET    = c64
PROG      = vi
DISK      = $(PROG)_disk.d64

# -O: optimise  --static-locals: keep locals in BSS (saves stack on 6502)
CFLAGS = -t $(TARGET) -O --static-locals -I src

SRCS = src/main.c   \
       src/editor.c \
       src/display.c \
       src/fileio.c

.PHONY: all clean run

all: $(PROG).prg $(DISK)

$(PROG).prg: $(SRCS) src/editor.h
	$(CL65) $(CFLAGS) -o $@ $(SRCS)

$(DISK): $(PROG).prg
	rm -f $(DISK)
	c1541 -format "vi editor,vi" d64 $(DISK) \
	      -attach $(DISK) \
	      -write $(PROG).prg $(PROG)

run: $(DISK)
	x64sc -autostart $(DISK)

clean:
	rm -f $(PROG).prg $(PROG).map $(DISK)
