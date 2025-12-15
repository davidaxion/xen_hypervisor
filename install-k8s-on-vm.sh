#!/bin/bash
# Install Kubernetes on the GPU VM (single-node cluster)
# This allows testing GPU isolation in K8s where hypervisor will run

set -e

INSTANCE="gpu-benchmarking"
ZONE="us-central1-a"
PROJECT="robotic-tide-459208-h4"

echo "=== Installing Kubernetes on GPU VM ==="
echo ""
echo "This installs a single-node K8s cluster on your GPU instance."
echo "Later, you'll deploy the hypervisor as a K8s daemonset."
echo ""

gcloud compute ssh $INSTANCE --zone=$ZONE --project=$PROJECT --command='
set -e

echo "Step 1: Installing K3s (lightweight Kubernetes)..."
echo ""
echo "K3s is perfect for single-node setups and supports GPU passthrough."
echo ""

# Install K3s with GPU support
curl -sfL https://get.k3s.io | sh -s - \
    --write-kubeconfig-mode 644 \
    --disable traefik \
    --kubelet-arg="feature-gates=DevicePlugins=true"

echo ""
echo "✓ K3s installed"

# Wait for K3s to be ready
echo ""
echo "Step 2: Waiting for Kubernetes to be ready..."
sudo k3s kubectl wait --for=condition=Ready node --all --timeout=60s

echo ""
echo "✓ Kubernetes is ready"

# Configure kubectl
echo ""
echo "Step 3: Configuring kubectl..."
mkdir -p ~/.kube
sudo cp /etc/rancher/k3s/k3s.yaml ~/.kube/config
sudo chown $USER:$USER ~/.kube/config
export KUBECONFIG=~/.kube/config

echo ""
echo "✓ kubectl configured"

# Install NVIDIA device plugin for Kubernetes
echo ""
echo "Step 4: Installing NVIDIA GPU device plugin..."
kubectl apply -f https://raw.githubusercontent.com/NVIDIA/k8s-device-plugin/v0.14.0/nvidia-device-plugin.yml

echo ""
echo "Waiting for GPU plugin to be ready..."
sleep 10

echo ""
echo "Step 5: Verifying GPU is available to Kubernetes..."
kubectl get nodes -o json | grep -i nvidia || echo "GPU not yet visible (may take a minute)"

echo ""
echo "=== Installation Complete ==="
echo ""
echo "Check GPU availability:"
echo "  kubectl get nodes -o json | grep nvidia.com/gpu"
echo ""
echo "List nodes:"
echo "  kubectl get nodes"
echo ""
echo "Next: Deploy GPU benchmark pod"
'

echo ""
echo "=== K8s Installation Complete ==="
echo ""
echo "To access the cluster:"
echo "  gcloud compute ssh $INSTANCE --zone=$ZONE --project=$PROJECT"
echo "  kubectl get nodes"
echo ""
echo "Next steps:"
echo "  1. Deploy benchmark as K8s pod"
echo "  2. Run baseline GPU tests in K8s"
echo "  3. Later: Deploy hypervisor as DaemonSet"
