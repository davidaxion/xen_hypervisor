#!/bin/bash
# Complete benchmark workflow - Run everything and collect results

set -e

INSTANCE="gpu-benchmarking"
ZONE="us-central1-a"
PROJECT="robotic-tide-459208-h4"

echo "╔═══════════════════════════════════════════════════════════════╗"
echo "║     GPU Benchmark - Complete Automated Workflow              ║"
echo "╚═══════════════════════════════════════════════════════════════╝"
echo ""

# Step 1: Run bare metal benchmark
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo "Step 1: Running Bare Metal Benchmark"
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo ""

gcloud compute ssh $INSTANCE --zone=$ZONE --project=$PROJECT --command='
set -e
cd /mnt/data

echo "Compiling benchmark..."
gcc -o gpu_benchmark mlperf-benchmark/scripts/simple_gpu_benchmark.c \
    -I/usr/local/cuda/include \
    -L/usr/local/cuda/lib64 \
    -lcuda

echo ""
echo "Running baseline benchmark..."
./gpu_benchmark | tee baseline.txt

echo ""
echo "✓ Baseline benchmark complete"
'

echo ""
echo "✅ Step 1 Complete: Bare metal baseline"
echo ""
read -p "Press Enter to continue to Step 2 (Install Kubernetes)..."

# Step 2: Install Kubernetes
echo ""
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo "Step 2: Installing Kubernetes (K3s) on GPU VM"
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo ""

gcloud compute ssh $INSTANCE --zone=$ZONE --project=$PROJECT --command='
set -e

# Check if K3s is already installed
if command -v k3s &> /dev/null; then
    echo "K3s already installed, skipping..."
else
    echo "Installing K3s..."
    curl -sfL https://get.k3s.io | sh -s - \
        --write-kubeconfig-mode 644 \
        --disable traefik \
        --kubelet-arg="feature-gates=DevicePlugins=true"

    echo "Waiting for K3s to be ready..."
    sleep 10
    sudo k3s kubectl wait --for=condition=Ready node --all --timeout=60s

    echo "Configuring kubectl..."
    mkdir -p ~/.kube
    sudo cp /etc/rancher/k3s/k3s.yaml ~/.kube/config
    sudo chown $USER:$USER ~/.kube/config

    echo "Installing NVIDIA device plugin..."
    kubectl apply -f https://raw.githubusercontent.com/NVIDIA/k8s-device-plugin/v0.14.0/nvidia-device-plugin.yml

    echo "Waiting for GPU plugin..."
    sleep 15
fi

echo ""
echo "Kubernetes status:"
kubectl get nodes
echo ""
echo "✓ Kubernetes installed"
'

echo ""
echo "✅ Step 2 Complete: Kubernetes installed"
echo ""
read -p "Press Enter to continue to Step 3 (K8s benchmark)..."

# Step 3: Upload K8s manifest and run benchmark
echo ""
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo "Step 3: Running Benchmark in Kubernetes Pod"
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo ""

echo "Uploading K8s manifest..."
gcloud compute scp kubernetes/gpu-benchmark-pod.yaml \
    $INSTANCE:/mnt/data/ \
    --zone=$ZONE \
    --project=$PROJECT

gcloud compute ssh $INSTANCE --zone=$ZONE --project=$PROJECT --command='
set -e
cd /mnt/data

echo "Deploying benchmark pod..."
kubectl delete pod gpu-benchmark --ignore-not-found=true
kubectl apply -f gpu-benchmark-pod.yaml

echo ""
echo "Waiting for pod to start..."
kubectl wait --for=condition=Ready pod/gpu-benchmark --timeout=60s || true

echo ""
echo "Watching pod logs (this will take ~30 seconds)..."
sleep 5
kubectl logs -f gpu-benchmark 2>/dev/null || kubectl logs gpu-benchmark

echo ""
echo "Saving results..."
kubectl logs gpu-benchmark > k8s-baseline-results.txt

echo ""
echo "✓ K8s benchmark complete"
'

echo ""
echo "✅ Step 3 Complete: Kubernetes benchmark"
echo ""
read -p "Press Enter to continue to Step 4 (Copy results)..."

# Step 4: Copy all results back
echo ""
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo "Step 4: Copying Results to Local Machine"
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo ""

mkdir -p mlperf-benchmark/results

echo "Copying baseline results..."
gcloud compute scp \
    $INSTANCE:/mnt/data/baseline.txt \
    mlperf-benchmark/results/ \
    --zone=$ZONE \
    --project=$PROJECT

echo "Copying K8s results..."
gcloud compute scp \
    $INSTANCE:/mnt/data/k8s-baseline-results.txt \
    mlperf-benchmark/results/ \
    --zone=$ZONE \
    --project=$PROJECT

echo ""
echo "✅ Step 4 Complete: Results copied"

# Step 5: Analyze results
echo ""
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo "Step 5: Analyzing Results"
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo ""

./analyze-results.sh

echo ""
echo "╔═══════════════════════════════════════════════════════════════╗"
echo "║                  Benchmark Complete!                          ║"
echo "╚═══════════════════════════════════════════════════════════════╝"
echo ""
echo "Results saved in: mlperf-benchmark/results/"
echo ""
echo "What's next:"
echo "  1. Test with GPU proxy: Follow RUN_BENCHMARK_YOURSELF.md"
echo "  2. Deploy hypervisor: Coming soon"
echo "  3. Push results to GitHub: ./push-to-github.sh"
echo ""
