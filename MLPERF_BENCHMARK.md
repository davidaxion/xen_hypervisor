# MLPerf Inference Benchmark on Kubernetes

## Overview

We'll run MLPerf Inference benchmarks as a Kubernetes pod to establish baseline GPU performance. Later, we'll add our Xen hypervisor layer and compare the overhead.

## MLPerf Metrics We'll Measure

```
┌─────────────────────────────────────────────────────────────────────────────┐
│ MLPerf Inference Metrics                                                     │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                              │
│ 1. THROUGHPUT (Offline Scenario)                                            │
│    "How many samples can you process if you have unlimited batching?"       │
│    Metric: samples/second                                                   │
│    Example: 42,000 images/sec for ResNet-50                                 │
│                                                                              │
│ 2. LATENCY (Server Scenario)                                                │
│    "How fast is each individual request?"                                   │
│    Metric: p90/p99 latency in milliseconds                                  │
│    Example: p99 < 15ms for ResNet-50                                        │
│                                                                              │
│ 3. SINGLE-STREAM (Edge Scenario)                                            │
│    "One request at a time, how fast?"                                       │
│    Metric: p90 latency                                                      │
│    Example: 0.5ms per image                                                 │
│                                                                              │
└─────────────────────────────────────────────────────────────────────────────┘
```

## Architecture

```
┌────────────────────────────────────────────────────────────────┐
│                     Kubernetes Cluster                          │
│                                                                 │
│  ┌─────────────────────────────────────────────────────────┐   │
│  │              MLPerf Benchmark Pod                        │   │
│  │                                                          │   │
│  │  ┌──────────────────┐      ┌──────────────────┐        │   │
│  │  │  ResNet-50       │      │  BERT-Large      │        │   │
│  │  │  (Image Class)   │      │  (NLP)           │        │   │
│  │  └────────┬─────────┘      └────────┬─────────┘        │   │
│  │           │                          │                  │   │
│  │           └──────────┬───────────────┘                  │   │
│  │                      │                                  │   │
│  │                      ↓                                  │   │
│  │           ┌──────────────────────┐                     │   │
│  │           │  CUDA Runtime        │                     │   │
│  │           └──────────┬───────────┘                     │   │
│  │                      │                                  │   │
│  └──────────────────────┼──────────────────────────────────┘   │
│                         │                                      │
│                         ↓                                      │
│              ┌──────────────────────┐                          │
│              │   NVIDIA Driver      │                          │
│              └──────────┬───────────┘                          │
└──────────────────────────┼──────────────────────────────────────┘
                           │
                           ↓
                    ┌──────────────┐
                    │  Tesla T4    │
                    │  GPU         │
                    └──────────────┘

                    BASELINE (No Hypervisor)
```

Then later:

```
┌────────────────────────────────────────────────────────────────┐
│                     Kubernetes Cluster                          │
│                                                                 │
│  ┌─────────────────────────────────────────────────────────┐   │
│  │              MLPerf Benchmark Pod                        │   │
│  │              (User Domain / DomU)                        │   │
│  │                                                          │   │
│  │  ┌──────────────────┐      ┌──────────────────┐        │   │
│  │  │  ResNet-50       │      │  BERT-Large      │        │   │
│  │  └────────┬─────────┘      └────────┬─────────┘        │   │
│  │           │                          │                  │   │
│  │           └──────────┬───────────────┘                  │   │
│  │                      │                                  │   │
│  │                      ↓                                  │   │
│  │           ┌──────────────────────┐                     │   │
│  │           │    libvgpu.so        │ ← Our interceptor   │   │
│  │           └──────────┬───────────┘                     │   │
│  │                      │                                  │   │
│  └──────────────────────┼──────────────────────────────────┘   │
│                         │ IDM                                  │
│         ════════════════╪════════════════                      │
│         Hypervisor (Xen)                                       │
│         ════════════════╪════════════════                      │
│                         │                                      │
│  ┌──────────────────────┼──────────────────────────────────┐   │
│  │     Driver Domain (Dom0)                                │   │
│  │                      ↓                                  │   │
│  │           ┌──────────────────────┐                     │   │
│  │           │   GPU Proxy          │                     │   │
│  │           └──────────┬───────────┘                     │   │
│  │                      │                                  │   │
│  │           ┌──────────┴───────────┐                     │   │
│  │           │   NVIDIA Driver      │                     │   │
│  │           └──────────┬───────────┘                     │   │
│  └──────────────────────┼──────────────────────────────────┘   │
└──────────────────────────┼──────────────────────────────────────┘
                           │ PCI Passthrough
                           ↓
                    ┌──────────────┐
                    │  Tesla T4    │
                    │  GPU         │
                    └──────────────┘

                    WITH HYPERVISOR
```

## Implementation Plan

### Phase 1: Baseline Benchmarking (No Hypervisor)

#### Step 1: Set up Kubernetes on GCP
```bash
# Option A: Use GKE (easiest)
gcloud container clusters create mlperf-cluster \
    --zone=us-central1-a \
    --machine-type=n1-standard-4 \
    --accelerator=type=nvidia-tesla-t4,count=1 \
    --num-nodes=1 \
    --enable-autoscaling \
    --min-nodes=1 \
    --max-nodes=3

# Install NVIDIA device plugin
kubectl apply -f https://raw.githubusercontent.com/NVIDIA/k8s-device-plugin/main/nvidia-device-plugin.yml

# Verify GPU nodes
kubectl get nodes -o json | jq '.items[].status.capacity'
```

#### Step 2: Deploy MLPerf Container
```yaml
# mlperf-pod.yaml
apiVersion: v1
kind: Pod
metadata:
  name: mlperf-resnet50
spec:
  containers:
  - name: mlperf
    image: nvcr.io/nvidia/pytorch:24.01-py3
    command:
      - /bin/bash
      - -c
      - |
        # Install MLPerf loadgen
        pip install mlperf-loadgen

        # Download ResNet-50 model
        wget https://zenodo.org/record/2535873/files/resnet50_v1.pb

        # Run benchmark
        python3 /workspace/mlperf/run_resnet50.py \
          --scenario Offline \
          --model resnet50_v1.pb \
          --dataset imagenet \
          --count 50000
    resources:
      limits:
        nvidia.com/gpu: 1
```

#### Step 3: Run Benchmarks
```bash
# Deploy pod
kubectl apply -f mlperf-pod.yaml

# Watch logs
kubectl logs -f mlperf-resnet50

# Collect results
kubectl cp mlperf-resnet50:/results ./baseline-results/
```

### Phase 2: With Hypervisor

#### Step 1: Build Custom Node Image with Xen
```bash
# Use Packer to build image
packer build \
  -var "project_id=robotic-tide-459208-h4" \
  -var "zone=us-central1-a" \
  image-builder/xen-node.json
```

#### Step 2: Deploy Modified Pod
```yaml
# mlperf-pod-xen.yaml
apiVersion: v1
kind: Pod
metadata:
  name: mlperf-resnet50-xen
spec:
  runtimeClassName: xen-gpu  # ← Custom runtime
  containers:
  - name: mlperf
    image: nvcr.io/nvidia/pytorch:24.01-py3
    volumeMounts:
    - name: libvgpu
      mountPath: /usr/local/lib/libcuda.so.1
      subPath: libcuda.so.1
    env:
    - name: LD_LIBRARY_PATH
      value: /usr/local/lib
    # ... same benchmark command
  volumes:
  - name: libvgpu
    hostPath:
      path: /opt/gpu-isolation/libvgpu/libcuda.so.1
```

#### Step 3: Compare Results
```bash
# Run both
kubectl apply -f mlperf-pod.yaml
kubectl apply -f mlperf-pod-xen.yaml

# Collect results
kubectl cp mlperf-resnet50:/results ./baseline-results/
kubectl cp mlperf-resnet50-xen:/results ./xen-results/

# Compare
python3 compare_results.py baseline-results/ xen-results/
```

## Expected Results

### Baseline (No Hypervisor)
```
ResNet-50 Offline:
  Throughput: 42,000 images/sec
  Latency p99: 12ms
  GPU Utilization: 98%

BERT-Large Server:
  Throughput: 8,500 queries/sec
  Latency p99: 15ms
  GPU Memory: 14.2 GB
```

### With Xen Hypervisor (Target)
```
ResNet-50 Offline:
  Throughput: 39,900 images/sec    (5% overhead) ✅
  Latency p99: 13ms                (+1ms)         ✅
  GPU Utilization: 96%

BERT-Large Server:
  Throughput: 8,075 queries/sec    (5% overhead) ✅
  Latency p99: 16ms                (+1ms)         ✅
  GPU Memory: 14.2 GB
```

**Success Criteria**: <5% performance overhead

## Detailed Implementation

### 1. MLPerf ResNet-50 Benchmark Script
```python
# run_resnet50.py
import mlperf_loadgen as lg
import numpy as np
import tensorflow as tf
from PIL import Image
import time

class ResNet50SUT:
    def __init__(self, model_path):
        # Load model
        self.graph = tf.Graph()
        with self.graph.as_default():
            with open(model_path, 'rb') as f:
                graph_def = tf.compat.v1.GraphDef()
                graph_def.ParseFromString(f.read())
                tf.import_graph_def(graph_def, name='')

        # Create session
        config = tf.compat.v1.ConfigProto()
        config.gpu_options.allow_growth = True
        self.sess = tf.compat.v1.Session(graph=self.graph, config=config)

        # Get input/output tensors
        self.input_tensor = self.graph.get_tensor_by_name('input_tensor:0')
        self.output_tensor = self.graph.get_tensor_by_name('ArgMax:0')

    def issue_queries(self, query_samples):
        """Process batch of samples"""
        batch_size = len(query_samples)
        batch_data = np.zeros((batch_size, 224, 224, 3), dtype=np.float32)

        # Preprocess images
        for i, sample in enumerate(query_samples):
            batch_data[i] = self.preprocess(sample.index)

        # Run inference
        start = time.time()
        predictions = self.sess.run(self.output_tensor,
                                     feed_dict={self.input_tensor: batch_data})
        latency = time.time() - start

        # Return results
        for i, sample in enumerate(query_samples):
            response = lg.QuerySampleResponse(sample.id,
                                               predictions[i:i+1].tobytes(),
                                               batch_data[i:i+1].nbytes)
            lg.QuerySamplesComplete([response])

    def preprocess(self, image_idx):
        # Load and preprocess ImageNet image
        img = Image.open(f'/data/imagenet/val/{image_idx}.JPEG')
        img = img.resize((224, 224))
        img = np.array(img).astype(np.float32) / 255.0
        return img

# Configure MLPerf
settings = lg.TestSettings()
settings.scenario = lg.TestScenario.Offline
settings.mode = lg.TestMode.PerformanceOnly
settings.offline_expected_qps = 40000  # Target QPS

# Run benchmark
sut = ResNet50SUT('resnet50_v1.pb')
lg.StartTest(sut, settings)

# Results are automatically saved to mlperf_log_summary.txt
```

### 2. Comparison Script
```python
# compare_results.py
import json
import sys

def load_results(path):
    with open(f'{path}/mlperf_log_summary.txt') as f:
        results = {}
        for line in f:
            if 'Samples per second' in line:
                results['throughput'] = float(line.split(':')[1].strip())
            elif '90.00 percentile latency' in line:
                results['p90_latency'] = float(line.split(':')[1].strip())
            elif '99.00 percentile latency' in line:
                results['p99_latency'] = float(line.split(':')[1].strip())
        return results

baseline = load_results(sys.argv[1])
xen = load_results(sys.argv[2])

print("=" * 60)
print("MLPerf Inference Results Comparison")
print("=" * 60)
print()
print(f"{'Metric':<30} {'Baseline':<15} {'Xen':<15} {'Overhead':<10}")
print("-" * 60)

# Throughput
throughput_overhead = (1 - xen['throughput']/baseline['throughput']) * 100
print(f"{'Throughput (samples/sec)':<30} "
      f"{baseline['throughput']:<15.0f} "
      f"{xen['throughput']:<15.0f} "
      f"{throughput_overhead:>9.1f}%")

# p90 Latency
p90_diff = xen['p90_latency'] - baseline['p90_latency']
print(f"{'p90 Latency (ms)':<30} "
      f"{baseline['p90_latency']:<15.2f} "
      f"{xen['p90_latency']:<15.2f} "
      f"{p90_diff:>8.2f}ms")

# p99 Latency
p99_diff = xen['p99_latency'] - baseline['p99_latency']
print(f"{'p99 Latency (ms)':<30} "
      f"{baseline['p99_latency']:<15.2f} "
      f"{xen['p99_latency']:<15.2f} "
      f"{p99_diff:>8.2f}ms")

print()
print("=" * 60)

# Success criteria
if throughput_overhead < 5:
    print("✅ Throughput overhead < 5% - PASS")
else:
    print("❌ Throughput overhead >= 5% - FAIL")

if p99_diff < 2:
    print("✅ p99 latency increase < 2ms - PASS")
else:
    print("❌ p99 latency increase >= 2ms - FAIL")
```

### 3. Kubernetes RuntimeClass for Xen
```yaml
# xen-runtime-class.yaml
apiVersion: node.k8s.io/v1
kind: RuntimeClass
metadata:
  name: xen-gpu
handler: xen-cri  # Points to our CRI runtime
scheduling:
  nodeSelector:
    xen-enabled: "true"
overhead:
  podFixed:
    memory: "256Mi"
    cpu: "100m"
```

## Quick Start Commands

```bash
# 1. Clone the project
cd gpu-isolation

# 2. Create GKE cluster
gcloud container clusters create mlperf-cluster \
    --zone=us-central1-a \
    --machine-type=n1-standard-8 \
    --accelerator=type=nvidia-tesla-t4,count=1 \
    --num-nodes=1

# 3. Install NVIDIA device plugin
kubectl apply -f https://raw.githubusercontent.com/NVIDIA/k8s-device-plugin/main/nvidia-device-plugin.yml

# 4. Run baseline benchmark
kubectl apply -f mlperf/baseline-pod.yaml
kubectl logs -f mlperf-resnet50 | tee baseline.log

# 5. Deploy Xen runtime (after Phase 2 implementation)
kubectl apply -f mlperf/xen-runtime-class.yaml
kubectl apply -f mlperf/xen-pod.yaml
kubectl logs -f mlperf-resnet50-xen | tee xen.log

# 6. Compare results
python3 mlperf/compare_results.py baseline.log xen.log
```

## Timeline

- **Week 1**: Set up GKE cluster, run baseline MLPerf
- **Week 2**: Build Xen-enabled node image
- **Week 3**: Implement CRI runtime, integrate with libvgpu
- **Week 4**: Run MLPerf with Xen, optimize performance

## Success Metrics

✅ **Baseline established**: Know exact GPU performance
✅ **Overhead measured**: Quantify Xen impact
✅ **Target achieved**: <5% overhead
✅ **Reproducible**: Can re-run benchmarks anytime
✅ **Production-ready**: Proven on industry-standard benchmark

## Next Steps

1. Create `mlperf/` directory in project
2. Write Dockerfile for MLPerf container
3. Set up GKE cluster on GCP
4. Run baseline benchmarks
5. Document results
6. Proceed with Xen integration

---

**Ready to proceed?** Let me know if you want me to start implementing the MLPerf benchmark infrastructure!
