#!/bin/bash
# Deploy GPU Benchmark to GKE

set -e

PROJECT_ID="robotic-tide-459208-h4"
REGION="us-central1"
ZONE="us-central1-a"
CLUSTER_NAME="gpu-benchmark-cluster"
IMAGE_NAME="gcr.io/${PROJECT_ID}/gpu-benchmark:latest"

echo "=== GPU Benchmark Deployment ==="
echo "Project: $PROJECT_ID"
echo "Zone: $ZONE"
echo "Cluster: $CLUSTER_NAME"
echo ""

# Step 1: Create GKE cluster with GPU node pool
echo "Step 1: Creating GKE cluster..."
if gcloud container clusters describe $CLUSTER_NAME --zone=$ZONE --project=$PROJECT_ID &>/dev/null; then
    echo "✓ Cluster already exists"
else
    echo "Creating cluster with T4 GPU..."
    gcloud container clusters create $CLUSTER_NAME \
        --zone=$ZONE \
        --project=$PROJECT_ID \
        --machine-type=n1-standard-4 \
        --accelerator=type=nvidia-tesla-t4,count=1 \
        --num-nodes=1 \
        --enable-autoscaling \
        --min-nodes=0 \
        --max-nodes=2 \
        --disk-size=100GB \
        --scopes=https://www.googleapis.com/auth/cloud-platform \
        --enable-stackdriver-kubernetes \
        --addons=HorizontalPodAutoscaling,HttpLoadBalancing

    echo "✓ Cluster created"
fi

# Step 2: Get cluster credentials
echo ""
echo "Step 2: Getting cluster credentials..."
gcloud container clusters get-credentials $CLUSTER_NAME \
    --zone=$ZONE \
    --project=$PROJECT_ID

echo "✓ Credentials configured"

# Step 3: Install NVIDIA device plugin
echo ""
echo "Step 3: Installing NVIDIA device plugin..."
kubectl apply -f https://raw.githubusercontent.com/GoogleCloudPlatform/container-engine-accelerators/master/nvidia-driver-installer/cos/daemonset-preloaded.yaml

echo "Waiting for NVIDIA drivers to install..."
sleep 30

kubectl apply -f https://raw.githubusercontent.com/NVIDIA/k8s-device-plugin/v0.14.0/nvidia-device-plugin.yml

echo "✓ NVIDIA device plugin installed"

# Step 4: Build and push Docker image
echo ""
echo "Step 4: Building Docker image..."
cd "$(dirname "$0")"

docker build -t $IMAGE_NAME -f docker/Dockerfile .

echo "✓ Image built"

echo "Pushing to GCR..."
docker push $IMAGE_NAME

echo "✓ Image pushed to GCR"

# Step 5: Deploy benchmark job
echo ""
echo "Step 5: Deploying benchmark job..."

# Clean up old jobs
kubectl delete job gpu-benchmark-baseline --ignore-not-found=true

# Deploy new job
kubectl apply -f kubernetes/benchmark-job.yaml

echo "✓ Job deployed"

# Step 6: Wait for completion and collect results
echo ""
echo "Step 6: Waiting for benchmark to complete..."
echo "(This will take ~5-10 minutes)"
echo ""

# Wait for job to complete
kubectl wait --for=condition=complete --timeout=20m job/gpu-benchmark-baseline

echo ""
echo "✓ Benchmark complete!"

# Get pod name
POD_NAME=$(kubectl get pods --selector=job-name=gpu-benchmark-baseline -o jsonpath='{.items[0].metadata.name}')

echo ""
echo "=== Benchmark Logs ==="
kubectl logs $POD_NAME

# Copy results
echo ""
echo "Copying results..."
mkdir -p results/baseline
kubectl cp ${POD_NAME}:/results/benchmark_results.json results/baseline/benchmark_results.json

echo "✓ Results saved to: results/baseline/benchmark_results.json"

# Display summary
echo ""
echo "=== Results Summary ==="
python3 -c "
import json
with open('results/baseline/benchmark_results.json') as f:
    data = json.load(f)
    print(f\"GPU: {data['gpu_info']['name']}\")
    print(f\"CUDA: {data['gpu_info']['cuda_version']}\")
    print()
    print('Benchmark Results:')
    for bench in data['benchmarks']:
        scenario = bench['scenario']
        if scenario == 'offline':
            print(f\"  Offline (batch={bench['batch_size']}): {bench['throughput_samples_per_sec']:,.0f} samples/sec\")
        elif scenario == 'server':
            print(f\"  Server (batch={bench['batch_size']}): p99={bench['latency_p99_ms']:.2f}ms\")
        elif scenario == 'single_stream':
            print(f\"  Single-stream: p90={bench['latency_p90_ms']:.2f}ms\")
"

echo ""
echo "=== Deployment Complete ==="
echo ""
echo "To view logs again:"
echo "  kubectl logs $POD_NAME"
echo ""
echo "To scale down cluster (save costs):"
echo "  gcloud container clusters resize $CLUSTER_NAME --num-nodes=0 --zone=$ZONE --project=$PROJECT_ID"
echo ""
echo "To delete cluster:"
echo "  gcloud container clusters delete $CLUSTER_NAME --zone=$ZONE --project=$PROJECT_ID"
