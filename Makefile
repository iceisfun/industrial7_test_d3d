# Each directory under tests/ builds to build/<dir>.exe (all .c in it + all common/*.c).
# The DLL under test is deliberately NOT linked; everything resolves via GetProcAddress.
# dxguid/uuid/ole32 are linked for IID/CLSID data and COM string helpers only.
CC      = x86_64-w64-mingw32-gcc
CFLAGS  ?= -O1 -Wall -Wno-unused-function -Wno-unused-variable -Wno-unused-but-set-variable -Wno-format
INCS    = -Icommon -Iassets
LIBS    = -static -lkernel32 -ladvapi32 -lntdll -ldxguid -luuid -lole32 -lgdi32 -luser32 -loleaut32
COMMON  := $(wildcard common/*.c)
HDRS    := $(wildcard common/*.h assets/*.h)
AREAS   := $(notdir $(wildcard tests/*))
EXES    := $(addprefix build/,$(addsuffix .exe,$(AREAS)))
all: $(EXES)
.SECONDEXPANSION:
build/%.exe: $$(wildcard tests/$$*/*.c) $$(wildcard tests/$$*/*.h) $(COMMON) $(HDRS)
	@mkdir -p build
	$(CC) $(CFLAGS) $(INCS) -o $@ $(wildcard tests/$*/*.c) $(COMMON) $(LIBS)
list-areas: ; @for a in $(AREAS); do echo $$a; done
clean: ; rm -rf build
# Validation under Wine in a headless sandbox (see tools/wine_run.sh). Windows is the authority.
wine-run: all
	@for e in $(EXES); do echo "== $$e"; tools/wine_run.sh $$e | grep -E '^(FAIL|SKIP|SUMMARY)|^  (FAIL|NOTE)'; done
.PHONY: all clean wine-run list-areas
