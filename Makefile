# =========================================================
# Byte-Bistro — portable Makefile (macOS/Linux)
# - Builds GBN and SR variants separately to avoid duplicate symbols
# - C11, pthreads, warnings enabled
# - Adds SSE4.2 on x86_64 when available
# - Object cache in build/
# =========================================================

# ---- Toolchain & flags ------------------------------------------------------
CC       ?= gcc
CFLAGS   ?= -O2 -g -Wall -Wextra -pthread
CFLAGS   += -std=c11
LDFLAGS  ?=
INCLUDES := -Iinclude

# Enable SSE4.2 on x86_64 (for CRC32C intrinsics) when possible
UNAME_M := $(shell uname -m)
ifeq ($(UNAME_M),x86_64)
  CFLAGS += -msse4.2
endif

# ---- Layout ----------------------------------------------------------------
SRC_DIR := src
OBJ_DIR := build

# Common sources (shared by all binaries)
SRC_COMMON := \
  $(SRC_DIR)/bb_common.c \
  $(SRC_DIR)/bb_log.c \
  $(SRC_DIR)/bb_timer.c \
  $(SRC_DIR)/bb_wire.c \
  $(SRC_DIR)/bb_checksum.c \
  $(SRC_DIR)/bb_channel.c \
  $(SRC_DIR)/bb_proto.c \
  $(SRC_DIR)/bb_app.c

# Transport-specific sources
SRC_GBN := $(SRC_DIR)/bb_gbn.c
SRC_SR  := $(SRC_DIR)/bb_sr.c

# Shim sources (provide the *other* factory symbol so linkers are happy)
SRC_SHIM_SR  := $(SRC_DIR)/shim_null_sr.c    # used in GBN builds
SRC_SHIM_GBN := $(SRC_DIR)/shim_null_gbn.c   # used in SR  builds

# Binaries (we build per-transport to avoid symbol collisions)
BIN_SERVER_GBN := byte-bistro-server-gbn
BIN_CLIENT_GBN := byte-bistro-client-gbn
BIN_SERVER_SR  := byte-bistro-server-sr
BIN_CLIENT_SR  := byte-bistro-client-sr

# Convenience aliases (default to GBN)
BIN_SERVER := byte-bistro-server
BIN_CLIENT := byte-bistro-client

# Tests
TEST_CHECKSUM := test_checksum
TEST_CHANNEL  := test_channel

# ---------------------------------------------------------------------------
.PHONY: all clean dirs tests run-demo

all: dirs $(BIN_SERVER_GBN) $(BIN_CLIENT_GBN) $(BIN_SERVER_SR) $(BIN_CLIENT_SR) $(BIN_SERVER) $(BIN_CLIENT) tests

dirs:
	@mkdir -p $(OBJ_DIR)

# ---------- Pattern rule: compile any .c into build/%.o ---------------------
$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

# ---------- Object lists ----------------------------------------------------
OBJS_COMMON := $(patsubst $(SRC_DIR)/%.c,$(OBJ_DIR)/%.o,$(SRC_COMMON))
OBJS_GBN    := $(patsubst $(SRC_DIR)/%.c,$(OBJ_DIR)/%.o,$(SRC_GBN))
OBJS_SR     := $(patsubst $(SRC_DIR)/%.c,$(OBJ_DIR)/%.o,$(SRC_SR))

# Shim objects (built by the same pattern rule)
OBJS_SHIM_SR  := $(patsubst $(SRC_DIR)/%.c,$(OBJ_DIR)/%.o,$(SRC_SHIM_SR))
OBJS_SHIM_GBN := $(patsubst $(SRC_DIR)/%.c,$(OBJ_DIR)/%.o,$(SRC_SHIM_GBN))

OBJ_MAIN_SERVER := $(OBJ_DIR)/main_server.o
OBJ_MAIN_CLIENT := $(OBJ_DIR)/main_client.o

# ---------- Compile mains ---------------------------------------------------
$(OBJ_MAIN_SERVER): $(SRC_DIR)/main_server.c
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

$(OBJ_MAIN_CLIENT): $(SRC_DIR)/main_client.c
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

# ---------- Link per-transport binaries ------------------------------------
# GBN binaries: link GBN engine and the SR *shim* (not the SR engine itself)
$(BIN_SERVER_GBN): $(OBJS_COMMON) $(OBJS_GBN) $(OBJ_MAIN_SERVER) $(OBJS_SHIM_SR)
	$(CC) $(CFLAGS) $(INCLUDES) $^ $(LDFLAGS) -o $@

$(BIN_CLIENT_GBN): $(OBJS_COMMON) $(OBJS_GBN) $(OBJ_MAIN_CLIENT) $(OBJS_SHIM_SR)
	$(CC) $(CFLAGS) $(INCLUDES) $^ $(LDFLAGS) -o $@

# SR binaries: link SR engine and the GBN *shim* (not the GBN engine itself)
$(BIN_SERVER_SR): $(OBJS_COMMON) $(OBJS_SR) $(OBJ_MAIN_SERVER) $(OBJS_SHIM_GBN)
	$(CC) $(CFLAGS) $(INCLUDES) $^ $(LDFLAGS) -o $@

$(BIN_CLIENT_SR): $(OBJS_COMMON) $(OBJS_SR) $(OBJ_MAIN_CLIENT) $(OBJS_SHIM_GBN)
	$(CC) $(CFLAGS) $(INCLUDES) $^ $(LDFLAGS) -o $@

# ---------- Convenience aliases (point to GBN by default) -------------------
$(BIN_SERVER): $(BIN_SERVER_GBN)
	@rm -f $@
	@ln -s $(BIN_SERVER_GBN) $@

$(BIN_CLIENT): $(BIN_CLIENT_GBN)
	@rm -f $@
	@ln -s $(BIN_CLIENT_GBN) $@

# ---------- Tests -----------------------------------------------------------
tests: $(TEST_CHECKSUM) $(TEST_CHANNEL)

$(TEST_CHECKSUM): tests/test_checksum.c $(OBJ_DIR)/bb_checksum.o $(OBJ_DIR)/bb_common.o
	$(CC) $(CFLAGS) $(INCLUDES) -o $@ tests/test_checksum.c $(OBJ_DIR)/bb_checksum.o $(OBJ_DIR)/bb_common.o $(LDFLAGS)

$(TEST_CHANNEL): tests/test_channel.c $(OBJ_DIR)/bb_channel.o $(OBJ_DIR)/bb_common.o $(OBJ_DIR)/bb_log.o $(OBJ_DIR)/bb_timer.o $(OBJ_DIR)/bb_wire.o $(OBJ_DIR)/bb_checksum.o
	$(CC) $(CFLAGS) $(INCLUDES) -o $@ tests/test_channel.c $(OBJ_DIR)/bb_channel.o $(OBJ_DIR)/bb_common.o $(OBJ_DIR)/bb_log.o $(OBJ_DIR)/bb_timer.o $(OBJ_DIR)/bb_wire.o $(OBJ_DIR)/bb_checksum.o $(LDFLAGS)

# ---------- Demo helper (optional) -----------------------------------------
run-demo: all
	@echo "== Starting server (GBN) on :7777 =="
	@./$(BIN_SERVER) --port 7777 --proto gbn -v & echo $$! > .srvpid; sleep 0.3
	@echo "== Running client (4x10) =="
	@./$(BIN_CLIENT) --addr 127.0.0.1:7777 --proto gbn -c 4 -n 10 -v || true
	@kill $$(cat .srvpid) 2>/dev/null || true; rm -f .srvpid

# ---------- Clean -----------------------------------------------------------
clean:
	@echo "Cleaning..."
	@rm -rf $(OBJ_DIR) $(BIN_SERVER_GBN) $(BIN_CLIENT_GBN) $(BIN_SERVER_SR) $(BIN_CLIENT_SR) $(BIN_SERVER) $(BIN_CLIENT) $(TEST_CHECKSUM) $(TEST_CHANNEL)
