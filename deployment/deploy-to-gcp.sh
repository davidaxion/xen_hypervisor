#!/bin/bash
# Deploy GPU Proxy Components to GCP Instance
# This script copies built binaries and installs them on the remote instance

set -e

PROJECT_ROOT="/Users/davidengstler/Projects/Hack_the_planet/GPU_Hypervisor_Xen"
INSTANCE_NAME="gpu-k8s-benchmark"
ZONE="us-central1-a"
PROJECT="robotic-tide-459208-h4"

echo "╔═══════════════════════════════════════════════════════════╗"
echo "║     Deploying GPU Proxy to GCP Instance                  ║"
echo "╚═══════════════════════════════════════════════════════════╝"
echo ""

# Check if binaries are built
echo "=== Checking built artifacts ==="
if [ ! -f "$PROJECT_ROOT/gpu-proxy/gpu_proxy_stub" ]; then
    echo "ERROR: gpu_proxy_stub not found"
    echo "Run: ./deployment/build-with-xen.sh"
    exit 1
fi

if [ ! -f "$PROJECT_ROOT/gpu-proxy/libvgpu/libcuda.so.1" ]; then
    echo "ERROR: libvgpu/libcuda.so.1 not found"
    echo "Run: ./deployment/build-with-xen.sh"
    exit 1
fi

echo "Artifacts found"
echo ""

# Copy installation script
echo "=== Step 1: Copying Xen installation script ==="
gcloud compute scp \
    "$PROJECT_ROOT/deployment/install-xen.sh" \
    "$INSTANCE_NAME:/tmp/install-xen.sh" \
    --zone="$ZONE" \
    --project="$PROJECT" \
    --quiet

echo "Installation script copied"
echo ""

# Copy GPU proxy binaries
echo "=== Step 2: Copying GPU proxy binaries ==="
gcloud compute ssh "$INSTANCE_NAME" \
    --zone="$ZONE" \
    --project="$PROJECT" \
    --command="sudo mkdir -p /opt/gpu-proxy /opt/gpu-proxy/lib"

# Copy stub proxy
gcloud compute scp \
    "$PROJECT_ROOT/gpu-proxy/gpu_proxy_stub" \
    "$INSTANCE_NAME:/tmp/gpu_proxy" \
    --zone="$ZONE" \
    --project="$PROJECT" \
    --quiet

# Copy libvgpu
gcloud compute scp \
    "$PROJECT_ROOT/gpu-proxy/libvgpu/libcuda.so.1" \
    "$INSTANCE_NAME:/tmp/libcuda.so.1" \
    --zone="$ZONE" \
    --project="$PROJECT" \
    --quiet

# Copy real proxy if available
if [ -f "$PROJECT_ROOT/gpu-proxy/gpu_proxy" ]; then
    echo "Copying real GPU proxy..."
    gcloud compute scp \
        "$PROJECT_ROOT/gpu-proxy/gpu_proxy" \
        "$INSTANCE_NAME:/tmp/gpu_proxy_real" \
        --zone="$ZONE" \
        --project="$PROJECT" \
        --quiet
fi

# Move to final location
gcloud compute ssh "$INSTANCE_NAME" \
    --zone="$ZONE" \
    --project="$PROJECT" \
    --command="
        sudo mv /tmp/gpu_proxy /opt/gpu-proxy/gpu_proxy &&
        sudo chmod +x /opt/gpu-proxy/gpu_proxy &&
        sudo mv /tmp/libcuda.so.1 /opt/gpu-proxy/lib/ &&
        sudo chmod 755 /opt/gpu-proxy/lib/libcuda.so.1
    "

if [ -f "$PROJECT_ROOT/gpu-proxy/gpu_proxy" ]; then
    gcloud compute ssh "$INSTANCE_NAME" \
        --zone="$ZONE" \
        --project="$PROJECT" \
        --command="
            sudo mv /tmp/gpu_proxy_real /opt/gpu-proxy/gpu_proxy_real &&
            sudo chmod +x /opt/gpu-proxy/gpu_proxy_real
        "
fi

echo "Binaries deployed"
echo ""

# Copy source code for rebuild
echo "=== Step 3: Copying source code ==="
gcloud compute ssh "$INSTANCE_NAME" \
    --zone="$ZONE" \
    --project="$PROJECT" \
    --command="mkdir -p /mnt/data/gpu-hypervisor"

# Tar and copy source
cd "$PROJECT_ROOT"
tar czf /tmp/gpu-hypervisor-source.tar.gz \
    idm-protocol/ \
    gpu-proxy/ \
    deployment/ \
    --exclude='*.o' \
    --exclude='*.so*' \
    --exclude='gpu_proxy*' \
    --exclude='test_*'

gcloud compute scp \
    /tmp/gpu-hypervisor-source.tar.gz \
    "$INSTANCE_NAME:/mnt/data/gpu-hypervisor/" \
    --zone="$ZONE" \
    --project="$PROJECT" \
    --quiet

gcloud compute ssh "$INSTANCE_NAME" \
    --zone="$ZONE" \
    --project="$PROJECT" \
    --command="
        cd /mnt/data/gpu-hypervisor &&
        tar xzf gpu-hypervisor-source.tar.gz
    "

rm /tmp/gpu-hypervisor-source.tar.gz

echo "Source code deployed"
echo ""

echo "╔═══════════════════════════════════════════════════════════╗"
echo "║     Deployment Complete!                                  ║"
echo "╚═══════════════════════════════════════════════════════════╝"
echo ""
echo "Deployed to: $INSTANCE_NAME"
echo "  - Binaries: /opt/gpu-proxy/"
echo "  - Source: /mnt/data/gpu-hypervisor/"
echo ""
echo "Next steps:"
echo "  1. Install Xen:"
echo "     gcloud compute ssh $INSTANCE_NAME --zone=$ZONE --project=$PROJECT"
echo "     sudo bash /tmp/install-xen.sh"
echo "     sudo reboot"
echo ""
echo "  2. After reboot, verify Xen:"
echo "     gcloud compute ssh $INSTANCE_NAME --zone=$ZONE --project=$PROJECT"
echo "     sudo xl info"
echo ""
echo "  3. Enable GPU proxy service:"
echo "     sudo systemctl enable --now gpu-proxy"
echo ""
