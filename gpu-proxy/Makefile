# GPU Proxy Makefile

CC = gcc
CFLAGS = -Wall -Wextra -O2 -g -pthread -I..

# Platform-specific linker flags
UNAME_S := $(shell uname -s)
ifeq ($(UNAME_S),Darwin)
    LDFLAGS = -pthread
else
    LDFLAGS = -pthread -lrt
endif

# CUDA paths (adjust if needed)
CUDA_PATH ?= /usr/local/cuda
CUDA_INC = -I$(CUDA_PATH)/include
CUDA_LIB = -L$(CUDA_PATH)/lib64 -lcuda

# Check if CUDA is available
CUDA_AVAILABLE := $(shell if [ -d "$(CUDA_PATH)" ]; then echo "yes"; else echo "no"; fi)

# Source files
SOURCES = main.c handlers.c handle_table.c ../idm-protocol/transport.c
HEADERS = handle_table.h ../idm-protocol/idm.h
TEST_SOURCES = test_client.c ../idm-protocol/transport.c

# Targets
TARGET = gpu_proxy
TEST_TARGET = test_client

.PHONY: all clean test help check-cuda

# Default target
all: check-cuda $(TARGET) $(TEST_TARGET)

# Check if CUDA is available
check-cuda:
	@if [ "$(CUDA_AVAILABLE)" = "no" ]; then \
		echo "ERROR: CUDA not found at $(CUDA_PATH)"; \
		echo ""; \
		echo "Please install CUDA or set CUDA_PATH:"; \
		echo "  make CUDA_PATH=/path/to/cuda"; \
		echo ""; \
		echo "Or for testing without GPU:"; \
		echo "  make stub"; \
		echo ""; \
		exit 1; \
	fi
	@echo "✓ CUDA found at $(CUDA_PATH)"

# Build with real CUDA
$(TARGET): $(SOURCES) $(HEADERS)
	@echo "Building GPU Proxy with CUDA support..."
	$(CC) $(CFLAGS) $(CUDA_INC) -o $@ $(SOURCES) $(LDFLAGS) $(CUDA_LIB)
	@echo "✓ Built: $@"

$(TEST_TARGET): $(TEST_SOURCES) $(HEADERS)
	@echo "Building test client with CUDA support..."
	$(CC) $(CFLAGS) $(CUDA_INC) -o $@ $(TEST_SOURCES) $(LDFLAGS) $(CUDA_LIB)
	@echo "✓ Built: $@"
	@echo ""
	@echo "Usage:"
	@echo "  Terminal 1: ./$(TARGET)"
	@echo "  Terminal 2: ./$(TEST_TARGET)"
	@echo ""

# Build stub version (no real CUDA)
stub: CFLAGS += -DSTUB_CUDA
stub: $(SOURCES) $(TEST_SOURCES) $(HEADERS)
	@echo "Building GPU Proxy in stub mode (no CUDA)..."
	$(CC) $(CFLAGS) -o $(TARGET)_stub $(SOURCES) $(LDFLAGS)
	@echo "✓ Built: $(TARGET)_stub"
	@echo "Building test client in stub mode..."
	$(CC) $(CFLAGS) -o $(TEST_TARGET)_stub $(TEST_SOURCES) $(LDFLAGS)
	@echo "✓ Built: $(TEST_TARGET)_stub"
	@echo ""
	@echo "Stub mode: All CUDA calls succeed but don't use real GPU"
	@echo "Usage:"
	@echo "  Terminal 1: ./$(TARGET)_stub"
	@echo "  Terminal 2: ./$(TEST_TARGET)_stub"
	@echo ""

# Clean
clean:
	rm -f $(TARGET) $(TARGET)_stub $(TEST_TARGET) $(TEST_TARGET)_stub
	@echo "Cleaned"

# Help
help:
	@echo "GPU Proxy Build System"
	@echo ""
	@echo "Targets:"
	@echo "  make          - Build with real CUDA (default)"
	@echo "  make stub     - Build stub version (no GPU needed)"
	@echo "  make clean    - Remove built files"
	@echo "  make help     - Show this help"
	@echo ""
	@echo "Requirements:"
	@echo "  - NVIDIA CUDA Toolkit"
	@echo "  - NVIDIA GPU (for real mode)"
	@echo ""
	@echo "Environment:"
	@echo "  CUDA_PATH=$(CUDA_PATH)"
	@echo ""
