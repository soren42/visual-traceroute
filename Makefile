include config.mk

BINDIR  = bin
TARGET  = $(BINDIR)/visual-traceroute
TESTBIN = $(BINDIR)/test_runner

# Core sources (always compiled)
CORE_SRC = \
	src/main.c \
	src/cli.c \
	src/log.c \
	src/core/host.c \
	src/core/edge.c \
	src/core/graph.c \
	src/core/scan.c \
	src/core/json_out.c \
	src/net/iface.c \
	src/net/dns.c \
	src/net/ping.c \
	src/net/icmp6.c \
	src/output/out_json.c \
	src/output/out_html.c \
	src/output/layout.c \
	src/util/alloc.c \
	src/util/strutil.c \
	vendor/cJSON/cJSON.c

# Platform-specific sources
ifeq ($(PLATFORM),darwin)
PLAT_SRC = \
	src/net/route_darwin.c \
	src/net/arp_darwin.c \
	src/net/icmp_darwin.c
ifeq ($(FOUND_DNS_SD),yes)
PLAT_SRC += src/net/mdns_darwin.c
endif
endif

ifeq ($(PLATFORM),linux)
PLAT_SRC = \
	src/net/route_linux.c \
	src/net/arp_linux.c \
	src/net/icmp_linux.c
ifeq ($(FOUND_AVAHI),yes)
PLAT_SRC += src/net/mdns_linux.c
endif
endif

# Optional output modules
ifeq ($(FOUND_NCURSES),yes)
OPT_SRC += src/output/out_curses.c
endif

ifeq ($(FOUND_CAIRO),yes)
OPT_SRC += src/output/out_png.c
endif

ifeq ($(FOUND_FFMPEG),yes)
ifeq ($(FOUND_CAIRO),yes)
OPT_SRC += src/output/out_mp4.c
endif
endif

ALL_SRC = $(CORE_SRC) $(PLAT_SRC) $(OPT_SRC)
ALL_OBJ = $(ALL_SRC:.c=.o)

# Test sources
TEST_SRC = \
	tests/test_main.c \
	tests/test_graph.c \
	tests/test_host.c \
	tests/test_layout.c \
	tests/test_json.c \
	tests/test_cli.c \
	tests/mock_net.c \
	src/cli.c \
	src/log.c \
	src/core/host.c \
	src/core/edge.c \
	src/core/graph.c \
	src/core/json_out.c \
	src/output/layout.c \
	src/util/alloc.c \
	src/util/strutil.c \
	vendor/cJSON/cJSON.c

TEST_OBJ = $(TEST_SRC:.c=.o)

INCLUDES = -I. -Isrc -Ivendor/cJSON

.PHONY: all clean test

all: $(TARGET)

$(TARGET): $(ALL_OBJ) | $(BINDIR)
	$(CC) $(LDFLAGS) -o $@ $(ALL_OBJ) $(LIBS)

$(TESTBIN): $(TEST_OBJ) | $(BINDIR)
	$(CC) $(LDFLAGS) -o $@ $(TEST_OBJ) $(LIBS)

%.o: %.c config.h
	$(CC) $(CFLAGS) $(INCLUDES) -c -o $@ $<

$(BINDIR):
	mkdir -p $(BINDIR)

test: $(TESTBIN)
	./$(TESTBIN)

clean:
	rm -f $(ALL_OBJ) $(TEST_OBJ) $(TARGET) $(TESTBIN)
	rm -f config.h config.mk
