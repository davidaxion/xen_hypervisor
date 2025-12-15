# Official MLPerf Inference Benchmark Guide

This guide uses the **official MLPerf Inference benchmark** from MLCommons, not a custom implementation.

## Why Official MLPerf?

- ✅ **Industry standard** - Used by NVIDIA, Intel, Google, etc.
- ✅ **Reproducible** - Standardized methodology
- ✅ **Comparable** - Results can be compared with published submissions
- ✅ **Comprehensive** - Tests real ML workloads (ResNet50, BERT, etc.)
- ✅ **Validated** - Reviewed by MLCommons

## Setup (One-time)

```bash
./setup-official-mlperf.sh
```

This installs:
- MLCFlow (MLCommons automation framework)
- Official MLPerf Inference repository
- All dependencies

**Location on GCP**: `/mnt/data/mlperf-official/`

---

## Available Benchmarks

### Image Classification
- **ResNet50** - CNN for image classification (ImageNet)
- Fast, GPU-optimized, good for testing throughput

### Natural Language Processing
- **BERT** - Transformer for question answering (SQuAD)
- Tests GPU memory and compute

### Object Detection
- **RetinaNet** - Object detection (COCO dataset)
- Tests GPU vision processing

### Speech Recognition
- **RNNT** - Recurrent neural transducer
- Tests sequential processing

### Others
- **3D-UNet** - Medical imaging segmentation
- **GPT-J** - Large language model
- **Stable Diffusion XL** - Image generation

---

## Quick Start: ResNet50 Benchmark

### SSH to Instance

```bash
gcloud compute ssh gpu-benchmarking \
    --zone=us-central1-a \
    --project=robotic-tide-459208-h4
```

### Run ResNet50 (Recommended for GPU testing)

```bash
cd /mnt/data/mlperf-official
source mlc/bin/activate

# Run ResNet50 on GPU
mlcr run-mlperf,inference,_find-performance,_full,_r5.1-dev \
  --model=resnet50 \
  --implementation=reference \
  --framework=onnxruntime \
  --category=edge \
  --scenario=Offline \
  --execution_mode=test \
  --device=cuda \
  --quiet
```

### Parameters Explained

- `--model=resnet50` - Image classification model
- `--device=cuda` - Use GPU (not CPU)
- `--scenario=Offline` - Batch processing mode (tests throughput)
- `--execution_mode=test` - Quick test run
- `--framework=onnxruntime` - Optimized ML runtime

---

## Other Scenarios

### Test Latency (Single Stream)

```bash
mlcr run-mlperf,inference \
  --model=resnet50 \
  --device=cuda \
  --scenario=SingleStream \
  --execution_mode=test
```

### Test Real-Time (Server)

```bash
mlcr run-mlperf,inference \
  --model=resnet50 \
  --device=cuda \
  --scenario=Server \
  --execution_mode=test
```

### Run BERT (NLP)

```bash
mlcr run-mlperf,inference \
  --model=bert \
  --device=cuda \
  --scenario=Offline \
  --execution_mode=test
```

---

## Viewing Results

```bash
# Results are saved in:
cd /mnt/data/mlperf-official/results

# View latest results
cat results/*/mlperf_log_summary.txt

# Copy results to local machine
gcloud compute scp --recurse \
    gpu-benchmarking:/mnt/data/mlperf-official/results/ \
    mlperf-benchmark/official-results/ \
    --zone=us-central1-a
```

---

## Running in Kubernetes

Once K8s is working, deploy MLPerf as a pod:

```yaml
apiVersion: v1
kind: Pod
metadata:
  name: mlperf-resnet50
spec:
  restartPolicy: Never
  containers:
  - name: mlperf
    image: mlcommons/mlperf-inference:latest
    command: ["/bin/bash", "-c"]
    args:
      - |
        source /mlperf/mlc/bin/activate
        mlcr run-mlperf,inference --model=resnet50 --device=cuda
    resources:
      limits:
        nvidia.com/gpu: 1
```

---

## Comparison: Custom vs Official

### Our Custom Simple Benchmark
```c
// Simple CUDA memory alloc/free test
Throughput: 6,977 ops/sec
Latency: 0.14 ms
Bandwidth: 4.5 GB/s
```

**Purpose**: Quick GPU health check, measures raw CUDA performance

### Official MLPerf ResNet50
```
// Real ML inference workload
Samples per second: ???
99th percentile latency: ???
Power consumption: ???
```

**Purpose**: Industry-standard ML benchmark, measures real-world performance

---

## Why Use Both?

1. **Simple benchmark** - Fast sanity check (30 seconds)
   - Tests basic GPU functionality
   - Measures raw CUDA performance
   - Good for detecting hardware issues

2. **Official MLPerf** - Comprehensive test (minutes to hours)
   - Tests real ML workloads
   - Industry-comparable results
   - Shows actual application performance

---

## Workflow

```
Step 1: Run simple benchmark
  ↓ (GPU works? Basic performance OK?)

Step 2: Run official MLPerf baseline
  ↓ (Get real ML performance)

Step 3: Add GPU isolation layer
  ↓ (Re-run both benchmarks)

Step 4: Add Xen hypervisor
  ↓ (Final performance comparison)
```

---

## Troubleshooting

### Python version issues
```bash
# MLPerf needs Python 3.8+
python3 --version
```

### CUDA not found
```bash
# Set CUDA path
export CUDA_HOME=/usr/local/cuda
export PATH=$CUDA_HOME/bin:$PATH
export LD_LIBRARY_PATH=$CUDA_HOME/lib64:$LD_LIBRARY_PATH
```

### Out of memory
```bash
# Run smaller batch size
mlcr run-mlperf,inference \
  --model=resnet50 \
  --device=cuda \
  --max_batchsize=1
```

---

## Next Steps

1. ✅ Install official MLPerf: `./setup-official-mlperf.sh`
2. ⏳ Run ResNet50 baseline on bare metal
3. ⏳ Run with GPU isolation layer
4. ⏳ Run with Xen hypervisor
5. ⏳ Compare all results

---

## References

- **Official Docs**: https://docs.mlcommons.org/inference/
- **GitHub**: https://github.com/mlcommons/inference
- **ResNet50 Docs**: https://docs.mlcommons.org/inference/benchmarks/image_classification/resnet50/
- **Submissions**: https://mlcommons.org/benchmarks/inference-datacenter/

---

This is the industry-standard way to benchmark GPU ML performance! 🚀
