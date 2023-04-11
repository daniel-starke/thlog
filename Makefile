PREFIX = 
CC = $(PREFIX)gcc

UNICODE = 1
#DEBUG = 1

SRC = $(wildcard src/*.c) $(wildcard src/utility/*.c)

COMMON_CFLAGS = -Wall -Wextra -Wshadow -Wformat -Wconversion -mstackrealign -fno-ident
ifeq (1, $(DEBUG))
 COMMON_CFLAGS += -Og -g3 -ggdb -gdwarf-3 -fno-omit-frame-pointer
else
 COMMON_CFLAGS += -flto -O2 -DNDEBUG
endif
PATHS = -Isrc
SYS := $(shell $(CC) -dumpmachine)
ifneq (, $(findstring linux, $(SYS)))
 COMMON_CFLAGS += -D_BSD_SOURCE -D_POSIX_C_SOURCE=200112L -D_XOPEN_SOURCE -D_XOPEN_SOURCE_EXTENDED -D_LARGEFILE64_SOURCE
 CFLAGS = -std=c99 $(COMMON_CFLAGS)
 LDFLAGS = -fno-ident
 LIBS = -lm
 OBJEXT = .o
 BINEXT = 
else
 ifneq (, $(findstring mingw, $(SYS))$(findstring windows, $(SYS)))
  WINDRES = $(PREFIX)windres
  CFLAGS = -std=c99 $(COMMON_CFLAGS)
  LDFLAGS = -static -fno-ident
  ifeq (1, $(UNICODE))
   CFLAGS += -municode
   LDFLAGS += -municode
  endif
  LIBS = -lwinmm -lm
  OBJEXT = .o
  BINEXT = .exe
  ifeq (, $(findstring __MINGW64__, $(shell $(CC) -dM -E - </dev/null 2>/dev/null)))
   # patch to handle missing symbols in mingw32 correctly
   CFLAGS += -D__MINGW64__=1
  endif
 else
  COMMON_CFLAGS += -D_BSD_SOURCE -D_POSIX_C_SOURCE=200112L -D_XOPEN_SOURCE -D_XOPEN_SOURCE_EXTENDED -D_LARGEFILE64_SOURCE
  CFLAGS = -std=c99 $(COMMON_CFLAGS)
  LDFLAGS = -fno-ident
  LIBS = -lm
  OBJEXT = .o
  BINEXT = 
 endif
endif
ifneq (1, $(DEBUG))
 LDFLAGS += -s
endif

OBJ = $(patsubst src%,bin%,$(patsubst %.c,%$(OBJEXT),$(SRC)))

all: bin bin/utility bin/thlog$(BINEXT)

.PHONY: clean
clean:
	rm -rf bin/*

bin:
	mkdir bin

bin/utility:
	mkdir bin/utility

src/thlog.c: src/license.i
src/license.i: doc/COPYING script/convert-license.sh
	script/convert-license.sh doc/COPYING $@

bin/%.o: src/%.c
	$(CC) $(CFLAGS) -c -o $@ $<

bin/utility/%.o: src/utility/%.c
	$(CC) $(CFLAGS) -c -o $@ $<

.PHONY: bin/thlog$(BINEXT)
bin/thlog$(BINEXT): $(OBJ)
ifeq (,$(strip $(WINDRES)))
	rm -f $@
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $+ $(LIBS)
else
	rm -f $@
	$(WINDRES) src/version.rc bin/version$(OBJEXT)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $+ bin/version$(OBJEXT) $(LIBS)
endif
