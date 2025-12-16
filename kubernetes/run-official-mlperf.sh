#!/bin/bash
# Run Official MLPerf Inference ResNet50 Benchmark

set -e

echo "╔═══════════════════════════════════════════════════════════╗"
echo "║     Running Official MLPerf Inference ResNet50           ║"
echo "╚═══════════════════════════════════════════════════════════╝"
echo ""

cd /mnt/data/inference/vision/classification_and_detection

echo "=== GPU Information ==="
nvidia-smi --query-gpu=name,driver_version,memory.total --format=csv
echo ""

echo "=== Running Official MLPerf ResNet50 Benchmark ==="
echo "Scenario: Offline"
echo "Model: ResNet50 ONNX"
echo "Backend: ONNX Runtime with CUDA"
echo ""

# Run the official MLPerf benchmark
python3 python/main.py \
  --model models/resnet50_v1.onnx \
  --backend onnxruntime \
  --scenario Offline \
  --mlperf-conf ../../mlperf.conf \
  --user-conf user.conf \
  --count 500 \
  --max-batchsize 32 \
  --output /mnt/data/mlperf-official-results

echo ""
echo "╔═══════════════════════════════════════════════════════════╗"
echo "║     Official MLPerf Benchmark Complete!                  ║"
echo "╚═══════════════════════════════════════════════════════════╝"
echo ""
echo "Results saved to: /mnt/data/mlperf-official-results"
echo ""
echo "To view summary:"
echo "  cat /mnt/data/mlperf-official-results/mlperf_log_summary.txt"
echo ""
