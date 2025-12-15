#!/bin/bash
# Copy all benchmark results from GCP instance to local repo

set -e

INSTANCE="gpu-benchmarking"
ZONE="us-central1-a"
PROJECT="robotic-tide-459208-h4"
DEST="mlperf-benchmark/results"

echo "=== Copying Benchmark Results from GCP ==="
echo ""
echo "Instance: $INSTANCE"
echo "Destination: $DEST/"
echo ""

# Create results directory if it doesn't exist
mkdir -p "$DEST"

# List available results on GCP
echo "Checking what results exist on GCP..."
gcloud compute ssh $INSTANCE --zone=$ZONE --project=$PROJECT --command='
ls -lh /mnt/data/*-results.txt /mnt/data/*.txt 2>/dev/null | grep -E "results|benchmark" || echo "No results found yet"
'

echo ""
echo "Copying all available results..."

# Copy individual result files
FILES=(
    "baseline.txt"
    "k8s-baseline-results.txt"
    "with-gpu-proxy-results.txt"
    "k8s-with-proxy-results.txt"
    "k8s-with-hypervisor-results.txt"
)

for file in "${FILES[@]}"; do
    echo "Attempting to copy $file..."
    gcloud compute scp \
        "$INSTANCE:/mnt/data/$file" \
        "$DEST/" \
        --zone=$ZONE \
        --project=$PROJECT 2>/dev/null && echo "  ✓ Copied $file" || echo "  - $file not found (not created yet)"
done

echo ""
echo "=== Results Copied ==="
echo ""
echo "Local results directory:"
ls -lh "$DEST/"

echo ""
echo "View results:"
echo "  cat $DEST/baseline_results.txt"
echo "  cat $DEST/k8s-baseline-results.txt"
echo ""
echo "Compare results:"
echo "  ./analyze-results.sh"
