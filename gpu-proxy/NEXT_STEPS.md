# Next Steps - GCP T4 Testing

## 1. Push to GitHub

```bash
# Create a new repository on GitHub (via web UI)
# Then push this code:

git remote add origin <your-repo-url>
git branch -M main
git push -u origin main
```

## 2. Create GCP T4 Instance

```bash
# Using gcloud CLI (install first if needed: https://cloud.google.com/sdk/docs/install)

gcloud compute instances create gpu-dev \
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

**Estimated cost**: ~$0.45/hour (~$10-15 for full testing session)

**Cost optimization**: Use `--preemptible` flag to save ~80% (but instance can be terminated)

## 3. SSH into Instance

```bash
gcloud compute ssh gpu-dev --zone=us-central1-a
```

## 4. Install NVIDIA Driver + CUDA

```bash
# Install NVIDIA driver
sudo apt-get update
sudo apt-get install -y nvidia-driver-535

# Reboot to load driver
sudo reboot

# SSH back in after reboot
gcloud compute ssh gpu-dev --zone=us-central1-a

# Verify GPU is detected
nvidia-smi
# Should show: Tesla T4 with driver version

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

## 5. Clone Repository and Build

```bash
# Clone your repo
git clone <your-repo-url>
cd gpu-proxy

# Build GPU proxy with real CUDA
cd gpu-proxy
make clean && make
cd ..

# Build libvgpu
cd libvgpu
make clean && make
cd ..
```

## 6. Test with Real GPU

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

**Terminal 2**: Run test app (new SSH session)
```bash
gcloud compute ssh gpu-dev --zone=us-central1-a
cd gpu-proxy/libvgpu
./test_app

# Expected output:
# === CUDA Test Application ===
# 1. Initializing CUDA...
#    ✓ CUDA initialized
# 2. Driver version: 12.3 (or similar)
# 3. Found 1 CUDA device(s)
# ...
# === All tests passed! ===
```

## 7. Performance Benchmarking

Create a benchmark to measure real GPU performance:

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

## 8. When Done Testing

**Stop instance** (keeps disk, no compute charges):
```bash
gcloud compute instances stop gpu-dev --zone=us-central1-a
```

**Start again later**:
```bash
gcloud compute instances start gpu-dev --zone=us-central1-a
```

**Delete instance** (frees everything):
```bash
gcloud compute instances delete gpu-dev --zone=us-central1-a
```

---

## What to Expect

### Phase 1 Success Criteria:
- ✅ GPU proxy builds with real CUDA
- ✅ libvgpu builds successfully
- ✅ Test app runs without errors
- ✅ Real GPU memory allocation works
- ✅ Performance is acceptable (<100µs latency)

### After Phase 1:
Read `docs/GCP_TESTING.md` for:
- **Phase 2**: Xen hypervisor integration (4 hours)
- **Phase 3**: Kubernetes integration (6 hours)

---

## Troubleshooting

### GPU not detected
```bash
nvidia-smi
# If error, driver not loaded properly
sudo reboot
```

### Build fails with "cuda.h not found"
```bash
# Check CUDA installation
ls /usr/local/cuda/include/cuda.h
# If missing, reinstall CUDA toolkit
```

### "library 'cuda' not found" when running test_app
```bash
cd libvgpu
ln -sf libcuda.so.1 libcuda.so
```

---

For more details, see:
- `QUICKSTART.md` - Local testing guide
- `STATUS.md` - Project status
- `docs/GCP_TESTING.md` - Complete GCP testing guide
- `.claude/project.md` - Claude Code project context
