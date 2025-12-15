# MLPerf Inference Benchmark

Official MLPerf Inference benchmarks for GPU performance testing.

## Overview

This uses the **official MLPerf Inference benchmark** from MLCommons:
- Industry standard used by NVIDIA, Intel, Google, AMD
- Reproducible methodology
- Comparable results with published submissions
- Real ML workloads (ResNet50, BERT, etc.)

## Setup

Run the automated setup script:

```bash
./setup-official-mlperf.sh
```

This installs:
- MLCFlow (MLCommons automation framework)
- Official MLPerf Inference repository
- Python environment with all dependencies

**Installation location**: `/mnt/data/mlperf-official/` on GCP instance

## Quick Start

### 1. SSH to GCP Instance

```bash
gcloud compute ssh gpu-benchmarking \
    --zone=us-central1-a \
    --project=robotic-tide-459208-h4
```

### 2. Run ResNet50 Benchmark

```bash
cd /mnt/data/mlperf-official
source mlc/bin/activate

# Run ResNet50 on GPU (Offline scenario)
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

## Available Benchmarks

- **ResNet50** - Image classification (recommended for GPU testing)
- **BERT** - Natural language processing
- **RetinaNet** - Object detection
- **RNNT** - Speech recognition
- **3D-UNet** - Medical imaging
- **GPT-J** - Large language model
- **Stable Diffusion XL** - Image generation

## Scenarios

- **Offline** - Batch processing (tests throughput)
- **SingleStream** - Single request (tests latency)
- **MultiStream** - Multiple streams (tests concurrent processing)
- **Server** - Real-time server scenario

## Results

Results are saved in: `/mnt/data/mlperf-official/results/`

### Copy Results to Local

```bash
gcloud compute scp --recurse \
    gpu-benchmarking:/mnt/data/mlperf-official/results/ \
    mlperf-benchmark/results/ \
    --zone=us-central1-a
```

## Documentation

- **Full Guide**: See `RUN_OFFICIAL_MLPERF.md` in project root
- **Official Docs**: https://docs.mlcommons.org/inference/
- **GitHub**: https://github.com/mlcommons/inference
- **ResNet50 Docs**: https://docs.mlcommons.org/inference/benchmarks/image_classification/resnet50/

## Workflow

```
1. Bare Metal Baseline
   ↓ Run MLPerf ResNet50 on bare metal

2. With GPU Isolation
   ↓ Run through GPU proxy layer

3. With Xen Hypervisor
   ↓ Run in full virtualized environment

4. Compare Results
   ↓ Measure overhead at each stage
```

## References

- MLCommons: https://mlcommons.org
- MLPerf Inference: https://mlcommons.org/benchmarks/inference-datacenter/
- Submissions Database: https://mlcommons.org/benchmarks/inference-datacenter/
