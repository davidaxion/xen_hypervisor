# Setting Up Additional Disk on GCP Instance

## Step 1: SSH into the Instance

```bash
gcloud compute ssh gpu-benchmarking \
    --zone=us-central1-a \
    --project=robotic-tide-459208-h4
```

## Step 2: Find the New Disk

```bash
# List all disks
lsblk
```

You should see something like:
```
NAME   MAJ:MIN RM  SIZE RO TYPE MOUNTPOINT
sda      8:0    0   10G  0 disk
└─sda1   8:1    0   10G  0 part /
sdb      8:16   0  400G  0 disk    ← This is your new disk
```

The new disk is likely `/dev/sdb` (or `/dev/sdc` if sdb is used).

## Step 3: Format and Mount the New Disk

```bash
# Check if disk is formatted
sudo file -s /dev/sdb

# If it says "data", it needs formatting
# Format it with ext4
sudo mkfs.ext4 -m 0 -E lazy_itable_init=0,lazy_journal_init=0,discard /dev/sdb

# Create mount point
sudo mkdir -p /mnt/data

# Mount the disk
sudo mount -o discard,defaults /dev/sdb /mnt/data

# Make it writable
sudo chmod 777 /mnt/data

# Verify
df -h /mnt/data
```

You should see:
```
Filesystem      Size  Used Avail Use% Mounted on
/dev/sdb        394G   28K  374G   1% /mnt/data
```

## Step 4: Make Mount Permanent (Survives Reboots)

```bash
# Get the disk UUID
sudo blkid /dev/sdb

# Example output:
# /dev/sdb: UUID="abc-123-def" TYPE="ext4"

# Add to fstab (use YOUR UUID from above)
echo "UUID=abc-123-def /mnt/data ext4 discard,defaults 0 2" | sudo tee -a /etc/fstab

# Test the fstab entry
sudo mount -a
```

## Step 5: Move Your Work to the New Disk

```bash
# Create workspace
mkdir -p /mnt/data/workspace
cd /mnt/data/workspace

# Copy your existing code
cp -r ~/gpu-isolation .

# Or start fresh and upload
# (from your local machine)
gcloud compute scp --recurse gpu-isolation/ gpu-benchmarking:/mnt/data/workspace/ \
    --zone=us-central1-a
```

## Step 6: Install Dependencies on New Disk

```bash
cd /mnt/data/workspace

# Create temp directory on new disk
export TMPDIR=/mnt/data/tmp
mkdir -p $TMPDIR

# Install pip (using new disk for temp)
curl https://bootstrap.pypa.io/get-pip.py | python3 - --user

# Add to PATH
export PATH=$HOME/.local/bin:$PATH
echo 'export PATH=$HOME/.local/bin:$PATH' >> ~/.bashrc
```

---

# Running the Benchmark Yourself

## Option 1: Simple CUDA Benchmark (Fast - 30 seconds)

This tests raw GPU performance with CUDA.

```bash
# 1. SSH into instance
gcloud compute ssh gpu-benchmarking --zone=us-central1-a --project=robotic-tide-459208-h4

# 2. Go to workspace
cd /mnt/data/workspace/gpu-isolation

# 3. Create the benchmark file
cat > simple_gpu_benchmark.c << 'EOF'
/*
 * Simple GPU Benchmark - measures bandwidth and latency
 */
#include <cuda.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>

#define cuCtxCreate cuCtxCreate_v2  // Fix for CUDA 13.x

#define CHECK_CUDA(call) do { \
    CUresult res = call; \
    if (res != CUDA_SUCCESS) { \
        const char *errStr; \
        cuGetErrorString(res, &errStr); \
        fprintf(stderr, "CUDA Error: %s\n", errStr); \
        exit(1); \
    } \
} while(0)

double get_time() {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec + ts.tv_nsec / 1e9;
}

int main() {
    printf("=== GPU Benchmark ===\n\n");

    // Initialize
    CHECK_CUDA(cuInit(0));

    CUdevice device;
    CHECK_CUDA(cuDeviceGet(&device, 0));

    char name[256];
    CHECK_CUDA(cuDeviceGetName(name, sizeof(name), device));
    printf("GPU: %s\n\n", name);

    CUcontext ctx;
    CHECK_CUDA(cuCtxCreate(&ctx, 0, device));

    // Test 1: Memory Bandwidth
    printf("=== Memory Bandwidth (100 MB) ===\n");
    size_t size = 100 * 1024 * 1024;
    char *host_data = malloc(size);
    CUdeviceptr dev_ptr;

    CHECK_CUDA(cuMemAlloc(&dev_ptr, size));

    double start = get_time();
    CHECK_CUDA(cuMemcpyHtoD(dev_ptr, host_data, size));
    CHECK_CUDA(cuCtxSynchronize());
    double bandwidth = (size / (get_time() - start)) / 1e9;
    printf("Host to Device: %.2f GB/s\n\n", bandwidth);

    CHECK_CUDA(cuMemFree(dev_ptr));

    // Test 2: Throughput
    printf("=== Throughput (1000 ops) ===\n");
    start = get_time();
    for (int i = 0; i < 1000; i++) {
        CHECK_CUDA(cuMemAlloc(&dev_ptr, 1024*1024));
        CHECK_CUDA(cuMemFree(dev_ptr));
    }
    double elapsed = get_time() - start;
    printf("Throughput: %.0f ops/sec\n", 1000.0/elapsed);
    printf("Avg latency: %.3f ms\n\n", elapsed*1000/1000);

    // Test 3: Latency percentiles
    printf("=== Latency (500 samples) ===\n");
    double *times = malloc(sizeof(double) * 500);
    for (int i = 0; i < 500; i++) {
        start = get_time();
        CHECK_CUDA(cuMemAlloc(&dev_ptr, 1024*1024));
        CHECK_CUDA(cuMemFree(dev_ptr));
        CHECK_CUDA(cuCtxSynchronize());
        times[i] = (get_time() - start) * 1000;
    }

    // Sort for percentiles
    for (int i = 0; i < 499; i++) {
        for (int j = i+1; j < 500; j++) {
            if (times[i] > times[j]) {
                double tmp = times[i];
                times[i] = times[j];
                times[j] = tmp;
            }
        }
    }

    printf("p50: %.3f ms\n", times[250]);
    printf("p90: %.3f ms\n", times[450]);
    printf("p99: %.3f ms\n\n", times[495]);

    printf("=== Benchmark Complete ===\n");

    CHECK_CUDA(cuCtxDestroy(ctx));
    free(host_data);
    free(times);
    return 0;
}
EOF

# 4. Compile
gcc -o gpu_benchmark simple_gpu_benchmark.c \
    -I/usr/local/cuda/include \
    -L/usr/local/cuda/lib64 \
    -lcuda

# 5. Run!
./gpu_benchmark

# 6. Save results
./gpu_benchmark | tee benchmark_results_baseline.txt
```

**Expected output**:
```
=== GPU Benchmark ===

GPU: Tesla T4

=== Memory Bandwidth (100 MB) ===
Host to Device: 4.31 GB/s

=== Throughput (1000 ops) ===
Throughput: 6977 ops/sec
Avg latency: 0.143 ms

=== Latency (500 samples) ===
p50: 0.135 ms
p90: 0.153 ms
p99: 0.198 ms

=== Benchmark Complete ===
```

---

## Option 2: Benchmark with Our GPU Isolation Layer

This tests performance **with** our libvgpu interceptor (to measure overhead).

```bash
# 1. Build GPU proxy
cd /mnt/data/workspace/gpu-isolation/gpu-proxy
make clean && make

# 2. Build libvgpu
cd libvgpu
make clean && make

# 3. Terminal 1: Start GPU proxy
cd /mnt/data/workspace/gpu-isolation/gpu-proxy
./gpu_proxy

# You should see:
# Found 1 CUDA device(s)
# Using device: Tesla T4
# Ready to process GPU requests...

# 4. Terminal 2: Run benchmark through libvgpu
# (Open new SSH session)
gcloud compute ssh gpu-benchmarking --zone=us-central1-a --project=robotic-tide-459208-h4

cd /mnt/data/workspace/gpu-isolation/gpu-proxy/libvgpu

# Compile benchmark that uses our interceptor
gcc -o benchmark_with_libvgpu ../../../simple_gpu_benchmark.c \
    -I/usr/local/cuda/include \
    -L. \
    -lcuda \
    -Wl,-rpath,.

# Run with our libvgpu
./benchmark_with_libvgpu | tee benchmark_results_with_libvgpu.txt

# 5. Compare results
echo "=== Comparison ==="
echo "Baseline:"
cat ~/gpu-isolation/benchmark_results_baseline.txt | grep "Throughput:"
echo ""
echo "With libvgpu:"
cat benchmark_results_with_libvgpu.txt | grep "Throughput:"
```

---

## Option 3: Full System Test (GPU Proxy + libvgpu + Test App)

```bash
# Terminal 1: GPU Proxy
cd /mnt/data/workspace/gpu-isolation/gpu-proxy
./gpu_proxy

# Terminal 2: Test application
cd /mnt/data/workspace/gpu-isolation/gpu-proxy/libvgpu
./test_app
```

---

## Troubleshooting

### Disk not showing up
```bash
# Check GCP console to see if disk is attached
gcloud compute instances describe gpu-benchmarking \
    --zone=us-central1-a \
    --format="value(disks.source)"

# Reattach if needed
gcloud compute instances attach-disk gpu-benchmarking \
    --disk=disk-20251215-123147 \
    --zone=us-central1-a
```

### Out of space errors
```bash
# Check space
df -h

# Clean old root disk
sudo rm -rf /var/cache/apt/* /tmp/* /var/tmp/*

# Use new disk for everything
cd /mnt/data/workspace
```

### CUDA errors
```bash
# Verify GPU
nvidia-smi

# Check CUDA
ls /usr/local/cuda/lib64/libcuda.so
```

---

## Quick Reference Commands

```bash
# Mount disk (if not auto-mounted)
sudo mount /dev/sdb /mnt/data

# Go to workspace
cd /mnt/data/workspace

# Run baseline benchmark
cd gpu-isolation
gcc -o gpu_benchmark simple_gpu_benchmark.c -I/usr/local/cuda/include -L/usr/local/cuda/lib64 -lcuda
./gpu_benchmark

# Run with GPU isolation
# Terminal 1:
cd gpu-proxy && ./gpu_proxy
# Terminal 2:
cd gpu-proxy/libvgpu && ./test_app
```

---

## Results to Save

After running benchmarks, copy results back to your local machine:

```bash
# From your local machine
gcloud compute scp gpu-benchmarking:/mnt/data/workspace/gpu-isolation/benchmark_*.txt \
    mlperf-benchmark/results/ \
    --zone=us-central1-a
```
