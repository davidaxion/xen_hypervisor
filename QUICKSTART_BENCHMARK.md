# Quick Start: Run GPU Benchmark Yourself

## Setup Complete! ✅

Your 400GB disk is now mounted at `/mnt/data` with all code ready to use.

## SSH into Instance

```bash
gcloud compute ssh gpu-benchmarking \
    --zone=us-central1-a \
    --project=robotic-tide-459208-h4
```

## Run Baseline Benchmark (30 seconds)

This tests raw GPU performance without any virtualization:

```bash
# Go to disk
cd /mnt/data

# Compile
gcc -o gpu_benchmark mlperf-benchmark/scripts/simple_gpu_benchmark.c \
    -I/usr/local/cuda/include \
    -L/usr/local/cuda/lib64 \
    -lcuda

# Run!
./gpu_benchmark
```

**Expected Results**:
```
=== GPU Benchmark ===

GPU: Tesla T4

=== Memory Bandwidth (100 MB) ===
Host to Device: ~4.5 GB/s

=== Throughput (1000 ops) ===
Throughput: ~7000 ops/sec
Avg latency: ~0.14 ms

=== Latency (500 samples) ===
p50: ~0.135 ms
p90: ~0.153 ms
p99: ~0.198 ms
```

## Test GPU Isolation System

Test with our CUDA interceptor to measure overhead:

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

### Terminal 2: Run Test with libvgpu

Open another SSH session:

```bash
gcloud compute ssh gpu-benchmarking --zone=us-central1-a --project=robotic-tide-459208-h4

cd /mnt/data/gpu-proxy/libvgpu
make clean && make
./test_app
```

## Compare Results

```bash
cd /mnt/data

# Run baseline
./gpu_benchmark | tee results_baseline.txt

# With libvgpu (while gpu_proxy is running)
cd gpu-proxy/libvgpu
./test_app | tee results_with_libvgpu.txt

# Compare throughput
echo "Baseline:"
grep "Throughput:" results_baseline.txt

echo ""
echo "With libvgpu:"
grep "operations completed" results_with_libvgpu.txt
```

## Full Test: End-to-End

```bash
# Terminal 1
cd /mnt/data/gpu-proxy
./gpu_proxy

# Terminal 2
cd /mnt/data/gpu-proxy/libvgpu
./test_app

# Output will show:
# - CUDA initialized ✓
# - Memory allocated ✓
# - Data copied ✓
# - All tests passed ✓
```

## Check Disk Space Anytime

```bash
df -h /mnt/data

# You have 393GB available!
```

## Save Results to Local Machine

```bash
# From your local machine
gcloud compute scp gpu-benchmarking:/mnt/data/results_*.txt \
    mlperf-benchmark/results/ \
    --zone=us-central1-a
```

---

## What Each Test Measures

**Baseline Benchmark**:
- Memory bandwidth (PCIe speed)
- Raw GPU allocation/free performance
- Latency percentiles
- **This is your performance target**

**With libvgpu**:
- Same tests but through our interceptor
- Measures software overhead
- Should be <5% slower than baseline
- Proves our isolation adds minimal cost

**End-to-End Test**:
- Full system: GPU proxy + IDM + libvgpu
- Real CUDA operations
- Memory transfers
- Context management

---

## Disk Info

- **Location**: `/mnt/data`
- **Size**: 400GB
- **Mount**: Permanent (survives reboots)
- **Temp files**: `/mnt/data/tmp`
- **Code**: `/mnt/data/gpu-proxy`, `/mnt/data/idm-protocol`

## Cost Savings

By using the larger disk:
- ✅ No more "disk full" errors
- ✅ Room for large datasets
- ✅ Space for Python/PyTorch if needed later
- ✅ Can run full MLPerf benchmarks

---

That's it! You now have everything set up to run benchmarks yourself. 🚀
