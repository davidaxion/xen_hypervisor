#!/bin/bash
# Setup git repository on GCP instance for remote development

set -e

INSTANCE="gpu-benchmarking"
ZONE="us-central1-a"
PROJECT="robotic-tide-459208-h4"
REPO="git@github.com:davidaxion/xen_hypervisor.git"

echo "=== Setting up Git on GCP Instance ==="
echo ""

# Check if SSH key exists on instance
echo "Step 1: Checking SSH configuration..."
gcloud compute ssh $INSTANCE --zone=$ZONE --project=$PROJECT --command='
if [ ! -f ~/.ssh/id_rsa ]; then
    echo "⚠ No SSH key found. Generating one..."
    ssh-keygen -t rsa -b 4096 -C "gpu-benchmarking@gcp" -f ~/.ssh/id_rsa -N ""
    echo ""
    echo "=== ADD THIS PUBLIC KEY TO GITHUB ==="
    echo ""
    cat ~/.ssh/id_rsa.pub
    echo ""
    echo "=== Go to: https://github.com/settings/keys ==="
    echo "=== Click \"New SSH key\" and paste the above key ==="
    echo ""
    read -p "Press Enter after adding the key to GitHub..."
else
    echo "✓ SSH key already exists"
fi
'

echo ""
echo "Step 2: Cloning repository to /mnt/data..."
gcloud compute ssh $INSTANCE --zone=$ZONE --project=$PROJECT --command='
cd /mnt/data

# Remove existing clone if it exists
if [ -d "xen_hypervisor" ]; then
    echo "Removing old clone..."
    rm -rf xen_hypervisor
fi

# Clone repository
echo "Cloning repository..."
git clone git@github.com:davidaxion/xen_hypervisor.git

cd xen_hypervisor

# Configure git
echo ""
echo "Step 3: Configuring git..."
git config user.name "David Engstler"
git config user.email "davidaxion@github.com"

echo "✓ Git configured"
echo ""
echo "Repository location: /mnt/data/xen_hypervisor"
echo ""
git status
'

echo ""
echo "=== Setup Complete ==="
echo ""
echo "To start developing on GCP:"
echo "  gcloud compute ssh $INSTANCE --zone=$ZONE --project=$PROJECT"
echo "  cd /mnt/data/xen_hypervisor"
echo ""
echo "To commit and push changes:"
echo "  git add ."
echo '  git commit -m "Your commit message"'
echo "  git push origin master"
