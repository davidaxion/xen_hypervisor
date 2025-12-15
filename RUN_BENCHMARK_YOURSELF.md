# Run GPU Benchmark Yourself - Simple Guide

## Step 1: Push Code to GitHub (30 seconds)

```bash
# From your local machine, in this directory
./push-to-github.sh
```

This will push all 7 commits to your repository.

---

## Step 2: SSH into Your GPU Instance (10 seconds)

```bash
gcloud compute ssh gpu-benchmarking \
    --zone=us-central1-a \
    --project=robotic-tide-459208-h4
```

---

## Step 3: Run the Baseline Benchmark (30 seconds)

This tests your GPU's raw performance without any virtualization.

```bash
# Go to the disk with space
cd /mnt/data

# Compile the benchmark
gcc -o gpu_benchmark mlperf-benchmark/scripts/simple_gpu_benchmark.c \
    -I/usr/local/cuda/include \
    -L/usr/local/cuda/lib64 \
    -lcuda

# Run it!
./gpu_benchmark
```

### Expected Output:

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

**This is your baseline!** Any virtualization overhead should be measured against these numbers.

---

## Step 4: Test with GPU Isolation (2 minutes)

Now test the same benchmark but with our GPU isolation layer to measure overhead.

### Terminal 1: Start GPU Proxy

```bash
cd /mnt/data/gpu-proxy
make clean && make
./gpu_proxy
```

You should see:
```
Found 1 CUDA device(s)
Using device: Tesla T4
Ready to process GPU requests...
```

Keep this running!

### Terminal 2: Run Benchmark Through libvgpu

Open a **second SSH session**:

```bash
gcloud compute ssh gpu-benchmarking \
    --zone=us-central1-a \
    --project=robotic-tide-459208-h4

cd /mnt/data/gpu-proxy/libvgpu
make clean && make
./test_app
```

This runs GPU operations through our isolation layer.

---

## Step 5: Compare Results

```bash
cd /mnt/data

# Save baseline results
./gpu_benchmark | tee baseline.txt

# In another terminal, start gpu_proxy
cd /mnt/data/gpu-proxy && ./gpu_proxy

# Back in first terminal, test with isolation
cd /mnt/data/gpu-proxy/libvgpu
./test_app | tee with_isolation.txt

# Compare
echo "=== COMPARISON ==="
echo "Baseline:"
grep "Throughput:" baseline.txt
echo ""
echo "With Isolation:"
grep "operations" with_isolation.txt
```

---

## What Each Number Means

**Memory Bandwidth (GB/s)**
- How fast data moves between CPU and GPU
- Higher is better
- Tesla T4 baseline: ~4.5 GB/s

**Throughput (ops/sec)**
- How many GPU operations per second
- Higher is better
- Tesla T4 baseline: ~7,000 ops/sec

**Latency (milliseconds)**
- How long each operation takes
- Lower is better
- Tesla T4 baseline: ~0.14 ms (140 microseconds)
- p99 = 99% of operations complete within this time

**Overhead Targets:**
- ✅ Excellent: < 1% slower
- ✅ Good: < 2% slower
- ✅ Acceptable: < 5% slower
- ❌ Needs improvement: > 5% slower

---

## Quick Commands Reference

```bash
# SSH to instance
gcloud compute ssh gpu-benchmarking --zone=us-central1-a --project=robotic-tide-459208-h4

# Check GPU
nvidia-smi

# Check disk space
df -h /mnt/data

# Run baseline benchmark
cd /mnt/data && ./gpu_benchmark

# Start GPU proxy (Terminal 1)
cd /mnt/data/gpu-proxy && ./gpu_proxy

# Test isolation (Terminal 2)
cd /mnt/data/gpu-proxy/libvgpu && ./test_app
```

---

## Troubleshooting

### "command not found: gcloud"
Install Google Cloud SDK: https://cloud.google.com/sdk/docs/install

### "disk full" errors
The 400GB disk is mounted at `/mnt/data`. Make sure you're working there:
```bash
cd /mnt/data
df -h .  # Should show ~393GB available
```

### CUDA errors
Check GPU and CUDA are working:
```bash
nvidia-smi
ls /usr/local/cuda/lib64/libcuda.so
```

### Can't compile
Make sure CUDA toolkit is installed:
```bash
ls /usr/local/cuda/include/cuda.h
```

---

## Next Steps After Benchmarking

1. **Measure overhead**: Calculate performance difference
2. **Optimize if needed**: If overhead > 5%, investigate bottlenecks
3. **Add Xen hypervisor**: Next major phase
4. **Re-benchmark with Xen**: Measure full virtualization overhead

---

That's it! You're measuring real GPU isolation performance. 🚀
