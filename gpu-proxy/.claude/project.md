# GPU Isolation for Kubernetes - Claude Project Context

## Project Overview
Building an open-source GPU isolation system for Kubernetes using Xen hypervisor, similar to Edera's commercial product. Enables multi-tenant GPU sharing with hardware-enforced isolation.

## Current Status
**70% Complete** - Core components working, Kubernetes integration pending

### Completed ✅
- IDM Protocol (inter-domain messaging)
- GPU Proxy Daemon (CUDA broker)
- libvgpu (CUDA interceptor)
- Full end-to-end testing in stub mode
- Comprehensive documentation

### In Progress 🚧
- GCP T4 instance testing with real GPU
- Xen hypervisor integration
- Minimal kernel builder

### Pending ⏳
- CRI runtime (Kubernetes integration)
- Node image builder (Packer)
- 3-node cluster deployment
- Production validation

## Key Files to Know

### Core Implementation
- `idm-protocol/idm.h` - Protocol definitions
- `idm-protocol/transport.c` - Messaging layer (Xen + POSIX stub)
- `gpu-proxy/main.c` - GPU proxy daemon
- `gpu-proxy/handlers.c` - CUDA call handlers
- `gpu-proxy/handle_table.c` - Security enforcement
- `libvgpu/libvgpu.c` - CUDA API interceptor (690 lines)
- `libvgpu/cuda.h` - CUDA definitions

### Documentation
- `README.md` - Project overview
- `STATUS.md` - Detailed progress (read this first!)
- `QUICKSTART.md` - 5-minute demo guide
- `docs/ARCHITECTURE.md` - Technical deep dive
- `docs/DEPLOYMENT.md` - Kubernetes integration plan
- `docs/GCP_TESTING.md` - T4 GPU testing guide

### Build System
- `gpu-proxy/Makefile` - Builds proxy daemon
- `libvgpu/Makefile` - Builds libcuda.so.1
- `idm-protocol/Makefile` - Builds test suite

## Architecture

```
CUDA Application
    ↓ cuMemAlloc()
libvgpu.so (intercepts)
    ↓ IDM_GPU_ALLOC message
[Xen Grant Tables / POSIX SHM]
    ↓
GPU Proxy Daemon
    ↓ Real cuMemAlloc()
NVIDIA Driver
    ↓
GPU Hardware (PCI Passthrough)
```

**Security**: Opaque handles, zone ownership, Xen MMU/IOMMU isolation

## Testing on GCP

### Quick Start
```bash
# Create T4 instance
gcloud compute instances create gpu-dev \
    --zone=us-central1-a \
    --machine-type=n1-standard-4 \
    --accelerator=type=nvidia-tesla-t4,count=1 \
    --image-family=ubuntu-2204-lts \
    --image-project=ubuntu-os-cloud \
    --boot-disk-size=100GB \
    --maintenance-policy=TERMINATE

# SSH into it
gcloud compute ssh gpu-dev --zone=us-central1-a

# Clone repo
git clone <repo-url>
cd gpu-proxy
```

See `docs/GCP_TESTING.md` for full guide.

## Common Commands

### Build Everything
```bash
# GPU proxy
cd gpu-proxy && make stub && cd ..

# libvgpu
cd libvgpu && make && cd ..
```

### Run Demo
```bash
# Terminal 1
cd gpu-proxy && ./gpu_proxy_stub

# Terminal 2
cd libvgpu && ./test_app
```

### With Real CUDA
```bash
# After installing NVIDIA driver + CUDA toolkit
cd gpu-proxy && make clean && make
cd ../libvgpu && make clean && make
```

## Development Guidelines

### Code Style
- C99 standard
- 4-space indentation (no tabs)
- Clear comments for complex logic
- Error handling on all system calls
- Use `#ifdef STUB_CUDA` for stub vs real builds

### Testing
- **No mocks!** All tests use real code paths
- Stub mode simulates GPU with malloc()
- Real mode requires NVIDIA GPU + CUDA

### Security
- Never expose real GPU pointers to user domains
- All handles are opaque (uint64_t)
- Zone ownership checked on every operation
- Validate buffer sizes before GPU access

## Next Development Steps

1. **Test on GCP T4** (Phase 1 - Basic Testing)
   - Install NVIDIA driver + CUDA
   - Build with real CUDA support
   - Run performance benchmarks
   - **Goal**: Prove real GPU works

2. **Xen Integration** (Phase 2)
   - Enable nested virtualization
   - Install Xen hypervisor
   - Configure PCI passthrough
   - Build minimal kernel
   - Test cross-domain IDM
   - **Goal**: Prove isolation works

3. **Kubernetes** (Phase 3)
   - Implement CRI runtime in Go
   - Configure RuntimeClass
   - Deploy test pod
   - **Goal**: End-to-end K8s integration

## Key Decisions Made

### Why Xen?
- Type-1 hypervisor (runs on bare metal)
- Hardware-enforced isolation (MMU + IOMMU)
- Grant tables for efficient shared memory
- Proven in production (AWS, Edera)

### Why IDM Protocol?
- Simple message format (header + payload)
- Works with Xen grant tables or POSIX SHM
- Sequence numbers for request/response matching
- Extensible (easy to add new GPU operations)

### Why Opaque Handles?
- Security: apps never see real GPU pointers
- Isolation: can't guess/forge other zone's handles
- Flexibility: handles can map to anything (ptr, offset, ID)

### Why Stub Mode?
- Enables testing without GPU hardware
- Validates protocol and logic
- Fast iteration during development
- Same code paths as real mode

## Performance Targets

| Metric | Target | Measured (Stub) | Measured (Real) |
|--------|--------|-----------------|-----------------|
| Alloc/Free Latency | <50µs | ~2.9ms | TBD |
| Throughput | >20K ops/sec | 340 ops/sec | TBD |
| Data Transfer (1MB) | <1% overhead | N/A | TBD |
| Overall Overhead | <5% | N/A | TBD |

*Stub mode is slower due to malloc() vs GPU, and POSIX SHM vs Xen grant tables*

## Troubleshooting

### "library 'cuda' not found"
```bash
cd libvgpu && ln -sf libcuda.so.1 libcuda.so
```

### "Failed to initialize IDM"
Start GPU proxy first (it creates shared memory)

### "Cannot allocate memory" on macOS
Ring buffer too large - already fixed (reduced to 32 entries)

### GPU proxy not receiving messages
Check zone IDs match (DRIVER_ZONE_ID=1, USER_ZONE_ID=2)

### Build fails with "undefined reference"
Missing `-lrt` on Linux or `-pthread` flag

## Resources

### Xen Documentation
- [Xen Project Wiki](https://wiki.xenproject.org/)
- [Grant Tables](https://wiki.xenproject.org/wiki/Grant_Table)
- [PCI Passthrough](https://wiki.xenproject.org/wiki/Xen_PCI_Passthrough)

### CUDA Driver API
- [CUDA Driver API Reference](https://docs.nvidia.com/cuda/cuda-driver-api/)

### Kubernetes CRI
- [CRI Spec](https://github.com/kubernetes/cri-api)
- [CRI-O Source](https://github.com/cri-o/cri-o) (reference implementation)

### Edera
- [Edera Website](https://edera.dev/)
- [Launch Article](https://www.theregister.com/2024/03/13/edera_gpu_isolation/)

## Questions to Answer

- **Performance**: What's the real overhead with actual GPU?
- **Isolation**: Can we prove two pods can't access each other's memory?
- **Scalability**: How many pods per GPU?
- **Stability**: Does it work with TensorFlow/PyTorch?

## Git Workflow

```bash
# Initialize repo
git init
git add .
git commit -m "Initial commit: GPU isolation POC (70% complete)"

# Push to GitHub
git remote add origin <repo-url>
git push -u origin main

# On GCP instance
git clone <repo-url>
cd gpu-proxy
# Continue development...
```

---

*This file helps Claude Code understand the project context when working on this codebase.*
