#!/bin/bash
# Deploy GPU isolation system to GCP instance

set -e

GCP_INSTANCE="gpu-benchmarking"
GCP_ZONE="us-central1-a"
GCP_PROJECT="robotic-tide-459208-h4"

echo "=== Deploying to GCP ==="
echo "Instance: $GCP_INSTANCE"
echo "Zone: $GCP_ZONE"
echo "Project: $GCP_PROJECT"
echo ""

# Create tarball
echo "Creating tarball..."
tar czf gpu-isolation.tar.gz \
    --exclude='.git' \
    --exclude='*.o' \
    --exclude='*.so' \
    --exclude='*.so.*' \
    --exclude='gpu_proxy' \
    --exclude='test_client' \
    --exclude='test_app' \
    .

echo "Tarball created: $(du -h gpu-isolation.tar.gz | cut -f1)"
echo ""

# Upload to GCP
echo "Uploading to GCP instance..."
gcloud compute scp \
    --zone=$GCP_ZONE \
    --project=$GCP_PROJECT \
    gpu-isolation.tar.gz \
    $GCP_INSTANCE:~/ || {
    echo ""
    echo "ERROR: Failed to upload. Please check:"
    echo "1. Instance is running: gcloud compute instances list --project=$GCP_PROJECT"
    echo "2. You have SSH access: gcloud compute ssh $GCP_INSTANCE --zone=$GCP_ZONE --project=$GCP_PROJECT"
    exit 1
}

echo ""
echo "Upload complete!"
echo ""
echo "=== Next Steps ==="
echo ""
echo "1. SSH into the instance:"
echo "   gcloud compute ssh $GCP_INSTANCE --zone=$GCP_ZONE --project=$GCP_PROJECT"
echo ""
echo "2. Extract and setup:"
echo "   tar xzf gpu-isolation.tar.gz"
echo "   cd gpu-isolation"
echo ""
echo "3. Check GPU:"
echo "   nvidia-smi"
echo ""
echo "4. Install dependencies (if not already installed):"
echo "   sudo apt-get update"
echo "   sudo apt-get install -y build-essential nvidia-cuda-toolkit"
echo ""
echo "5. Build and test:"
echo "   cd gpu-proxy"
echo "   make clean && make"
echo "   ./gpu_proxy &"
echo ""
echo "   cd libvgpu"
echo "   make clean && make"
echo "   ./test_app"
echo ""
echo "See gpu-proxy/NEXT_STEPS.md for detailed instructions"
