# Next Steps - MLPerf Benchmark Setup

## Current Situation

✅ **Code is ready and pushed to GitHub**
- Official MLPerf Inference setup complete
- Docker-based runner script created
- All documentation written

❌ **Current GCP instance has broken packages**
- apt has unmet dependencies from failed CUDA install
- Cannot install new packages (Docker, git, etc.)
- Root disk is 84% full

## Solution: Fresh GCP Instance

The easiest path forward is to create a **fresh GCP instance** with proper setup from the start.

### Option 1: Create New Instance (Recommended)

```bash
# 1. Create new instance with larger boot disk and attached 400GB disk
gcloud compute instances create gpu-benchmarking-v2 \
    --zone=us-central1-a \
    --project=robotic-tide-459208-h4 \
    --machine-type=n1-standard-4 \
    --accelerator=type=nvidia-tesla-t4,count=1 \
    --boot-disk-size=50GB \
    --maintenance-policy=TERMINATE \
    --image-family=debian-12 \
    --image-project=debian-cloud \
    --disk=name=disk-20251215-123147,mode=rw

# 2. Install NVIDIA drivers
gcloud compute ssh gpu-benchmarking-v2 --zone=us-central1-a --command='
    curl https://raw.githubusercontent.com/GoogleCloudPlatform/compute-gpu-installation/main/linux/install_gpu_driver.py --output install_gpu_driver.py
    sudo python3 install_gpu_driver.py
'

# 3. Install Docker
gcloud compute ssh gpu-benchmarking-v2 --zone=us-central1-a --command='
    curl -fsSL https://get.docker.com -o get-docker.sh
    sudo sh get-docker.sh
    sudo usermod -aG docker $USER
'

# 4. Install NVIDIA Container Toolkit
gcloud compute ssh gpu-benchmarking-v2 --zone=us-central1-a --command='
    distribution=$(. /etc/os-release;echo $ID$VERSION_ID)
    curl -s -L https://nvidia.github.io/nvidia-docker/gpgkey | sudo apt-key add -
    curl -s -L https://nvidia.github.io/nvidia-docker/$distribution/nvidia-docker.list | sudo tee /etc/apt/sources.list.d/nvidia-docker.list
    sudo apt-get update && sudo apt-get install -y nvidia-docker2
    sudo systemctl restart docker
'

# 5. Test GPU in Docker
gcloud compute ssh gpu-benchmarking-v2 --zone=us-central1-a --command='
    docker run --rm --gpus all nvidia/cuda:13.1.0-base-ubuntu22.04 nvidia-smi
'

# 6. Run MLPerf benchmark
./run-mlperf-docker.sh
```

### Option 2: Fix Current Instance (Not Recommended)

```bash
# SSH to current instance
gcloud compute ssh gpu-benchmarking --zone=us-central1-a

# Try to fix apt
sudo apt --fix-broken install -y
sudo apt-get clean
sudo apt-get autoremove -y

# If that doesn't work, remove CUDA
sudo apt-get remove --purge cuda-* -y
sudo apt-get autoremove -y
sudo apt-get update

# Then install Docker
curl -fsSL https://get.docker.com | sudo sh
```

**Problem**: This may still fail due to dependency conflicts.

## What The Benchmark Will Show

Once running, MLPerf ResNet50 will output:

```
================================================
MLPerf Results Summary
================================================
Benchmark: resnet50
Scenario: Offline
Samples per second: 3456.78
================================================
Early stopping 90th percentile estimate: 8.23 ms
Early stopping 99th percentile estimate: 14.56 ms
================================================
```

### What This Means:

- **3,456 samples/sec** = GPU processed 3,456 images per second
- **p90: 8.23ms** = 90% of images processed in under 8.23ms
- **p99: 14.56ms** = 99% of images processed in under 14.56ms

### Baseline Expectations (Tesla T4):

| Metric | Expected Value |
|--------|---------------|
| Throughput | 3,000-4,000 samples/sec |
| p90 Latency | 5-10 ms |
| p99 Latency | 10-20 ms |

## Full Workflow After Fresh Instance

```
1. Fresh GCP Instance
   ↓ (Install NVIDIA drivers, Docker, NVIDIA Container Toolkit)

2. Run Bare Metal Baseline
   ↓ ./run-mlperf-docker.sh
   ↓ Save results: ~3,500 samples/sec

3. Run with GPU Isolation
   ↓ Deploy GPU proxy
   ↓ Run benchmark through proxy
   ↓ Measure overhead: should be <5%

4. Run with Xen Hypervisor
   ↓ Deploy Xen on Kubernetes
   ↓ Run benchmark in VM
   ↓ Measure total overhead: goal <10%

5. Compare All Results
   ↓ ./analyze-results.sh
   ↓ Prove GPU isolation works!
```

## Files Ready to Use

All these are in the repo and ready:

- `run-mlperf-docker.sh` - Main benchmark runner (uses Docker)
- `setup-official-mlperf.sh` - Alternative Python-based setup
- `MLPERF_EXPLAINED.md` - Complete explanation of how it works
- `RUN_OFFICIAL_MLPERF.md` - Technical guide
- `mlperf-benchmark/README.md` - Quick reference

## Repository

Everything is pushed to: `git@github.com:davidaxion/xen_hypervisor.git`

Clone on new instance:
```bash
cd /mnt/data
git clone git@github.com:davidaxion/xen_hypervisor.git
cd xen_hypervisor
./run-mlperf-docker.sh
```

## Summary

**Current Status**: Code ✅ | Instance ❌

**Recommendation**: Create fresh instance with 50GB boot disk

**Time to working benchmark**: ~30 minutes on fresh instance

**What you'll get**: Industry-standard GPU performance metrics to prove your hypervisor works

---

Let me know if you want me to:
1. Create the new instance for you
2. Help fix the current one
3. Explain anything else about the benchmark
