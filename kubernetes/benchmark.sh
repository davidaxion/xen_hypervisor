#!/bin/bash
set -e

echo "╔═══════════════════════════════════════════════════════════╗"
echo "║     MLPerf ResNet50 Benchmark in Kubernetes              ║"
echo "╚═══════════════════════════════════════════════════════════╝"
echo ""

echo "=== GPU Information ==="
nvidia-smi --query-gpu=name,driver_version,memory.total --format=csv
echo ""

echo "=== Installing Dependencies ==="
apt-get update -qq
apt-get install -y -qq python3 python3-pip wget git
pip3 install --quiet numpy onnxruntime-gpu pillow

echo ""
echo "=== Setting up workspace on /mnt/data ==="
mkdir -p /mnt/data/mlperf
cd /mnt/data/mlperf

echo ""
echo "=== Downloading ResNet50 Model ==="
wget -q https://zenodo.org/record/2592612/files/resnet50_v1.onnx

echo ""
echo "=== Creating Sample Images ==="
python3 /scripts/create_images.py

echo ""
echo "=== Running ResNet50 Inference Benchmark ==="
python3 /scripts/run_benchmark.py

echo ""
echo "✓ Benchmark complete! Pod will remain running for log inspection."
sleep infinity
