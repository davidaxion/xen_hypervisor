#!/bin/bash
# Setup 400GB disk on GCP instance

set -e

INSTANCE="gpu-benchmarking"
ZONE="us-central1-a"
PROJECT="robotic-tide-459208-h4"

echo "=== Setting up 400GB disk on $INSTANCE ==="
echo ""

# Run setup on remote instance
gcloud compute ssh $INSTANCE --zone=$ZONE --project=$PROJECT --command='
set -e

echo "Step 1: Finding the disk..."
lsblk

echo ""
echo "Step 2: Checking if disk is already mounted..."
if mount | grep -q "/mnt/data"; then
    echo "✓ Disk already mounted at /mnt/data"
    df -h /mnt/data
    exit 0
fi

echo "Step 3: Identifying the new disk..."
DISK=$(lsblk -ndo NAME,SIZE | grep "400G" | awk "{print \$1}" | head -1)

if [ -z "$DISK" ]; then
    echo "ERROR: Could not find 400GB disk"
    echo "Available disks:"
    lsblk
    exit 1
fi

DISK_PATH="/dev/$DISK"
echo "Found disk: $DISK_PATH"

echo ""
echo "Step 4: Checking if disk is formatted..."
if sudo file -s $DISK_PATH | grep -q "ext4"; then
    echo "✓ Disk already formatted with ext4"
else
    echo "Formatting disk with ext4..."
    sudo mkfs.ext4 -m 0 -E lazy_itable_init=0,lazy_journal_init=0,discard $DISK_PATH
    echo "✓ Disk formatted"
fi

echo ""
echo "Step 5: Creating mount point..."
sudo mkdir -p /mnt/data
echo "✓ Mount point created"

echo ""
echo "Step 6: Mounting disk..."
sudo mount -o discard,defaults $DISK_PATH /mnt/data
sudo chmod 777 /mnt/data
echo "✓ Disk mounted"

echo ""
echo "Step 7: Making mount permanent..."
UUID=$(sudo blkid $DISK_PATH | grep -oP "UUID=\"\K[^\"]+")
echo "Disk UUID: $UUID"

if grep -q "$UUID" /etc/fstab; then
    echo "✓ Already in fstab"
else
    echo "UUID=$UUID /mnt/data ext4 discard,defaults 0 2" | sudo tee -a /etc/fstab
    echo "✓ Added to fstab"
fi

echo ""
echo "Step 8: Creating workspace..."
mkdir -p /mnt/data/workspace
mkdir -p /mnt/data/tmp
echo "✓ Workspace created"

echo ""
echo "Step 9: Setting up environment..."
cat >> ~/.bashrc << "BASHRC_EOF"

# Use new disk for temp files
export TMPDIR=/mnt/data/tmp

# Workspace shortcut
alias workspace="cd /mnt/data/workspace"
BASHRC_EOF
echo "✓ Environment configured"

echo ""
echo "=== Setup Complete ==="
df -h /mnt/data
echo ""
echo "Workspace: /mnt/data/workspace"
echo "Temp dir: /mnt/data/tmp"
echo ""
echo "Use: cd /mnt/data/workspace"
'

echo ""
echo "=== Uploading code to new disk ==="

# Upload gpu-isolation code
echo "Uploading gpu-isolation..."
gcloud compute scp --recurse \
    gpu-isolation/ \
    $INSTANCE:/mnt/data/workspace/ \
    --zone=$ZONE \
    --project=$PROJECT

# Upload benchmark
echo "Uploading benchmark..."
gcloud compute scp \
    mlperf-benchmark/scripts/simple_gpu_benchmark.c \
    $INSTANCE:/mnt/data/workspace/ \
    --zone=$ZONE \
    --project=$PROJECT

echo ""
echo "=== All Done! ==="
echo ""
echo "To use the new disk:"
echo "  gcloud compute ssh $INSTANCE --zone=$ZONE --project=$PROJECT"
echo "  cd /mnt/data/workspace"
echo ""
echo "To run benchmark:"
echo "  cd /mnt/data/workspace/gpu-isolation"
echo "  gcc -o gpu_benchmark ../simple_gpu_benchmark.c -I/usr/local/cuda/include -L/usr/local/cuda/lib64 -lcuda"
echo "  ./gpu_benchmark"
