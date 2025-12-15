# libvgpu Makefile

CC = gcc
CFLAGS = -Wall -Wextra -O2 -g -fPIC -pthread -I../..
LDFLAGS = -shared -pthread

# Platform-specific linker flags
UNAME_S := $(shell uname -s)
ifeq ($(UNAME_S),Darwin)
    LDFLAGS += -Wl,-install_name,@rpath/libcuda.so.1
else
    LDFLAGS += -Wl,-soname,libcuda.so.1 -lrt
endif

# Source files
SOURCES = libvgpu.c ../../idm-protocol/transport.c
HEADERS = cuda.h ../../idm-protocol/idm.h

# Targets
TARGET = libcuda.so.1
TEST_APP = test_app

.PHONY: all clean test help

# Default target
all: $(TARGET) $(TEST_APP)

# Build shared library
$(TARGET): $(SOURCES) $(HEADERS)
	@echo "Building libvgpu as $(TARGET)..."
	$(CC) $(CFLAGS) $(SOURCES) -o $@ $(LDFLAGS)
	@echo "✓ Built: $@"
	@echo ""
	@echo "This library intercepts CUDA calls and forwards them via IDM"
	@echo ""
	@echo "Usage:"
	@echo "  1. Link your app: gcc app.c -L. -lcuda"
	@echo "  2. Run with: LD_LIBRARY_PATH=. ./app"
	@echo "  Or install: sudo cp $(TARGET) /usr/local/lib/ && sudo ldconfig"
	@echo ""

# Build test application
$(TEST_APP): test_app.c $(TARGET) $(HEADERS)
	@echo "Building test application..."
	$(CC) -Wall -Wextra -O2 -g -I. test_app.c -o $@ -L. -lcuda -Wl,-rpath,.
	@echo "✓ Built: $@"
	@echo ""
	@echo "Run with: ./$(TEST_APP)"
	@echo "(Make sure GPU proxy is running first!)"
	@echo ""

# Clean
clean:
	rm -f $(TARGET) $(TEST_APP)
	@echo "Cleaned"

# Help
help:
	@echo "libvgpu Build System"
	@echo ""
	@echo "Targets:"
	@echo "  make          - Build libcuda.so.1 (virtual GPU library)"
	@echo "  make clean    - Remove built files"
	@echo "  make help     - Show this help"
	@echo ""
	@echo "What is libvgpu?"
	@echo "  A drop-in replacement for NVIDIA's libcuda.so"
	@echo "  Intercepts CUDA Driver API calls"
	@echo "  Forwards requests to GPU proxy via IDM"
	@echo "  Enables GPU access without direct hardware access"
	@echo ""
