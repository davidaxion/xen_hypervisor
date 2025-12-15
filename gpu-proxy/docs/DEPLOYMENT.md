# Deployment Architecture

This document explains how all components fit together in a production Kubernetes cluster.

## Overview

Our system provides GPU isolation for Kubernetes using Xen hypervisor, similar to Edera's approach.

```
┌─────────────────────────────────────────────────────────────────┐
│                    Kubernetes Cluster                            │
│                                                                  │
│  ┌────────────────────────────────────────────────────────┐    │
│  │ Control Plane Node                                      │    │
│  │  - kube-apiserver                                       │    │
│  │  - kube-controller-manager                              │    │
│  │  - kube-scheduler                                       │    │
│  │  - etcd                                                 │    │
│  └────────────────────────────────────────────────────────┘    │
│                                                                  │
│  ┌────────────────────────────────────────────────────────┐    │
│  │ GPU Worker Node 1 (Physical Server with NVIDIA GPU)    │    │
│  │                                                         │    │
│  │  ┌──────────────────────────────────────────────────┐ │    │
│  │  │ Xen Hypervisor (Dom0 - Driver Domain)            │ │    │
│  │  │                                                   │ │    │
│  │  │  ┌────────────────────────────────────────────┐  │ │    │
│  │  │  │ kubelet (manages pods)                     │  │ │    │
│  │  │  │                                             │  │ │    │
│  │  │  │ Uses: vgpu-runtime (our custom CRI)        │  │ │    │
│  │  │  └────────────────────────────────────────────┘  │ │    │
│  │  │                                                   │ │    │
│  │  │  ┌────────────────────────────────────────────┐  │ │    │
│  │  │  │ GPU Proxy Daemon (persistent)              │  │ │    │
│  │  │  │  - Listens for IDM messages                │  │ │    │
│  │  │  │  - Has exclusive GPU access                │  │ │    │
│  │  │  │  - Calls real NVIDIA driver                │  │ │    │
│  │  │  └────────────────────────────────────────────┘  │ │    │
│  │  │                                                   │ │    │
│  │  │  GPU Hardware (PCI Passthrough)                  │ │    │
│  │  │  ↓                                                │ │    │
│  │  │  [NVIDIA GPU A100/H100]                          │ │    │
│  │  └──────────────────────────────────────────────────┘ │    │
│  │                                                         │    │
│  │  ┌──────────────────────────────────────────────────┐ │    │
│  │  │ User Domains (Xen VMs) - One per Pod             │ │    │
│  │  │                                                   │ │    │
│  │  │  Pod 1 (DomU)          Pod 2 (DomU)              │ │    │
│  │  │  ┌─────────────┐       ┌─────────────┐          │ │    │
│  │  │  │ Minimal     │       │ Minimal     │          │ │    │
│  │  │  │ Kernel      │       │ Kernel      │          │ │    │
│  │  │  │ (~50MB)     │       │ (~50MB)     │          │ │    │
│  │  │  │             │       │             │          │ │    │
│  │  │  │ Container   │       │ Container   │          │ │    │
│  │  │  │ Rootfs      │       │ Rootfs      │          │ │    │
│  │  │  │             │       │             │          │ │    │
│  │  │  │ libvgpu.so  │       │ libvgpu.so  │          │ │    │
│  │  │  │ (symlinked  │       │ (symlinked  │          │ │    │
│  │  │  │  as         │       │  as         │          │ │    │
│  │  │  │  libcuda)   │       │  libcuda)   │          │ │    │
│  │  │  │             │       │             │          │ │    │
│  │  │  │ TensorFlow  │       │ PyTorch     │          │ │    │
│  │  │  │ App         │       │ App         │          │ │    │
│  │  │  └─────────────┘       └─────────────┘          │ │    │
│  │  │        ↕                      ↕                  │ │    │
│  │  │      IDM Messages         IDM Messages           │ │    │
│  │  │        ↕                      ↕                  │ │    │
│  │  │  [Xen Grant Tables - Shared Memory]             │ │    │
│  │  └──────────────────────────────────────────────────┘ │    │
│  └────────────────────────────────────────────────────────┘    │
│                                                                  │
│  ┌────────────────────────────────────────────────────────┐    │
│  │ GPU Worker Node 2 (identical setup)                    │    │
│  └────────────────────────────────────────────────────────┘    │
└─────────────────────────────────────────────────────────────────┘
```

## Components Breakdown

### 1. Custom CRI Runtime (`vgpu-runtime`)

**Location**: Runs in Dom0 on each GPU worker node
**Purpose**: Intercepts pod creation requests from kubelet
**What it does**:
- Implements CRI gRPC interface
- When kubelet says "create pod", instead of calling containerd:
  1. Creates a Xen domain (VM) via `xl create`
  2. Boots our minimal kernel
  3. Mounts the container image rootfs
  4. Injects libvgpu.so
  5. Starts the container process

**Files to create**:
```
cri-runtime/
├── main.go              # CRI gRPC server
├── runtime_service.go   # RunPodSandbox, StopPodSandbox
├── image_service.go     # PullImage, ListImages
├── xen_manager.go       # xl create/destroy wrapper
└── config.yaml          # Runtime configuration
```

### 2. Minimal Kernel Image

**Purpose**: The OS that boots inside each pod's Xen domain
**Size**: ~50MB (vs 100MB+ for regular Linux)
**What's included**:
- Linux kernel (custom config)
- Minimal init system (systemd-lite or busybox)
- libvgpu.so pre-installed at `/usr/lib/x86_64-linux-gnu/libcuda.so.1`
- Symlink: `/usr/lib/x86_64-linux-gnu/libcuda.so` → `libcuda.so.1`
- IDM kernel module (for Xen grant tables)

**Build process**:
```bash
# Kernel config
make tinyconfig
# Enable: Xen, 9P, overlay filesystem
# Disable: Most drivers, sound, wireless, etc.

# Rootfs
debootstrap --variant=minbase focal rootfs/
# Install: libvgpu.so, init, bash, coreutils
# Remove: docs, man pages, apt
```

**Files to create**:
```
kernel-builder/
├── kernel-config         # Linux .config
├── build-kernel.sh       # Builds kernel
├── build-rootfs.sh       # Builds minimal rootfs
├── inject-libvgpu.sh     # Installs libvgpu.so
└── create-image.sh       # Packages everything
```

### 3. Node Image (Packer)

**Purpose**: Bootable disk image for GPU worker nodes
**What's installed**:
- Xen hypervisor
- Dom0 kernel (full Linux)
- GPU Proxy daemon (systemd service)
- NVIDIA driver (in Dom0 only!)
- kubelet
- vgpu-runtime (our CRI)
- Minimal kernel image (for pods)

**Files to create**:
```
packer/
├── gpu-worker-node.pkr.hcl  # Packer template
├── scripts/
│   ├── install-xen.sh
│   ├── install-nvidia.sh
│   ├── install-gpu-proxy.sh
│   ├── install-kubelet.sh
│   └── install-vgpu-runtime.sh
└── configs/
    ├── gpu-proxy.service
    └── vgpu-runtime.yaml
```

## Deployment Flow

### Step 1: Build Artifacts
```bash
# Build minimal kernel
cd kernel-builder
./build-all.sh
# Produces: minimal-kernel.tar.gz

# Build CRI runtime
cd cri-runtime
go build -o vgpu-runtime
# Produces: vgpu-runtime binary

# Build GPU proxy
cd gpu-proxy
make
# Produces: gpu_proxy binary

# Build node image
cd packer
packer build gpu-worker-node.pkr.hcl
# Produces: gpu-worker-node.qcow2
```

### Step 2: Deploy Cluster
```bash
# Create VMs or bare-metal servers from node image
terraform apply  # Or manual deployment

# Bootstrap Kubernetes
kubeadm init --control-plane-endpoint=...

# Join GPU worker nodes
kubeadm join --token=... --discovery-token-ca-cert-hash=...
```

### Step 3: Configure RuntimeClass
```yaml
# runtime-class.yaml
apiVersion: node.k8s.io/v1
kind: RuntimeClass
metadata:
  name: vgpu-isolated
handler: vgpu-runtime  # References our custom CRI
scheduling:
  nodeSelector:
    gpu-isolation: xen  # Only schedule on our nodes
```

### Step 4: Deploy GPU Workload
```yaml
# gpu-pod.yaml
apiVersion: v1
kind: Pod
metadata:
  name: tensorflow-training
spec:
  runtimeClassName: vgpu-isolated  # Use our runtime!
  containers:
  - name: tensorflow
    image: tensorflow/tensorflow:latest-gpu
    resources:
      limits:
        nvidia.com/gpu: 1  # Handled by vgpu-runtime
```

## How It Works (Step-by-Step)

### Pod Creation Flow:

1. **User runs**: `kubectl apply -f gpu-pod.yaml`

2. **kube-scheduler** sees `runtimeClassName: vgpu-isolated`
   - Schedules pod to node with label `gpu-isolation: xen`

3. **kubelet** on GPU worker node:
   - Detects RuntimeClass `vgpu-isolated`
   - Calls our `vgpu-runtime` via CRI gRPC

4. **vgpu-runtime** receives `RunPodSandbox` request:
   ```go
   // Pseudo-code
   func (r *Runtime) RunPodSandbox(req *RunPodSandboxRequest) {
       // Create Xen domain config
       xlConfig := GenerateXenConfig(req.PodConfig)
       // Config includes:
       // - Memory: 2GB
       // - vCPUs: 4
       // - Kernel: /opt/minimal-kernel/vmlinuz
       // - Rootfs: 9p mount of container image

       // Create domain
       domainID := exec("xl create", xlConfig)

       // Wait for domain to boot
       WaitForBoot(domainID)

       // Domain is now running with libvgpu pre-installed!
       return &RunPodSandboxResponse{PodSandboxId: domainID}
   }
   ```

5. **Minimal kernel boots** inside Xen domain:
   - Mounts container rootfs via 9p
   - Runs container's entrypoint (e.g., `python train.py`)

6. **Application starts**:
   ```python
   # train.py
   import tensorflow as tf
   # This calls CUDA under the hood
   # CUDA calls hit libvgpu.so (symlinked as libcuda.so)
   ```

7. **libvgpu.so intercepts** CUDA call:
   ```c
   CUresult cuMemAlloc(CUdeviceptr *ptr, size_t size) {
       // Build IDM message
       struct idm_gpu_alloc req = {.size = size};
       idm_send(IDM_GPU_ALLOC, &req);

       // Wait for response from GPU proxy
       uint64_t handle = idm_recv_response();
       *ptr = handle;  // Opaque handle!
       return CUDA_SUCCESS;
   }
   ```

8. **GPU Proxy** (running in Dom0) receives IDM message:
   - Calls real CUDA: `cuMemAlloc(&real_ptr, size)`
   - Creates handle: `handle = handle_table_insert(zone_id, real_ptr, size)`
   - Sends response back

9. **Application continues** using opaque handles
   - All GPU operations go through IDM → GPU Proxy
   - Application never sees real GPU memory addresses
   - **Isolation enforced by Xen + handle table**

### Security Guarantees:

1. **Memory Isolation**: Xen MMU ensures DomU can't access Dom0 memory
2. **Handle Isolation**: Handle table ensures Zone 2 can't use Zone 3's handles
3. **IOMMU Isolation**: GPU DMA can only target Dom0 memory (configured by Xen)
4. **No Direct GPU Access**: DomU has no GPU device, can't talk to hardware

## Next Steps for POC

Based on what we have, here's the priority order:

### Immediate (for local testing):
1. ✅ IDM protocol - DONE
2. ✅ GPU proxy - DONE
3. ✅ libvgpu - DONE

### Next (to prove concept works):
4. **Minimal kernel builder** - Build a tiny Linux kernel + rootfs with libvgpu
5. **Local Xen test** - Boot the minimal kernel in a Xen VM, run test app
   - This proves the full stack works without Kubernetes

### Then (Kubernetes integration):
6. **CRI runtime** - Implement vgpu-runtime
7. **RuntimeClass setup** - Configure Kubernetes to use it
8. **Single-node test** - Deploy k8s + Xen on one machine, run GPU pod

### Finally (production-ready):
9. **Node image builder** - Packer template for GPU workers
10. **3-node cluster** - Deploy full cluster
11. **POC validation** - Run all validation tests

## What Should We Build Next?

**Option A**: Minimal Kernel Builder
- Fastest path to proving the full stack
- Can test locally with Xen
- ~4-6 hours of work

**Option B**: CRI Runtime
- More impressive (Kubernetes integration)
- Requires understanding CRI gRPC protocol
- ~8-10 hours of work

**Option C**: Documentation & Demo
- Create comprehensive docs
- Record demo video
- Prepare for presentation

What would you like to focus on?
