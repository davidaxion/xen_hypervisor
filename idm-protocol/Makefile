# IDM Protocol Makefile

CC = gcc
CFLAGS = -Wall -Wextra -O2 -g -pthread
LDFLAGS = -pthread -lrt

# Xen libraries (only needed for Xen mode)
XEN_CFLAGS = -DUSE_XEN $(shell pkg-config --cflags xencontrol xenstore xenevtchn 2>/dev/null)
XEN_LDFLAGS = $(shell pkg-config --libs xencontrol xenstore xenevtchn 2>/dev/null)

# Source files
SOURCES = transport.c test.c
HEADERS = idm.h

# Targets
TARGET_STUB = idm_test
TARGET_XEN = idm_test_xen

.PHONY: all clean stub xen test help

# Default: build stub version (no Xen required)
all: stub

# Stub version (POSIX shared memory, for local testing)
stub: $(TARGET_STUB)

$(TARGET_STUB): $(SOURCES) $(HEADERS)
	@echo "Building stub version (POSIX shared memory)..."
	$(CC) $(CFLAGS) -o $@ $(SOURCES) $(LDFLAGS)
	@echo "✓ Built: $@"
	@echo ""
	@echo "Usage:"
	@echo "  Terminal 1: ./$(TARGET_STUB) server"
	@echo "  Terminal 2: ./$(TARGET_STUB) client"
	@echo ""

# Xen version (Xen grant tables, for production)
xen: $(TARGET_XEN)

$(TARGET_XEN): $(SOURCES) $(HEADERS)
	@echo "Building Xen version (grant tables + event channels)..."
	@if ! pkg-config --exists xencontrol 2>/dev/null; then \
		echo "ERROR: Xen development libraries not found!"; \
		echo "Install with: sudo apt-get install libxen-dev"; \
		exit 1; \
	fi
	$(CC) $(CFLAGS) $(XEN_CFLAGS) -o $@ $(SOURCES) $(LDFLAGS) $(XEN_LDFLAGS)
	@echo "✓ Built: $@"

# Run test
test: stub
	@echo "Running IDM test..."
	@echo ""
	@echo "This will test the IDM protocol using POSIX shared memory."
	@echo "Open two terminals and run:"
	@echo ""
	@echo "  Terminal 1: ./$(TARGET_STUB) server"
	@echo "  Terminal 2: ./$(TARGET_STUB) client"
	@echo ""

# Clean
clean:
	rm -f $(TARGET_STUB) $(TARGET_XEN)
	@echo "Cleaned"

# Help
help:
	@echo "IDM Protocol Build System"
	@echo ""
	@echo "Targets:"
	@echo "  make          - Build stub version (default)"
	@echo "  make stub     - Build stub version (POSIX shared memory)"
	@echo "  make xen      - Build Xen version (requires libxen-dev)"
	@echo "  make test     - Show test instructions"
	@echo "  make clean    - Remove built files"
	@echo "  make help     - Show this help"
	@echo ""
	@echo "Files:"
	@echo "  idm.h         - Message format definitions"
	@echo "  transport.c   - Transport layer (Xen + stub)"
	@echo "  test.c        - Test program"
	@echo ""
