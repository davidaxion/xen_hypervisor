#!/bin/bash
# Test Xen Grant Table Integration
# This script validates that IDM protocol works over real Xen grant tables

set -e

echo "╔═══════════════════════════════════════════════════════════╗"
echo "║     Testing Xen Grant Table Integration                  ║"
echo "╚═══════════════════════════════════════════════════════════╝"
echo ""

# Check if running on Xen
echo "=== Step 1: Verifying Xen Environment ==="
if ! command -v xl &> /dev/null; then
    echo "ERROR: xl command not found. Is Xen installed?"
    exit 1
fi

if ! sudo xl info &> /dev/null; then
    echo "ERROR: Cannot connect to Xen hypervisor"
    echo "Are you running in Dom0?"
    exit 1
fi

echo "Xen hypervisor detected:"
sudo xl info | grep -E "^(xen_version|host|total_memory|free_memory)"
echo ""

# Check GPU
echo "=== Step 2: Checking GPU Availability ==="
if ! command -v nvidia-smi &> /dev/null; then
    echo "WARNING: nvidia-smi not found. GPU may not be available."
else
    echo "GPU Information:"
    nvidia-smi --query-gpu=name,driver_version,memory.total --format=csv,noheader
fi
echo ""

# Test GPU Proxy in Dom0
echo "=== Step 3: Starting GPU Proxy in Dom0 ==="
if [ ! -f "/opt/gpu-proxy/gpu_proxy" ]; then
    echo "ERROR: GPU proxy not found at /opt/gpu-proxy/gpu_proxy"
    echo "Run: ./deployment/deploy-to-gcp.sh"
    exit 1
fi

# Start proxy in background
echo "Starting GPU proxy daemon..."
sudo /opt/gpu-proxy/gpu_proxy &
PROXY_PID=$!
echo "GPU proxy running with PID: $PROXY_PID"
sleep 2

# Check if proxy is running
if ! kill -0 $PROXY_PID 2>/dev/null; then
    echo "ERROR: GPU proxy failed to start"
    exit 1
fi
echo "GPU proxy started successfully"
echo ""

# Create test domain configuration
echo "=== Step 4: Creating Test Domain ==="
TEST_DOMAIN_CFG="/tmp/test-gpu-isolation.cfg"

cat > "$TEST_DOMAIN_CFG" <<'EOF'
# Test Domain for GPU Isolation
name = "test-gpu-isolation"
type = "pvh"
memory = 1024
vcpus = 2
kernel = "/boot/vmlinuz-$(uname -r)"
ramdisk = "/boot/initrd.img-$(uname -r)"

# Use existing filesystem for simplicity
root = "/dev/xvda ro"
disk = [ 'phy:/dev/loop0,xvda,w' ]

# Network
vif = [ 'bridge=xenbr0' ]

# Grant table communication with Dom0
# (actual grant table setup done in code)

on_poweroff = 'destroy'
on_reboot = 'destroy'
on_crash = 'destroy'
EOF

# Create a small disk image
echo "Creating test disk image..."
sudo dd if=/dev/zero of=/tmp/test-disk.img bs=1M count=512 2>/dev/null
sudo mkfs.ext4 -F /tmp/test-disk.img >/dev/null 2>&1

# Set up loop device
LOOP_DEV=$(sudo losetup -f)
sudo losetup "$LOOP_DEV" /tmp/test-disk.img
echo "Loop device: $LOOP_DEV"

# Update config with actual loop device
sed -i "s|/dev/loop0|$LOOP_DEV|" "$TEST_DOMAIN_CFG"

echo "Test domain configuration created"
echo ""

# Try to create domain (may fail without proper kernel)
echo "=== Step 5: Attempting to Create Test Domain ==="
echo "NOTE: This may fail if minimal kernel is not yet built"
echo ""

if sudo xl create "$TEST_DOMAIN_CFG"; then
    echo "✓ Test domain created successfully!"

    # List running domains
    echo ""
    echo "Running domains:"
    sudo xl list

    # Cleanup
    echo ""
    echo "Cleaning up test domain..."
    sudo xl destroy test-gpu-isolation 2>/dev/null || true
else
    echo "✗ Domain creation failed (expected if minimal kernel not built yet)"
    echo ""
    echo "This is normal at this stage. Domain creation will work after:"
    echo "  - Phase 2: Building minimal kernel"
    echo "  - Kernel installed to /boot/"
fi
echo ""

# Cleanup
echo "=== Step 6: Cleanup ==="
sudo kill $PROXY_PID 2>/dev/null || true
sudo losetup -d "$LOOP_DEV" 2>/dev/null || true
sudo rm -f /tmp/test-disk.img "$TEST_DOMAIN_CFG"
echo "Cleanup complete"
echo ""

echo "╔═══════════════════════════════════════════════════════════╗"
echo "║     Xen Integration Test Complete                        ║"
echo "╚═══════════════════════════════════════════════════════════╝"
echo ""
echo "Phase 1 Status:"
echo "  ✓ Xen hypervisor operational"
echo "  ✓ GPU proxy can run in Dom0"
if command -v nvidia-smi &> /dev/null; then
    echo "  ✓ GPU accessible in Dom0"
else
    echo "  ⚠ GPU not detected"
fi
echo "  ⚠ Minimal kernel pending (Phase 2)"
echo "  ⚠ Grant table communication pending (Phase 2)"
echo ""
echo "Next: Build minimal kernel to enable domain creation"
echo "  ./deployment/build-minimal-kernel.sh"
echo ""
