# Quick Start Guide

Get the GPU isolation system running in 5 minutes.

## Prerequisites

- Linux or macOS
- GCC compiler
- Make
- (Optional) NVIDIA GPU + CUDA toolkit for real testing

## Build Everything

```bash
# Clone/navigate to project
cd gpu-proxy

# Build IDM protocol test
cd idm-protocol
make
cd ..

# Build GPU proxy daemon
cd gpu-proxy
make stub  # Use 'make' if you have CUDA
cd ..

# Build libvgpu
cd libvgpu
make
cd ..
```

## Run the Demo

### Terminal 1: Start GPU Proxy Daemon
```bash
cd gpu-proxy
./gpu_proxy_stub

# You should see:
# IDM: Initializing stub mode (POSIX shared memory)
# === GPU Proxy Daemon ===
# Ready to process GPU requests...
```

### Terminal 2: Run CUDA Test Application
```bash
cd gpu-proxy/libvgpu
./test_app

# You should see:
# === CUDA Test Application ===
# 1. Initializing CUDA...
#    ✓ CUDA initialized
# 2. Driver version: 12.0
# 3. Found 1 CUDA device(s)
# 4. Using device 0: Virtual GPU 0 (via Xen)
# ...
# === All tests passed! ===
```

## What Just Happened?

```
test_app (CUDA application)
    ↓ Calls cuMemAlloc()
libvgpu.so (intercepts CUDA)
    ↓ Sends IDM_GPU_ALLOC message
[POSIX Shared Memory]
    ↓
gpu_proxy_stub (processes request)
    ↓ Calls real cuMemAlloc() [stubbed]
    ↓ Returns opaque handle
libvgpu.so (receives handle)
    ↓
test_app (gets CUdeviceptr)
```

**Security**: The application never sees the real GPU pointer, only an opaque handle!

## Try It Yourself

Write your own CUDA application:

```c
// my_app.c
#include "cuda.h"
#include <stdio.h>

int main() {
    cuInit(0);

    CUdevice device;
    cuDeviceGet(&device, 0);

    char name[256];
    cuDeviceGetName(name, sizeof(name), device);
    printf("Device: %s\n", name);

    return 0;
}
```

Compile and run:
```bash
cd libvgpu
gcc my_app.c -I. -L. -lcuda -Wl,-rpath,. -o my_app
./my_app
```

Output:
```
Device: Virtual GPU 0 (via Xen)
```

## Next Steps

- Read [ARCHITECTURE.md](docs/ARCHITECTURE.md) for technical details
- Read [DEPLOYMENT.md](docs/DEPLOYMENT.md) for Kubernetes integration
- Read [STATUS.md](STATUS.md) for project roadmap
- Check [README.md](README.md) for overview

## Troubleshooting

### "library 'cuda' not found"
```bash
cd libvgpu
ln -sf libcuda.so.1 libcuda.so
```

### "Failed to initialize IDM"
Make sure GPU proxy is running first:
```bash
# Terminal 1
cd gpu-proxy
./gpu_proxy_stub
```

### "Cannot allocate memory" (macOS)
The ring buffer is too large for macOS shared memory limits. This is already fixed in the current code (ring size reduced to 32 entries).

### Want to test with real CUDA?
```bash
# 1. Install CUDA toolkit
# 2. Build with real CUDA
cd gpu-proxy
make clean && make

cd libvgpu
make clean && make

# 3. Run on a machine with NVIDIA GPU
./test_app
```

## Performance Notes

Current stub mode results:
- **Average latency**: ~2.9ms per alloc+free operation
- **Throughput**: 340 ops/sec

Expected with real GPU:
- **Average latency**: <100µs per operation
- **Throughput**: >10,000 ops/sec

The difference is due to:
1. Stub mode uses `malloc()` instead of GPU memory
2. No actual GPU hardware involved
3. POSIX shared memory instead of Xen grant tables

With real Xen + GPU, performance should be much better!

## What's Working

✅ IDM messaging protocol
✅ GPU proxy daemon
✅ libvgpu CUDA interceptor
✅ End-to-end data flow
✅ Security (opaque handles)
✅ Error handling

## What's Not Yet Done

⏳ Real Xen integration
⏳ Minimal kernel for pods
⏳ CRI runtime (Kubernetes)
⏳ Multi-tenant testing
⏳ Performance benchmarks with real GPU

---

**Questions?** Check [STATUS.md](STATUS.md) for detailed project status.
