# Testing on GCP with T4 GPU

## Overview

We'll use a GCP instance with NVIDIA T4 GPU to test the GPU isolation system with real hardware.

## GCP Instance Specifications

### Recommended Configuration:

```
Instance Type: n1-standard-4
GPU: 1x NVIDIA Tesla T4
vCPUs: 4
Memory: 15 GB
Boot Disk: Ubuntu 22.04 LTS, 100 GB
Zone: us-central1-a (or any zone with T4 availability)
```

**Estimated Cost**: ~$0.45/hour (~$10-15 for full testing session)

## Phase 1: Basic Testing (No Xen) - 2 hours

**Goal**: Verify our code works with real CUDA on real GPU

### Step 1: Create GCP Instance

```bash
# Using gcloud CLI
gcloud compute instances create gpu-test-instance \
    --zone=us-central1-a \
    --machine-type=n1-standard-4 \
    --accelerator=type=nvidia-tesla-t4,count=1 \
    --image-family=ubuntu-2204-lts \
    --image-project=ubuntu-os-cloud \
    --boot-disk-size=100GB \
    --maintenance-policy=TERMINATE \
    --metadata=startup-script='#!/bin/bash
    apt-get update
    apt-get install -y build-essential git'
```

Or use GCP Console:
1. Go to Compute Engine → VM instances
2. Click "Create Instance"
3. Select n1-standard-4
4. Click "CPU platform and GPU" → Add GPU → NVIDIA T4
5. Select Ubuntu 22.04 LTS
6. Create

### Step 2: Install NVIDIA Driver + CUDA

SSH into instance:
```bash
gcloud compute ssh gpu-test-instance --zone=us-central1-a
```

Install drivers:
```bash
# Install NVIDIA driver
sudo apt-get update
sudo apt-get install -y nvidia-driver-535

# Reboot to load driver
sudo reboot

# SSH back in after reboot
gcloud compute ssh gpu-test-instance --zone=us-central1-a

# Verify GPU is detected
nvidia-smi
# Should show: Tesla T4 with driver version
```

Install CUDA toolkit:
```bash
# Install CUDA 12.x
wget https://developer.download.nvidia.com/compute/cuda/repos/ubuntu2204/x86_64/cuda-keyring_1.1-1_all.deb
sudo dpkg -i cuda-keyring_1.1-1_all.deb
sudo apt-get update
sudo apt-get install -y cuda-toolkit-12-3

# Add to PATH
echo 'export PATH=/usr/local/cuda/bin:$PATH' >> ~/.bashrc
echo 'export LD_LIBRARY_PATH=/usr/local/cuda/lib64:$LD_LIBRARY_PATH' >> ~/.bashrc
source ~/.bashrc

# Verify CUDA
nvcc --version
```

### Step 3: Clone and Build Project

```bash
# Install dependencies
sudo apt-get install -y git build-essential

# Clone project
git clone <your-repo-url>
cd gpu-proxy

# Build GPU proxy with real CUDA
cd gpu-proxy
make clean && make
# Should build successfully

# Build libvgpu
cd ../libvgpu
make clean && make
# Should build successfully

cd ..
```

### Step 4: Test with Real GPU

**Terminal 1**: Start GPU proxy
```bash
cd gpu-proxy
./gpu_proxy

# You should see:
# Initializing CUDA...
# Found 1 CUDA device(s)
# Using device: Tesla T4
# CUDA initialized successfully
# Ready to process GPU requests...
```

**Terminal 2**: Run test app (different SSH session)
```bash
gcloud compute ssh gpu-test-instance --zone=us-central1-a
cd gpu-proxy/libvgpu
./test_app

# Expected output:
# === CUDA Test Application ===
# 1. Initializing CUDA...
#    ✓ CUDA initialized
# 2. Driver version: 12.3 (or similar)
# 3. Found 1 CUDA device(s)
# 4. Using device 0: Virtual GPU 0 (via Xen)
# ...
# === All tests passed! ===
```

**Expected Results**:
- ✅ Real CUDA initialization works
- ✅ Real GPU memory allocation works
- ✅ Data transfers work (H2D, D2H)
- ✅ Performance is good (<100µs latency)

### Step 5: Performance Benchmarking

Create a more intensive test:
```bash
cd libvgpu
cat > bench.c << 'EOF'
#include "cuda.h"
#include <stdio.h>
#include <time.h>

#define ITERATIONS 10000

int main() {
    cuInit(0);

    CUdevice device;
    cuDeviceGet(&device, 0);

    CUcontext ctx;
    cuCtxCreate(&ctx, 0, device);

    struct timespec start, end;
    clock_gettime(CLOCK_MONOTONIC, &start);

    for (int i = 0; i < ITERATIONS; i++) {
        CUdeviceptr ptr;
        cuMemAlloc(&ptr, 1024 * 1024);  // 1MB
        cuMemFree(ptr);
    }

    clock_gettime(CLOCK_MONOTONIC, &end);

    double elapsed = (end.tv_sec - start.tv_sec) +
                     (end.tv_nsec - start.tv_nsec) / 1e9;
    double avg_us = (elapsed / ITERATIONS) * 1e6;

    printf("Iterations: %d\n", ITERATIONS);
    printf("Total time: %.3f seconds\n", elapsed);
    printf("Average latency: %.2f µs\n", avg_us);
    printf("Throughput: %.2f ops/sec\n", ITERATIONS / elapsed);

    return 0;
}
EOF

gcc bench.c -I. -L. -lcuda -Wl,-rpath,. -o bench
./bench
```

**Target Performance**:
- Latency: <50µs per operation
- Throughput: >20,000 ops/sec

---

## Phase 2: Testing with Xen - 4 hours

**Goal**: Test with real hardware isolation

### Prerequisites

Xen requires nested virtualization, which GCP supports on certain instance types.

### Step 1: Create Xen-Compatible Instance

```bash
gcloud compute instances create gpu-xen-test \
    --zone=us-central1-a \
    --machine-type=n1-standard-8 \
    --min-cpu-platform="Intel Haswell" \
    --accelerator=type=nvidia-tesla-t4,count=1 \
    --image-family=ubuntu-2204-lts \
    --image-project=ubuntu-os-cloud \
    --boot-disk-size=200GB \
    --maintenance-policy=TERMINATE \
    --enable-nested-virtualization
```

**Note**: Use n1-standard-8 (8 vCPUs) for better performance.

### Step 2: Install Xen Hypervisor

```bash
# SSH into instance
gcloud compute ssh gpu-xen-test --zone=us-central1-a

# Install Xen
sudo apt-get update
sudo apt-get install -y xen-hypervisor-4.16-amd64 xen-utils-4.16

# Configure GRUB to boot Xen
sudo update-grub
sudo reboot

# After reboot, verify Xen is running
sudo xl info
# Should show: Xen version 4.16.x
```

### Step 3: Install NVIDIA Driver in Dom0

```bash
# Install driver (same as before)
sudo apt-get install -y nvidia-driver-535
sudo reboot

# Verify GPU in Dom0
nvidia-smi
```

### Step 4: Configure GPU for PCI Passthrough

```bash
# Find GPU PCI address
lspci | grep NVIDIA
# Output: 00:04.0 3D controller: NVIDIA Corporation TU104GL [Tesla T4]

# Enable IOMMU
sudo vim /etc/default/grub
# Add to GRUB_CMDLINE_LINUX_DEFAULT:
# intel_iommu=on xen-pciback.hide=(00:04.0)

sudo update-grub
sudo reboot

# Verify IOMMU groups
sudo find /sys/kernel/iommu_groups/ -type l
```

### Step 5: Build Minimal Kernel for DomU

We'll build a minimal kernel that runs inside Xen domains (the pods).

```bash
# Install kernel build dependencies
sudo apt-get install -y build-essential libncurses-dev bison flex \
    libssl-dev libelf-dev bc

# Download kernel source
wget https://cdn.kernel.org/pub/linux/kernel/v6.x/linux-6.1.tar.xz
tar xf linux-6.1.tar.xz
cd linux-6.1

# Use minimal config
make tinyconfig

# Enable required features
make menuconfig
# Enable:
# - Xen support (Processor type → Paravirtualized guest support)
# - 9P filesystem (File systems → Network File Systems → Plan 9)
# - Overlay FS (File systems → Overlay filesystem support)

# Build kernel
make -j$(nproc)
# Produces: arch/x86/boot/bzImage

# Copy to /opt
sudo mkdir -p /opt/minimal-kernel
sudo cp arch/x86/boot/bzImage /opt/minimal-kernel/vmlinuz
```

### Step 6: Create Minimal Rootfs with libvgpu

```bash
# Create rootfs directory
sudo mkdir -p /opt/minimal-rootfs
cd /opt/minimal-rootfs

# Install minimal system
sudo debootstrap --variant=minbase jammy . http://archive.ubuntu.com/ubuntu/

# Chroot and configure
sudo chroot .

# Install essentials
apt-get update
apt-get install -y init systemd-sysv bash coreutils

# Copy libvgpu from our build
exit  # Exit chroot
sudo cp ~/gpu-proxy/libvgpu/libcuda.so.1 /opt/minimal-rootfs/usr/lib/x86_64-linux-gnu/
sudo ln -s libcuda.so.1 /opt/minimal-rootfs/usr/lib/x86_64-linux-gnu/libcuda.so

# Copy IDM transport (needed by libvgpu)
# Already linked into libcuda.so.1

# Set up init
sudo cat > /opt/minimal-rootfs/sbin/init << 'EOF'
#!/bin/bash
mount -t proc proc /proc
mount -t sysfs sysfs /sys
exec /bin/bash
EOF
sudo chmod +x /opt/minimal-rootfs/sbin/init
```

### Step 7: Start GPU Proxy in Dom0

```bash
# Build and start GPU proxy
cd ~/gpu-proxy/gpu-proxy
make clean && make
sudo ./gpu_proxy &

# Should show:
# Found 1 CUDA device(s)
# Using device: Tesla T4
# Ready to process GPU requests...
```

### Step 8: Create DomU Configuration

```bash
sudo cat > /opt/test-domu.cfg << 'EOF'
name = "gpu-test-domain"
kernel = "/opt/minimal-kernel/vmlinuz"
root = "/dev/xvda ro"
memory = 2048
vcpus = 2

# Use our minimal rootfs as root disk (9pfs for simplicity)
# In production, use a real disk image
disk = [
    'format=raw,vdev=xvda,access=ro,target=/opt/minimal-rootfs'
]

# Network
vif = ['bridge=xenbr0']

# Console
on_crash = 'destroy'
EOF
```

### Step 9: Launch DomU and Test

```bash
# Create domain
sudo xl create /opt/test-domu.cfg -c

# You're now in the DomU console
# Should boot to a bash prompt

# Inside DomU, test CUDA
cd /
cat > test.c << 'EOF'
#include <cuda.h>
#include <stdio.h>

int main() {
    CUresult res = cuInit(0);
    printf("cuInit: %d\n", res);

    int count;
    cuDeviceGetCount(&count);
    printf("Devices: %d\n", count);

    return 0;
}
EOF

# Compile (if gcc is available, otherwise pre-compile)
gcc test.c -lcuda -o test
./test

# Expected:
# [libvgpu] Initialized (virtual GPU via IDM)
# cuInit: 0
# Devices: 1
```

**This proves**:
- ✅ Xen domain isolation works
- ✅ IDM communication across domains works
- ✅ libvgpu inside DomU can talk to GPU proxy in Dom0
- ✅ Real GPU accessed via isolation boundary

---

## Phase 3: Kubernetes Integration - 6 hours

**Goal**: Deploy minimal Kubernetes and test RuntimeClass

### Step 1: Install Kubernetes on Dom0

```bash
# Install containerd, kubelet, kubeadm
sudo apt-get update
sudo apt-get install -y containerd kubelet kubeadm kubectl

# Initialize single-node cluster
sudo kubeadm init --pod-network-cidr=10.244.0.0/16

# Configure kubectl
mkdir -p $HOME/.kube
sudo cp /etc/kubernetes/admin.conf $HOME/.kube/config
sudo chown $(id -u):$(id -g) $HOME/.kube/config

# Install CNI (Flannel)
kubectl apply -f https://github.com/flannel-io/flannel/releases/latest/download/kube-flannel.yml

# Allow pods on control plane (single-node setup)
kubectl taint nodes --all node-role.kubernetes.io/control-plane-
```

### Step 2: Build and Install vgpu-runtime

(We'll create this in next step - for now, document the plan)

```bash
# Build CRI runtime
cd ~/gpu-proxy/cri-runtime
go build -o vgpu-runtime

# Install
sudo cp vgpu-runtime /usr/local/bin/
sudo mkdir -p /etc/vgpu-runtime

# Configure
sudo cat > /etc/vgpu-runtime/config.yaml << 'EOF'
kernel: /opt/minimal-kernel/vmlinuz
rootfs: /opt/minimal-rootfs
gpuProxy: localhost:50051
EOF

# Start as systemd service
sudo systemctl start vgpu-runtime
```

### Step 3: Configure RuntimeClass

```bash
kubectl apply -f - << 'EOF'
apiVersion: node.k8s.io/v1
kind: RuntimeClass
metadata:
  name: vgpu-isolated
handler: vgpu-runtime
EOF
```

### Step 4: Deploy Test Pod

```bash
kubectl apply -f - << 'EOF'
apiVersion: v1
kind: Pod
metadata:
  name: cuda-test
spec:
  runtimeClassName: vgpu-isolated
  containers:
  - name: cuda
    image: nvidia/cuda:12.0-runtime-ubuntu22.04
    command: ["nvidia-smi"]
  restartPolicy: Never
EOF

# Check logs
kubectl logs cuda-test
```

---

## Cost Optimization

### Option 1: Use Preemptible Instances
```bash
--preemptible  # Add to gcloud create command
# Saves ~80% cost but can be terminated
```

### Option 2: Use Spot Instances
```bash
--provisioning-model=SPOT
# Similar to preemptible
```

### Option 3: Stop When Not Using
```bash
# Stop instance (keeps disk, no compute charges)
gcloud compute instances stop gpu-test-instance --zone=us-central1-a

# Start when needed
gcloud compute instances start gpu-test-instance --zone=us-central1-a
```

### Option 4: Delete When Done
```bash
# Delete instance (frees everything)
gcloud compute instances delete gpu-test-instance --zone=us-central1-a
```

---

## Testing Checklist

### Phase 1: Basic Testing ✓
- [ ] Instance created with T4 GPU
- [ ] NVIDIA driver installed
- [ ] CUDA toolkit installed
- [ ] GPU proxy builds and runs
- [ ] libvgpu builds and runs
- [ ] Test app passes with real GPU
- [ ] Performance benchmarks completed

### Phase 2: Xen Testing ✓
- [ ] Nested virtualization enabled
- [ ] Xen hypervisor installed
- [ ] GPU visible in Dom0
- [ ] PCI passthrough configured
- [ ] Minimal kernel built
- [ ] Minimal rootfs created with libvgpu
- [ ] DomU boots successfully
- [ ] libvgpu in DomU communicates with Dom0
- [ ] Real GPU operations work across isolation

### Phase 3: Kubernetes Testing ✓
- [ ] Kubernetes installed on Dom0
- [ ] vgpu-runtime built and installed
- [ ] RuntimeClass configured
- [ ] Test pod scheduled
- [ ] CUDA application runs in isolated pod

---

## Expected Timeline

| Phase | Duration | Cost |
|-------|----------|------|
| Phase 1: Basic Testing | 2 hours | ~$1 |
| Phase 2: Xen Testing | 4 hours | ~$2 |
| Phase 3: Kubernetes | 6 hours | ~$3 |
| **Total** | **12 hours** | **~$6** |

*(Using preemptible instances)*

---

## Next Steps

1. **Create GCP instance** with T4 GPU
2. **Start with Phase 1** (basic testing)
3. **Iterate** based on results
4. **Document findings** for next phases

Ready to get started? I can help you:
- Set up the GCP instance
- Debug any issues during testing
- Optimize the configuration
- Build the CRI runtime when ready

Let me know when you want to begin!
