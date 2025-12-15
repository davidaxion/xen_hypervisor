#!/bin/bash
# Complete MLPerf Benchmark Runner
# Runs official MLPerf inference benchmarks and saves results

set -e

INSTANCE="gpu-benchmarking"
ZONE="us-central1-a"
PROJECT="robotic-tide-459208-h4"
RESULTS_DIR="mlperf-benchmark/results"

echo "╔═══════════════════════════════════════════════════════════════╗"
echo "║           Official MLPerf Inference Benchmark Runner          ║"
echo "╚═══════════════════════════════════════════════════════════════╝"
echo ""

# Create results directory
mkdir -p "$RESULTS_DIR"

echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo "Step 1: Installing Official MLPerf on Fresh Instance"
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo ""

gcloud compute ssh $INSTANCE --zone=$ZONE --project=$PROJECT --command='
set -e

echo "Checking disk space..."
df -h /mnt/data

echo ""
echo "Installing Python and dependencies..."
# Download and install pip without apt (to avoid broken packages)
cd /mnt/data
curl https://bootstrap.pypa.io/get-pip.py -o get-pip.py
python3 get-pip.py --break-system-packages

# Add to PATH
export PATH=$HOME/.local/bin:$PATH
echo "export PATH=\$HOME/.local/bin:\$PATH" >> ~/.bashrc

echo ""
echo "Installing Git manually (avoiding apt)..."
cd /mnt/data
wget https://github.com/git/git/archive/refs/tags/v2.43.0.tar.gz
tar -xzf v2.43.0.tar.gz
cd git-2.43.0
make configure
./configure --prefix=/mnt/data/git-install
make all
make install
export PATH=/mnt/data/git-install/bin:$PATH
echo "export PATH=/mnt/data/git-install/bin:\$PATH" >> ~/.bashrc

echo ""
echo "Installing MLPerf..."
cd /mnt/data/mlperf-official
python3 -m pip install --break-system-packages mlcommons

echo ""
echo "Cloning MLPerf Inference repository..."
cd /mnt/data
git clone https://github.com/mlcommons/inference.git mlperf-inference

echo ""
echo "✓ MLPerf installed"
'

echo ""
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo "Step 2: Running ResNet50 Benchmark (Bare Metal)"
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo ""

gcloud compute ssh $INSTANCE --zone=$ZONE --project=$PROJECT --command='
set -e
export PATH=$HOME/.local/bin:/mnt/data/git-install/bin:$PATH

cd /mnt/data/mlperf-inference

echo "Installing ResNet50 dependencies..."
cd vision/classification_and_detection
python3 setup.py install --user

echo ""
echo "Downloading ResNet50 model..."
mkdir -p models
cd models
wget https://zenodo.org/record/2535873/files/resnet50_v1.pb -O resnet50_v1.pb

echo ""
echo "Running ResNet50 benchmark..."
cd ..
python3 -m onnxruntime.backend.classification \
    --model models/resnet50_v1.pb \
    --dataset imagenet \
    --backend onnxruntime \
    --scenario Offline \
    --max_batchsize 32 \
    --count 1000 \
    --time 60 \
    2>&1 | tee /mnt/data/resnet50_bare_metal_results.txt

echo ""
echo "✓ Bare metal benchmark complete"
echo "Results saved to: /mnt/data/resnet50_bare_metal_results.txt"
'

echo ""
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo "Step 3: Copying Results to Local Machine"
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo ""

gcloud compute scp \
    $INSTANCE:/mnt/data/resnet50_bare_metal_results.txt \
    $RESULTS_DIR/resnet50_bare_metal_$(date +%Y%m%d_%H%M%S).txt \
    --zone=$ZONE \
    --project=$PROJECT

echo ""
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo "Step 4: Analyzing Results"
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo ""

LATEST_RESULT=$(ls -t $RESULTS_DIR/resnet50_bare_metal_*.txt | head -1)

echo "Latest results: $LATEST_RESULT"
echo ""
cat "$LATEST_RESULT"

echo ""
echo "╔═══════════════════════════════════════════════════════════════╗"
echo "║                    Benchmark Complete!                        ║"
echo "╚═══════════════════════════════════════════════════════════════╝"
echo ""
echo "Results saved in: $RESULTS_DIR/"
echo ""
echo "Next steps:"
echo "  1. Review results above"
echo "  2. Run with GPU isolation layer"
echo "  3. Run with Xen hypervisor"
echo "  4. Compare all results"
echo ""
