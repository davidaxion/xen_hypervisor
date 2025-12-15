# MLPerf Benchmark Explained - Simple Guide

## What is MLPerf?

MLPerf is the **industry standard** for measuring ML performance. Think of it like "speedtest.net" but for AI/ML workloads.

- Used by NVIDIA, Intel, Google, AMD to compare hardware
- Tests **real ML workloads** (not synthetic benchmarks)
- Reproducible and comparable results

## How It Works

### 1. What It Tests

MLPerf runs actual AI models:
- **ResNet50**: Image classification (like "is this a cat or dog?")
- **BERT**: Natural language processing (like ChatGPT text understanding)
- **RetinaNet**: Object detection (finds objects in images)

### 2. What It Measures

Three main metrics:

**Throughput (samples/second)**
```
How many images can your GPU classify per second?
Example: 3,500 images/sec on Tesla T4

Higher = Better
```

**Latency (milliseconds)**
```
How long does it take to process ONE image?
Example:
- p50: 5ms (median - half are faster, half slower)
- p90: 8ms (90% of images processed within this time)
- p99: 15ms (99% processed within this time)

Lower = Better
```

**Accuracy**
```
Does the model still work correctly?
Must maintain >75% accuracy for ResNet50

Should stay = 100% (same as baseline)
```

## What The Output Looks Like

When you run MLPerf, you'll see output like this:

```
================================================
MLPerf Results Summary
================================================
Benchmark: resnet50
Scenario: Offline
Mode: PerformanceOnly
Samples per second: 3456.78
================================================
                   PERF
Min Duration:       60000 ms
Max Duration:       0 ms
Min Queries:        24576
Max Queries:        0
================================================
Early Stopping Result:
 * Processed at least 64 queries (24576).
 * Would discard 0 highest latency queries.
 * Early stopping 90th percentile estimate: 8.23 ms
 * Early stopping 99th percentile estimate: 14.56 ms

================================================
Performance test took 60.032 seconds
================================================
```

### What This Means:

- **3,456 samples/sec** = Your GPU processed 3,456 images per second
- **p90: 8.23ms** = 90% of images were classified in under 8.23 milliseconds
- **p99: 14.56ms** = 99% of images were classified in under 14.56 milliseconds

## Running Without Kubernetes (Bare Metal)

```bash
# Simple way - Direct on GPU
./run-mlperf-benchmark.sh
```

This will:
1. Install MLPerf on `/mnt/data` (400GB disk)
2. Download ResNet50 model
3. Run benchmark for 60 seconds
4. Save results to `mlperf-benchmark/results/`

### Expected Results (Tesla T4):
```
Throughput: 3,000-4,000 samples/sec
p90 Latency: 5-10 ms
p99 Latency: 10-20 ms
```

## Running With Kubernetes

### Why Kubernetes?

Because your Xen hypervisor will run **as a Kubernetes DaemonSet**. So you need to test:
1. Bare metal (baseline)
2. In Kubernetes pod (measure K8s overhead)
3. K8s + GPU isolation (measure your code overhead)
4. K8s + Xen hypervisor (final production setup)

### How It Works in K8s

```
┌─────────────────────────────────────┐
│          Your GPU VM                │
│                                     │
│  ┌───────────────────────────────┐ │
│  │   Kubernetes (K3s)            │ │
│  │                               │ │
│  │  ┌─────────────────────────┐ │ │
│  │  │  Pod: mlperf-benchmark  │ │ │
│  │  │                         │ │ │
│  │  │  Runs ResNet50          │ │ │
│  │  │         ↓               │ │ │
│  │  │  nvidia.com/gpu: 1      │ │ │
│  │  └─────────────────────────┘ │ │
│  └───────────────────────────────┘ │
│              ↓                      │
│       NVIDIA Driver                 │
│              ↓                      │
│        Tesla T4 GPU                 │
└─────────────────────────────────────┘
```

## Comparison: What You'll Measure

```
Test 1: Bare Metal
├─ Throughput: 3,500 samples/sec
├─ Latency p99: 15 ms
└─ This is your BASELINE (100%)

Test 2: Kubernetes Pod
├─ Throughput: 3,450 samples/sec  (-1.4% overhead)
├─ Latency p99: 15.2 ms
└─ K8s adds ~1-2% overhead (GOOD)

Test 3: K8s + GPU Proxy
├─ Throughput: 3,325 samples/sec  (-5% overhead)
├─ Latency p99: 16 ms
└─ Your isolation layer should be <5% (TARGET)

Test 4: K8s + Xen Hypervisor
├─ Throughput: ??? samples/sec
├─ Latency p99: ??? ms
└─ Final goal: <10% total overhead
```

## How Results Are Saved

After running, results are saved as:

```
mlperf-benchmark/results/
├── resnet50_bare_metal_20251215_140530.txt
├── resnet50_k8s_20251215_141045.txt
├── resnet50_with_proxy_20251215_142130.txt
└── resnet50_with_hypervisor_20251215_143500.txt
```

Each file contains:
- Throughput (samples/sec)
- Latency percentiles (p50, p90, p99)
- Test duration
- GPU info
- Full MLPerf log

## Quick Commands

### Bare Metal (No K8s)
```bash
./run-mlperf-benchmark.sh
```

### With Kubernetes
```bash
# 1. Install K8s on the VM
./install-k8s-on-vm.sh

# 2. Run benchmark in K8s pod
kubectl apply -f kubernetes/mlperf-pod.yaml
kubectl logs -f mlperf-benchmark

# 3. Copy results
kubectl logs mlperf-benchmark > results/k8s_result.txt
```

### Compare Results
```bash
# Show all results side by side
./analyze-results.sh
```

## What's Next?

1. ✅ Run bare metal baseline
2. ⏳ Run in K8s (measure K8s overhead)
3. ⏳ Add GPU isolation proxy
4. ⏳ Add Xen hypervisor
5. ⏳ Compare all results

## Why This Matters

Your goal is to add GPU isolation **without killing performance**.

**Good Result**:
```
Bare Metal:    3,500 samples/sec
With Xen:      3,325 samples/sec  (5% overhead) ✅
```

**Bad Result**:
```
Bare Metal:    3,500 samples/sec
With Xen:      2,800 samples/sec  (20% overhead) ❌
```

MLPerf gives you **precise, industry-standard measurements** to prove your hypervisor works well.

---

That's it! MLPerf is how you prove your GPU isolation actually works in production. 🚀
