# Compiler and flags
CC := gcc
CFLAGS := -O2 -Wall -Wextra -I./include
LDFLAGS :=

# Directories
SRC_DIR := src
OBJ_DIR := obj
BIN_DIR := bin
INCLUDE_DIR := include

# Output binary
TARGET := $(BIN_DIR)/tcp_record

# Source files (autodiscovery from src/ directory)
SOURCES := $(wildcard $(SRC_DIR)/*.c)
OBJECTS := $(patsubst $(SRC_DIR)/%.c, $(OBJ_DIR)/%.o, $(SOURCES))
DEPS := $(OBJECTS:.o=.d)

# Default target
.PHONY: all
all: $(TARGET)

# Create target binary
$(TARGET): $(OBJECTS) | $(BIN_DIR)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)
	@echo "Build successful: $@"

# Compile source files to object files
$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c | $(OBJ_DIR)
	$(CC) $(CFLAGS) -MMD -MP -c $< -o $@

# Create necessary directories
$(OBJ_DIR):
	mkdir -p $(OBJ_DIR)

$(BIN_DIR):
	mkdir -p $(BIN_DIR)

# Include dependency files
-include $(DEPS)

# Clean build artifacts
.PHONY: clean
clean:
	rm -rf $(OBJ_DIR) $(BIN_DIR)
	@echo "Clean complete"

# Clean everything including target
.PHONY: distclean
distclean: clean
	@echo "Distclean complete"

# Install binary (optional)
.PHONY: install
install: $(TARGET)
	install -D $(TARGET) /usr/local/bin/tcp_record
	@echo "Installed to /usr/local/bin/tcp_record"

# Uninstall binary
.PHONY: uninstall
uninstall:
	rm -f /usr/local/bin/tcp_record
	@echo "Uninstalled"

# Show build information
.PHONY: info
info:
	@echo "TCP_RECORD Build Configuration"
	@echo "=============================="
	@echo "CC: $(CC)"
	@echo "CFLAGS: $(CFLAGS)"
	@echo "LDFLAGS: $(LDFLAGS)"
	@echo ""
	@echo "Directories:"
	@echo "  SRC_DIR:     $(SRC_DIR)"
	@echo "  OBJ_DIR:     $(OBJ_DIR)"
	@echo "  BIN_DIR:     $(BIN_DIR)"
	@echo "  INCLUDE_DIR: $(INCLUDE_DIR)"
	@echo ""
	@echo "Sources found:"
	@for f in $(SOURCES); do echo "  $$f"; done
	@echo ""
	@echo "Objects generated:"
	@for o in $(OBJECTS); do echo "  $$o"; done
	@echo ""
	@echo "Target: $(TARGET)"

# Help target
.PHONY: help
help:
	@echo "TCP_RECORD Makefile Targets"
	@echo "==========================="
	@echo "  all        - Build the tcp_record binary (default)"
	@echo "  clean      - Remove build artifacts (obj/, bin/)"
	@echo "  distclean  - Remove all generated files"
	@echo "  install    - Install binary to /usr/local/bin/"
	@echo "  uninstall  - Remove installed binary"
	@echo "  info       - Show build configuration"
	@echo "  help       - Display this help message"
