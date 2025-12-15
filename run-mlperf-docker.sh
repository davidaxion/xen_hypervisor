#!/bin/bash
# MLPerf Benchmark using Official Docker Image
# Simplest way to run MLPerf - uses official containers

set -e

INSTANCE="gpu-benchmarking"
ZONE="us-central1-a"
PROJECT="robotic-tide-459208-h4"
RESULTS_DIR="mlperf-benchmark/results"

echo "╔═══════════════════════════════════════════════════════════════╗"
echo "║      Official MLPerf Inference (Docker Version)              ║"
echo "╚═══════════════════════════════════════════════════════════════╝"
echo ""

mkdir -p "$RESULTS_DIR"

echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo "Step 1: Running MLPerf ResNet50 Benchmark in Docker"
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo ""

gcloud compute ssh $INSTANCE --zone=$ZONE --project=$PROJECT --command='
set -e

echo "Checking Docker..."
if ! command -v docker &> /dev/null; then
    echo "Installing Docker..."
    curl -fsSL https://get.docker.com -o get-docker.sh
    sudo sh get-docker.sh
    sudo usermod -aG docker $USER
    echo "Docker installed. You may need to log out and back in for group changes."
fi

echo ""
echo "Checking NVIDIA Container Toolkit..."
if ! docker run --rm --gpus all nvidia/cuda:13.1.0-base-ubuntu22.04 nvidia-smi &> /dev/null; then
    echo "Installing NVIDIA Container Toolkit..."

    # Clean install
    distribution=$(. /etc/os-release;echo $ID$VERSION_ID)
    curl -s -L https://nvidia.github.io/libnvidia-container/gpgkey | sudo apt-key add -
    curl -s -L https://nvidia.github.io/libnvidia-container/stable/deb/nvidia-container-toolkit.list | \
        sudo tee /etc/apt/sources.list.d/nvidia-container-toolkit.list

    sudo apt-get update
    sudo apt-get install -y nvidia-container-toolkit
    sudo systemctl restart docker
fi

echo ""
echo "Testing GPU access in Docker..."
docker run --rm --gpus all nvidia/cuda:13.1.0-base-ubuntu22.04 nvidia-smi

echo ""
echo "Creating workspace on 400GB disk..."
mkdir -p /mnt/data/mlperf-docker
cd /mnt/data/mlperf-docker

echo ""
echo "Running MLPerf ResNet50 benchmark..."
echo "This will take 2-5 minutes..."
echo ""

# Run official MLPerf inference Docker container
docker run --rm --gpus all \
    -v /mnt/data/mlperf-docker:/results \
    mlcommons/inference:datacenter-gpu-latest \
    /bin/bash -c "
    echo \"=== MLPerf ResNet50 Benchmark ===\"
    echo \"\"
    echo \"GPU Info:\"
    nvidia-smi --query-gpu=name,driver_version,memory.total --format=csv
    echo \"\"
    echo \"Starting benchmark...\"
    echo \"\"

    # Run ResNet50 benchmark
    cd /workspace/inference/vision/classification_and_detection

    # Download model if not exists
    if [ ! -f resnet50_v1.onnx ]; then
        echo \"Downloading ResNet50 model...\"
        wget -q https://zenodo.org/record/2592612/files/resnet50_v1.onnx
    fi

    # Download sample images
    if [ ! -d imagenet_samples ]; then
        echo \"Downloading sample images...\"
        mkdir -p imagenet_samples
        wget -q http://image-net.org/small/train_32x32.tar -O /tmp/imagenet.tar
        tar -xf /tmp/imagenet.tar -C imagenet_samples --strip-components=1 || echo \"Using synthetic data\"
    fi

    # Run benchmark
    echo \"\"
    echo \"Running ResNet50 inference...\"
    python3 python/main.py \
        --model resnet50_v1.onnx \
        --backend onnxruntime \
        --scenario Offline \
        --max-batchsize 32 \
        --count 1000 \
        --time 60 \
        2>&1 | tee /results/resnet50_results.txt

    echo \"\"
    echo \"✓ Benchmark complete\"
    "

echo ""
echo "✓ Benchmark complete! Results saved to /mnt/data/mlperf-docker/resnet50_results.txt"
'

echo ""
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo "Step 2: Copying Results to Local Machine"
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo ""

TIMESTAMP=$(date +%Y%m%d_%H%M%S)
gcloud compute scp \
    $INSTANCE:/mnt/data/mlperf-docker/resnet50_results.txt \
    $RESULTS_DIR/resnet50_docker_${TIMESTAMP}.txt \
    --zone=$ZONE \
    --project=$PROJECT

echo ""
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo "Step 3: Showing Results"
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo ""

LATEST_RESULT="$RESULTS_DIR/resnet50_docker_${TIMESTAMP}.txt"
echo "Results saved to: $LATEST_RESULT"
echo ""
cat "$LATEST_RESULT"

echo ""
echo "╔═══════════════════════════════════════════════════════════════╗"
echo "║                    Benchmark Complete!                        ║"
echo "╚═══════════════════════════════════════════════════════════════╝"
echo ""
echo "What you just measured:"
echo "  - Throughput: samples/second (higher is better)"
echo "  - Latency: milliseconds (lower is better)"
echo ""
echo "Next steps:"
echo "  1. Run with GPU isolation: ./run-mlperf-with-proxy.sh"
echo "  2. Run with Xen hypervisor: ./run-mlperf-with-xen.sh"
echo "  3. Compare all results: ./analyze-results.sh"
echo ""
