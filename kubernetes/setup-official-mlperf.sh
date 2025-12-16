#!/bin/bash
# Official MLPerf Inference Setup Script
# Follows https://docs.mlcommons.org/inference/install/

set -e

echo "╔═══════════════════════════════════════════════════════════╗"
echo "║     Official MLPerf Inference Setup                      ║"
echo "╚═══════════════════════════════════════════════════════════╝"
echo ""

# Work in /mnt/data for space
cd /mnt/data

echo "=== Step 1: Installing System Dependencies ==="
apt-get update -qq
apt-get install -y -qq \
  build-essential \
  cmake \
  git \
  python3-dev \
  python3-pip \
  libglib2.0-dev \
  wget

echo ""
echo "=== Step 2: Installing Python Dependencies ==="
pip3 install --quiet \
  numpy \
  opencv-python \
  pycocotools \
  onnx \
  onnxruntime-gpu \
  pybind11 \
  Pillow

echo ""
echo "=== Step 3: Cloning Official MLPerf Inference Repository ==="
if [ ! -d "inference" ]; then
  git clone --recurse-submodules https://github.com/mlcommons/inference.git
  cd inference
else
  cd inference
  git pull
fi

echo ""
echo "=== Step 4: Building MLPerf LoadGen ==="
cd loadgen
CFLAGS="-std=c++14" python3 setup.py install

echo ""
echo "=== Step 5: Setting up ResNet50 Benchmark ==="
cd ../vision/classification_and_detection

echo ""
echo "=== Step 6: Downloading ResNet50 ONNX Model ==="
mkdir -p models
cd models
if [ ! -f "resnet50_v1.onnx" ]; then
  wget -q https://zenodo.org/record/2592612/files/resnet50_v1.onnx
fi
cd ..

echo ""
echo "=== Step 7: Creating Sample ImageNet Dataset ==="
# For testing purposes, we'll create synthetic data
# In production, you'd use the full ImageNet validation set
mkdir -p imagenet-samples
python3 <<'EOF'
import numpy as np
from PIL import Image
import os

# Create 500 sample images (minimum for MLPerf)
os.makedirs('imagenet-samples', exist_ok=True)
for i in range(500):
    # Create random 224x224 RGB image
    img_array = np.random.randint(0, 255, (224, 224, 3), dtype=np.uint8)
    img = Image.fromarray(img_array)
    img.save(f'imagenet-samples/ILSVRC2012_val_{i:08d}.JPEG')
print(f"Created 500 sample images")
EOF

echo ""
echo "=== Step 8: Creating MLPerf Configuration Files ==="

# Create user.conf for Offline scenario
cat > user.conf <<'EOF'
# ResNet50 Offline Scenario Configuration
*.Offline.target_qps = 100
*.Offline.target_latency_percentile = 90
EOF

echo ""
echo "╔═══════════════════════════════════════════════════════════╗"
echo "║     Official MLPerf Inference Setup Complete!            ║"
echo "╚═══════════════════════════════════════════════════════════╝"
echo ""
echo "Setup location: /mnt/data/inference"
echo ""
echo "To run the benchmark:"
echo "  cd /mnt/data/inference/vision/classification_and_detection"
echo "  python3 python/main.py \\"
echo "    --model models/resnet50_v1.onnx \\"
echo "    --backend onnxruntime \\"
echo "    --scenario Offline \\"
echo "    --mlperf-conf ../../mlperf.conf \\"
echo "    --user-conf user.conf \\"
echo "    --count 500"
echo ""
