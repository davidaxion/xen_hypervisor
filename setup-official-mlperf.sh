#!/bin/bash
# Setup Official MLPerf Inference Benchmark
# Based on https://docs.mlcommons.org/inference/install/

set -e

INSTANCE="gpu-benchmarking"
ZONE="us-central1-a"
PROJECT="robotic-tide-459208-h4"

echo "╔═══════════════════════════════════════════════════════════════╗"
echo "║     Setting Up Official MLPerf Inference Benchmark            ║"
echo "╚═══════════════════════════════════════════════════════════════╝"
echo ""

gcloud compute ssh $INSTANCE --zone=$ZONE --project=$PROJECT --command='
set -e

echo "Step 1: Installing prerequisites..."
sudo apt-get update
sudo apt-get install -y git python3-pip python3-venv

echo ""
echo "Step 2: Creating MLPerf workspace..."
cd /mnt/data
mkdir -p mlperf-official
cd mlperf-official

echo ""
echo "Step 3: Creating Python virtual environment..."
python3 -m venv mlc
source mlc/bin/activate

echo ""
echo "Step 4: Installing MLCFlow (MLCommons automation framework)..."
pip install --upgrade pip
pip install mlcflow

echo ""
echo "Step 5: Pulling MLPerf Inference repository..."
mlc pull repo --url=mlcommons@mlperf-automations

echo ""
echo "Step 6: Checking available benchmarks..."
echo "Available benchmarks:"
echo "  - resnet50 (Image Classification)"
echo "  - bert (Natural Language Processing)"
echo "  - retinanet (Object Detection)"
echo "  - rnnt (Speech Recognition)"
echo "  - 3d-unet (Medical Imaging)"
echo "  - gptj (Language Generation)"
echo "  - stable-diffusion-xl (Image Generation)"

echo ""
echo "✓ Official MLPerf Inference setup complete!"
echo ""
echo "Location: /mnt/data/mlperf-official"
echo ""
echo "Next steps:"
echo "  1. Activate environment: source mlc/bin/activate"
echo "  2. Run ResNet50: mlcr run-mlperf,inference,_find-performance --model=resnet50 --device=cuda"
'

echo ""
echo "═══════════════════════════════════════════════════════════════"
echo "✓ Official MLPerf Inference installed on GCP"
echo ""
echo "To run benchmarks:"
echo "  1. SSH: gcloud compute ssh $INSTANCE --zone=$ZONE --project=$PROJECT"
echo "  2. cd /mnt/data/mlperf-official"
echo "  3. source mlc/bin/activate"
echo "  4. mlcr run-mlperf,inference --model=resnet50 --device=cuda"
echo ""
