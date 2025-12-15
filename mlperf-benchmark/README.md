# GPU Benchmark on Kubernetes

## Overview

This project benchmarks GPU performance on Kubernetes using ResNet-50 inference. It measures:

1. **Throughput** (Offline Scenario) - Maximum samples/second with batching
2. **Latency** (Server Scenario) - Per-request latency (p90, p99)
3. **Single-Stream** (Edge Scenario) - One sample at a time

## Quick Start

### One-Command Deploy
```bash
cd mlperf-benchmark
./deploy.sh
```

This will:
1. Create GKE cluster with T4 GPU
2. Install NVIDIA drivers
3. Build and push Docker image
4. Run benchmark
5. Save results to `results/baseline/`

**Time**: ~15-20 minutes total
**Cost**: ~$0.50/hour for cluster

## Manual Deployment

### Step 1: Set Up GKE Cluster
```bash
gcloud container clusters create gpu-benchmark-cluster \
    --zone=us-central1-a \
    --project=robotic-tide-459208-h4 \
    --machine-type=n1-standard-4 \
    --accelerator=type=nvidia-tesla-t4,count=1 \
    --num-nodes=1 \
    --enable-autoscaling \
    --min-nodes=0 \
    --max-nodes=2
```

### Step 2: Install NVIDIA Device Plugin
```bash
# Install NVIDIA drivers
kubectl apply -f https://raw.githubusercontent.com/GoogleCloudPlatform/container-engine-accelerators/master/nvidia-driver-installer/cos/daemonset-preloaded.yaml

# Install device plugin
kubectl apply -f https://raw.githubusercontent.com/NVIDIA/k8s-device-plugin/v0.14.0/nvidia-device-plugin.yml

# Verify GPU nodes
kubectl get nodes "-o=custom-columns=NAME:.metadata.name,GPU:.status.allocatable.nvidia\.com/gpu"
```

### Step 3: Build Docker Image
```bash
cd mlperf-benchmark

docker build -t gcr.io/robotic-tide-459208-h4/gpu-benchmark:latest -f docker/Dockerfile .

docker push gcr.io/robotic-tide-459208-h4/gpu-benchmark:latest
```

### Step 4: Deploy Benchmark
```bash
kubectl apply -f kubernetes/benchmark-job.yaml

# Watch progress
kubectl get jobs -w

# View logs
POD=$(kubectl get pods --selector=job-name=gpu-benchmark-baseline -o jsonpath='{.items[0].metadata.name}')
kubectl logs -f $POD
```

### Step 5: Collect Results
```bash
# Copy results from pod
kubectl cp ${POD}:/results/benchmark_results.json results/baseline/benchmark_results.json

# View summary
cat results/baseline/benchmark_results.json | python3 -m json.tool
```

## Benchmark Scenarios

### 1. Offline (Throughput)
**Goal**: Maximum samples/second with unlimited batching

**Batch sizes tested**: 1, 8, 32

**Expected results** (T4 GPU):
- Batch 1: ~200 samples/sec
- Batch 8: ~1,200 samples/sec
- Batch 32: ~3,500 samples/sec

### 2. Server (Latency)
**Goal**: Per-request latency percentiles

**Metrics**: p90, p95, p99 latency in milliseconds

**Expected results** (T4 GPU):
- Batch 1: p99 < 10ms
- Batch 8: p99 < 15ms
- Batch 32: p99 < 30ms

### 3. Single-Stream (Edge)
**Goal**: Minimal latency, one sample at a time

**Batch size**: 1

**Expected results** (T4 GPU):
- p90 latency: ~5ms

## Results Format

Results are saved as JSON:
```json
{
  "timestamp": "2025-12-15T12:30:00",
  "gpu_info": {
    "name": "Tesla T4",
    "cuda_version": "12.0",
    "total_memory_gb": 15.0
  },
  "benchmarks": [
    {
      "scenario": "offline",
      "batch_size": 32,
      "throughput_samples_per_sec": 3500,
      "duration_sec": 30.0
    },
    {
      "scenario": "server",
      "batch_size": 32,
      "latency_p99_ms": 28.5,
      "latency_p90_ms": 25.2
    },
    {
      "scenario": "single_stream",
      "batch_size": 1,
      "latency_p90_ms": 4.8
    }
  ]
}
```

## Project Structure

```
mlperf-benchmark/
├── README.md              # This file
├── deploy.sh              # Automated deployment
├── docker/
│   └── Dockerfile         # Container definition
├── kubernetes/
│   └── benchmark-job.yaml # Kubernetes Job manifest
├── scripts/
│   └── gpu_benchmark.py   # Benchmark script
└── results/
    └── baseline/          # Results saved here
        └── benchmark_results.json
```

## Costs

### GKE Cluster
- **n1-standard-4**: $0.19/hour
- **T4 GPU**: $0.35/hour
- **Total**: ~$0.54/hour

### Optimization
```bash
# Scale to 0 when not in use
gcloud container clusters resize gpu-benchmark-cluster \
    --num-nodes=0 \
    --zone=us-central1-a

# Delete when done
gcloud container clusters delete gpu-benchmark-cluster \
    --zone=us-central1-a
```

## Troubleshooting

### GPU not detected
```bash
# Check NVIDIA device plugin
kubectl get pods -n kube-system | grep nvidia

# Check node GPU allocation
kubectl describe nodes | grep nvidia
```

### Pod stuck in Pending
```bash
# Check events
kubectl describe pod <pod-name>

# Common issues:
# - No GPU nodes available
# - NVIDIA drivers not installed
# - Resource limits too high
```

### Container fails to build
```bash
# Enable Docker BuildKit
export DOCKER_BUILDKIT=1

# Rebuild with no cache
docker build --no-cache -t gcr.io/robotic-tide-459208-h4/gpu-benchmark:latest .
```

## Next Steps

After getting baseline results, we'll:

1. **Add Xen Hypervisor Layer**
   - Deploy libvgpu interceptor
   - Run same benchmark
   - Compare results

2. **Measure Overhead**
   - Target: <5% performance degradation
   - Key metrics: throughput, p99 latency

3. **Multi-Tenant Testing**
   - Run multiple pods
   - Verify isolation
   - Measure interference

## References

- [MLPerf Inference](https://mlcommons.org/benchmarks/inference/)
- [GKE GPUs](https://cloud.google.com/kubernetes-engine/docs/how-to/gpus)
- [NVIDIA k8s-device-plugin](https://github.com/NVIDIA/k8s-device-plugin)
