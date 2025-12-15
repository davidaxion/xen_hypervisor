# Benchmark Results

All GPU benchmark results are stored here for comparison and analysis.

## Current Results

### 1. Baseline (Bare Metal)
**File**: `baseline_results.txt`
**Location**: Local (manually created from GCP output)
**Environment**: Direct GPU access, no virtualization
**Date**: 2025-12-15

**Key Metrics**:
- Throughput: 6,977 ops/sec
- Average Latency: 0.14 ms (140 microseconds)
- p99 Latency: 0.198 ms
- Memory Bandwidth: 4.31 GB/s (H2D), 4.53 GB/s (D2H)

---

## Planned Results

### 2. Kubernetes Pod (Bare Metal)
**File**: `k8s-baseline-results.txt` (not yet created)
**Command to generate**:
```bash
# On GCP instance
kubectl logs gpu-benchmark > /mnt/data/k8s-baseline-results.txt

# Copy to local
gcloud compute scp gpu-benchmarking:/mnt/data/k8s-baseline-results.txt \
    mlperf-benchmark/results/ \
    --zone=us-central1-a \
    --project=robotic-tide-459208-h4
```

### 3. With GPU Proxy (No Hypervisor)
**File**: `with-gpu-proxy-results.txt` (not yet created)
**Command to generate**:
```bash
# On GCP instance, with gpu_proxy running
cd /mnt/data/gpu-proxy/libvgpu
./test_app | tee /mnt/data/with-gpu-proxy-results.txt

# Copy to local
gcloud compute scp gpu-benchmarking:/mnt/data/with-gpu-proxy-results.txt \
    mlperf-benchmark/results/ \
    --zone=us-central1-a \
    --project=robotic-tide-459208-h4
```

### 4. Kubernetes + GPU Proxy
**File**: `k8s-with-proxy-results.txt` (not yet created)
**Environment**: K8s pod → GPU proxy daemonset → GPU
**Purpose**: Measure K8s + isolation overhead

### 5. Kubernetes + Xen Hypervisor (Final)
**File**: `k8s-with-hypervisor-results.txt` (not yet created)
**Environment**: K8s pod → Xen hypervisor → GPU
**Purpose**: Full production setup benchmark

---

## Results Structure

```
mlperf-benchmark/results/
├── README.md                        # This file
├── baseline_results.txt             # ✅ Bare metal baseline
├── k8s-baseline-results.txt         # ⏳ K8s pod (no isolation)
├── with-gpu-proxy-results.txt       # ⏳ Bare metal + GPU proxy
├── k8s-with-proxy-results.txt       # ⏳ K8s + GPU proxy
├── k8s-with-hypervisor-results.txt  # ⏳ K8s + Xen (final goal)
└── comparison.md                    # ⏳ Side-by-side comparison
```

---

## Copying Results from GCP

### Quick Commands

```bash
# Copy all results at once
gcloud compute scp "gpu-benchmarking:/mnt/data/*-results.txt" \
    mlperf-benchmark/results/ \
    --zone=us-central1-a \
    --project=robotic-tide-459208-h4

# Or individual files
gcloud compute scp gpu-benchmarking:/mnt/data/k8s-baseline-results.txt \
    mlperf-benchmark/results/ \
    --zone=us-central1-a
```

### From Kubernetes Pods

```bash
# Get logs from K8s pod
kubectl logs gpu-benchmark > /mnt/data/k8s-baseline-results.txt

# Then copy to local
gcloud compute scp gpu-benchmarking:/mnt/data/k8s-baseline-results.txt \
    mlperf-benchmark/results/ \
    --zone=us-central1-a
```

---

## Analysis

Once all results are collected, create `comparison.md`:

```bash
./analyze-results.sh  # Script to compare all benchmarks
```

**Expected Overhead Targets**:
- K8s alone: < 1%
- GPU proxy: < 5%
- Full hypervisor: < 10%

---

## On GCP Instance

Results are stored in `/mnt/data/` on the GPU instance:

```
/mnt/data/
├── baseline.txt                    # Bare metal benchmark
├── k8s-baseline-results.txt        # K8s pod benchmark
├── with-gpu-proxy-results.txt      # With GPU isolation
└── k8s-with-proxy-results.txt      # K8s + GPU isolation
```

**Remember to copy them back to this repo after each test!**

---

## Comparison Script (Coming Soon)

```bash
#!/bin/bash
# analyze-results.sh - Compare all benchmark results

echo "=== GPU Benchmark Comparison ==="
echo ""

echo "1. Baseline (Bare Metal):"
grep "Throughput:" mlperf-benchmark/results/baseline_results.txt

echo ""
echo "2. Kubernetes Pod:"
grep "Throughput:" mlperf-benchmark/results/k8s-baseline-results.txt

echo ""
echo "3. With GPU Proxy:"
grep "Throughput:" mlperf-benchmark/results/with-gpu-proxy-results.txt

echo ""
echo "4. K8s + GPU Proxy:"
grep "Throughput:" mlperf-benchmark/results/k8s-with-proxy-results.txt

echo ""
echo "5. K8s + Hypervisor:"
grep "Throughput:" mlperf-benchmark/results/k8s-with-hypervisor-results.txt
```
