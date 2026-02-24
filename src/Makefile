# QueueProbe — local build
# Run from the src/ directory.
# Requires the Solace C SDK extracted alongside the repo root:
#   Windows: ../solclient-7.33.1.1-win/
#   Linux:   ../solclient-7.33.1.1-lin/  (or set SOLACE_HOME)

CC  = gcc
SRC = solace_consumer_debug.c

# ── Windows (MSYS2 UCRT64) ───────────────────────────────────────────────────
SOLACE_WIN = ../solclient-7.33.1.1-win
OUT_WIN    = ../windows/QueueProbe.exe

windows: $(SRC)
	$(CC) $(SRC) \
	  -I$(SOLACE_WIN)/include \
	  -L$(SOLACE_WIN)/lib/Win64 \
	  -DSOLCLIENT_CONST_PROPERTIES \
	  -lsolclient -lws2_32 \
	  -static-libgcc \
	  -o $(OUT_WIN)

# ── Linux ────────────────────────────────────────────────────────────────────
# -Wl,-rpath,'$ORIGIN' makes the binary look for .so files in its own directory,
# so users can run it by just placing libsolclient.so* next to the binary.
SOLACE_LIN = $(if $(SOLACE_HOME),$(SOLACE_HOME),../solclient-7.33.1.1-lin)
OUT_LIN    = ../linux/QueueProbe

linux: $(SRC)
	mkdir -p ../linux
	$(CC) $(SRC) \
	  -I$(SOLACE_LIN)/include \
	  -L$(SOLACE_LIN)/lib \
	  -DSOLCLIENT_CONST_PROPERTIES \
	  -Wl,-rpath,'$$ORIGIN' \
	  -lsolclient \
	  -o $(OUT_LIN)
	@echo ""
	@echo "  Next: copy Solace .so files into ../linux/ so the bundle is self-contained:"
	@echo "    cp $(SOLACE_LIN)/lib/libsolclient.so* ../linux/"
	@echo ""

.DEFAULT_GOAL := windows
.PHONY: windows linux