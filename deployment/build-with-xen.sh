#!/bin/bash
# Build IDM Protocol and GPU Proxy with Xen Support
# This script compiles all components with USE_XEN flag enabled

set -e

PROJECT_ROOT="/Users/davidengstler/Projects/Hack_the_planet/GPU_Hypervisor_Xen"

echo "╔═══════════════════════════════════════════════════════════╗"
echo "║     Building GPU Proxy with Xen Grant Table Support      ║"
echo "╚═══════════════════════════════════════════════════════════╝"
echo ""

# Check for Xen development libraries
echo "=== Checking Xen development dependencies ==="
if ! dpkg -l | grep -q libxen-dev; then
    echo "ERROR: libxen-dev not installed"
    echo "Install with: sudo apt-get install libxen-dev libxenstore-dev"
    exit 1
fi

if ! dpkg -l | grep -q libxenstore3.0; then
    echo "ERROR: libxenstore3.0 not installed"
    echo "Install with: sudo apt-get install libxenstore3.0"
    exit 1
fi

echo "Xen libraries found"
echo ""

# Build IDM Protocol with Xen
echo "=== Step 1: Building IDM Protocol with Xen ==="
cd "$PROJECT_ROOT/idm-protocol"

echo "Cleaning previous builds..."
make clean 2>/dev/null || true

echo "Compiling with -DUSE_XEN flag..."
make CFLAGS="-DUSE_XEN -I/usr/include/xen" LDFLAGS="-lxenctrl -lxenstore"

if [ ! -f "test" ]; then
    echo "ERROR: IDM protocol build failed"
    exit 1
fi

echo "IDM protocol built successfully with Xen support"
echo ""

# Build GPU Proxy with Xen
echo "=== Step 2: Building GPU Proxy with Xen ==="
cd "$PROJECT_ROOT/gpu-proxy"

echo "Cleaning previous builds..."
make clean 2>/dev/null || true

echo "Building GPU proxy stub (no CUDA, for testing)..."
make stub CFLAGS="-DUSE_XEN -I/usr/include/xen" LDFLAGS="-lxenctrl -lxenstore"

if [ ! -f "gpu_proxy_stub" ]; then
    echo "ERROR: GPU proxy stub build failed"
    exit 1
fi

echo "GPU proxy stub built successfully"
echo ""

# Build libvgpu with Xen
echo "=== Step 3: Building libvgpu with Xen ==="
cd "$PROJECT_ROOT/gpu-proxy/libvgpu"

echo "Cleaning previous builds..."
make clean 2>/dev/null || true

echo "Building libvgpu..."
make CFLAGS="-DUSE_XEN -I/usr/include/xen" LDFLAGS="-lxenctrl -lxenstore"

if [ ! -f "libcuda.so.1" ]; then
    echo "ERROR: libvgpu build failed"
    exit 1
fi

echo "libvgpu built successfully"
echo ""

# Optionally build with real CUDA (if available)
echo "=== Step 4: Building GPU Proxy with CUDA (optional) ==="
cd "$PROJECT_ROOT/gpu-proxy"

if command -v nvcc &> /dev/null; then
    echo "CUDA compiler found, building real GPU proxy..."
    make CFLAGS="-DUSE_XEN -I/usr/include/xen" LDFLAGS="-lxenctrl -lxenstore -lcuda"

    if [ -f "gpu_proxy" ]; then
        echo "Real GPU proxy built successfully"
    else
        echo "WARNING: Real GPU proxy build failed, using stub version"
    fi
else
    echo "CUDA compiler not found, skipping real GPU proxy build"
    echo "Only stub version available"
fi
echo ""

echo "╔═══════════════════════════════════════════════════════════╗"
echo "║     Build Complete!                                       ║"
echo "╚═══════════════════════════════════════════════════════════╝"
echo ""
echo "Built artifacts:"
echo "  - IDM Protocol: $PROJECT_ROOT/idm-protocol/test"
echo "  - GPU Proxy Stub: $PROJECT_ROOT/gpu-proxy/gpu_proxy_stub"
echo "  - libvgpu: $PROJECT_ROOT/gpu-proxy/libvgpu/libcuda.so.1"
if [ -f "$PROJECT_ROOT/gpu-proxy/gpu_proxy" ]; then
    echo "  - GPU Proxy (Real): $PROJECT_ROOT/gpu-proxy/gpu_proxy"
fi
echo ""
echo "To deploy to GCP instance:"
echo "  ./deployment/deploy-to-gcp.sh"
echo ""
