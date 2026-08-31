# ---------------------------------------------------------------
# Makefile — TamaPoke
#
# BOARD is the folder under boards/ and the PIO env of the same name:
#   make BOARD=amoled_175 ...   Waveshare ESP32-S3-Touch-AMOLED-1.75
#   make BOARD=lcd_185c ...     Waveshare ESP32-S3-Touch-LCD-1.85C V2
# Short aliases: BOARD=175 and BOARD=185c. Default here is lcd_185c.
#
# First time:
#   make deps          # .venv + pip install -r requirements.txt
#   make sprites       # fetch PMD sprites and pack tools/sdcard/mons/
#   make build         # compile firmware
#   make upload        # flash the board
#
# Sprites live on the board's microSD, not in flash:
#   Format the card FAT32 (not exFAT). Then either:
#     make sd-copy SD=/run/media/deck/TAMAPOKE   # USB SD adapter
#     make sd-send                               # over USB once firmware runs
#   Eject the adapter and put the card in the Waveshare slot.
#
# Daily:
#   make help
#   make upload && make monitor
#
# ---------------------------------------------------------------

PYTHON ?= python3
VENV   := .venv
PY     := $(VENV)/bin/python
PIO    ?= pio
PORT   ?= /dev/ttyACM0
BOARD  ?= lcd_185c
SD     ?=
MONS   := tools/sdcard/mons

# Folder name = PIO env. Only 175 / 185c are aliases; anything else is the folder.
ifeq ($(BOARD),175)
ENV := amoled_175
else ifeq ($(BOARD),185c)
ENV := lcd_185c
else
ENV := $(BOARD)
endif

.DEFAULT_GOAL := help

.PHONY: help deps build upload flash monitor test test-native \
        sprites thumbs fonts bundle web \
        sd-copy sd-send sd-ls \
        clean clean-sprites

help: ## Show this help
	@echo "Firmware:"
	@echo "  deps               Create .venv and pip install -r requirements.txt"
	@echo "  build              Compile with PlatformIO (BOARD=$(BOARD))"
	@echo "  upload             Flash the board on $(PORT)"
	@echo "  flash              build + upload"
	@echo "  monitor            Serial monitor (115200, Ctrl-C to quit)"
	@echo "  BOARD=<folder>     boards/ folder + PIO env (aliases: 175, 185c)"
	@echo "  test               Host checks (Python)"
	@echo "  test-native        Host C++ tests (g++, no board)"
	@echo ""
	@echo "Sprites (~40 MB, PMD SpriteCollab, needs network):"
	@echo "  sprites            Pack all 151 + shinies + thumbs"
	@echo "  thumbs             Rebuild thumbs.bin from already-packed mons"
	@echo "  fonts              Pack Unifont JA/ZH faces -> font_ja.bin font_zh.bin"
	@echo "  bundle             Pack web/sprites.pak for the browser installer"
	@echo ""
	@echo "microSD (FAT32, files go in /mons on the card):"
	@echo "  sd-copy SD=...     Copy packed bins onto a mounted card"
	@echo "  sd-send            Push sprites to the board over USB (needs running fw)"
	@echo "  sd-ls              List what the board already has on its SD"
	@echo ""
	@echo "Other:"
	@echo "  web                Rebuild firmware bin + sprites.pak (Arduino CLI path)"
	@echo "  clean              Wipe .pio build cache"
	@echo "  clean-sprites      Delete packed bins (not the download cache)"
	@echo ""
	@echo "Format a blank card FAT32 (pick the partition, not the whole disk):"
	@echo "  lsblk"
	@echo "  sudo mkfs.vfat -F 32 -n TAMAPOKE /dev/sdX1"
	@echo "  make sprites"
	@echo "  make sd-copy SD=/run/media/deck/TAMAPOKE"
	@echo ""
	@echo "Variables: PORT=$(PORT)  BOARD=$(BOARD)  SD=$(SD)  PYTHON=$(PYTHON)"

# ---- setup --------------------------------------------------------

$(VENV)/bin/python: requirements.txt
	$(PYTHON) -m venv $(VENV)
	$(VENV)/bin/pip install -r requirements.txt

deps: $(VENV)/bin/python ## Create .venv and install requirements.txt

# ---- firmware -----------------------------------------------------

build: ## Compile firmware
	$(PIO) run -e $(ENV)

test: ## Host checks \(no board\)
	$(PYTHON) test/check_invariants.py

test-native: ## Host C++ tests via g++ \(does not need libc headers\)
	mkdir -p .pio
	g++ -std=c++20 -Itest/support -Isrc -I. test/test_host/test_host.cpp -o .pio/test_host
	.pio/test_host

upload: ## Flash firmware to $(PORT)
	$(PIO) run -e $(ENV) -t upload --upload-port $(PORT)

# If upload dies with "Protocol error" on RTS: unplug, hold BOOT, plug in, retry.
upload-boot: ## Same as upload; print the BOOT-button hint first
	@echo Hold BOOT, plug USB in, then flashing $(PORT)...
	$(PIO) run -e $(ENV) -t upload --upload-port $(PORT)

flash: build upload ## Compile and flash

monitor: ## Serial monitor on $(PORT)
	$(PIO) device monitor --port $(PORT)

# ---- sprites ------------------------------------------------------

sprites: $(VENV)/bin/python ## Fetch + pack 151 + shinies, then thumbs
	$(PY) tools/pack_pmd.py
	$(PY) tools/make_thumbs.py
	@echo Packed $$(ls $(MONS)/*.bin 2>/dev/null | wc -l) files in $(MONS)

thumbs: $(VENV)/bin/python ## Rebuild thumbs.bin from packed TPK2 files
	$(PY) tools/make_thumbs.py

fonts: $(VENV)/bin/python ## Pack Unifont subsets for JA / ZH
	$(PY) tools/pack_font.py

bundle: $(VENV)/bin/python ## Bundle packed sprites into web/sprites.pak
	$(PY) tools/pack_bundle.py

web: ## Official Arduino-CLI web-installer rebuild
	bash tools/build_web.sh

# ---- microSD ------------------------------------------------------
# Firmware mounts FAT \(FAT16/FAT32\) via SD_MMC and looks for /mons/*.bin.
# exFAT will not mount. The board can format an empty card itself, but a
# USB adapter + FAT32 is faster and does not need a running firmware.

sd-copy: ## Copy packed sprites to a mounted FAT32 card \(SD=/run/media/deck/TAMAPOKE\)
	@test -n "$(SD)" || { echo "Set SD= to the mounted card, e.g. make sd-copy SD=/run/media/deck/TAMAPOKE"; exit 1; }
	@test -d "$(SD)" || { echo "Not a directory: $(SD)"; exit 1; }
	@test -n "$$(ls $(MONS)/*.bin 2>/dev/null)" || { echo "No packed sprites. Run: make sprites"; exit 1; }
	mkdir -p "$(SD)/mons"
	cp -v $(MONS)/*.bin "$(SD)/mons/"
	sync
	@echo Copied to $(SD)/mons — eject the adapter and put the card in the board.

sd-send: $(VENV)/bin/python ## Push sprites to the board SD over USB
	$(PY) tools/send_sd.py --port $(PORT)

sd-ls: $(VENV)/bin/python ## List files on the board SD
	$(PY) tools/send_sd.py --port $(PORT) --ls

# ---- clean --------------------------------------------------------

clean: ## Wipe PlatformIO build cache
	$(PIO) run -e $(ENV) -t clean

clean-sprites: ## Delete packed bins \(keeps tools/pmd_cache\)
	rm -f $(MONS)/*.bin
