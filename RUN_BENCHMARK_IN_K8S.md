# Run GPU Benchmark in Kubernetes

This guide shows how to run the GPU benchmark in a Kubernetes pod on your GPU VM. This simulates the production environment where the hypervisor will run.

## Why Kubernetes?

Since you'll deploy the Xen GPU hypervisor as a Kubernetes DaemonSet later, we need to:
1. Test GPU access from within K8s pods
2. Measure baseline performance in K8s
3. Compare K8s vs bare metal overhead
4. Ensure the hypervisor can run in this environment

---

## Step 1: Install Kubernetes on GPU VM (10 minutes)

```bash
# From your local machine
chmod +x install-k8s-on-vm.sh
./install-k8s-on-vm.sh
```

This installs **K3s** (lightweight Kubernetes) on your GPU instance with:
- Single-node cluster
- NVIDIA device plugin for GPU access
- kubectl configured

---

## Step 2: Upload K8s Manifests

```bash
gcloud compute scp kubernetes/gpu-benchmark-pod.yaml \
    gpu-benchmarking:/mnt/data/ \
    --zone=us-central1-a \
    --project=robotic-tide-459208-h4
```

---

## Step 3: Deploy Benchmark Pod

SSH to your instance:
```bash
gcloud compute ssh gpu-benchmarking \
    --zone=us-central1-a \
    --project=robotic-tide-459208-h4
```

Deploy the pod:
```bash
cd /mnt/data
kubectl apply -f gpu-benchmark-pod.yaml
```

---

## Step 4: Watch It Run

```bash
# Check pod status
kubectl get pods -w

# View logs (real-time)
kubectl logs -f gpu-benchmark

# Once complete, view full results
kubectl logs gpu-benchmark
```

---

## Expected Output

```
=== GPU Benchmark in Kubernetes ===

Sun Dec 15 12:00:00 2024
+-----------------------------------------------------------------------------+
| NVIDIA-SMI 590.44.01    Driver Version: 590.44.01    CUDA Version: 13.1   |
|-------------------------------+----------------------+----------------------+
|   0  Tesla T4            Off  | 00000000:00:04.0 Off |                    0 |
+-------------------------------+----------------------+----------------------+

=== Running Baseline Benchmark ===

=== Simple GPU Benchmark ===

Found 1 CUDA device(s)
Using device: Tesla T4
Total memory: 14.56 GB

=== Memory Bandwidth Benchmark ===
Size: 100 MB
Host to Device: 4.31 GB/s
Device to Host: 4.53 GB/s

=== Throughput Benchmark (Alloc/Free) ===
Allocation size: 1024 KB
Iterations: 1000
Total time: 0.143 seconds
Throughput: 6977 ops/sec
Average latency: 0.14 ms

=== Latency Benchmark ===
Allocation size: 1024 KB
Samples: 500
p50 latency: 0.135 ms
p90 latency: 0.153 ms
p99 latency: 0.198 ms

=== Benchmark Complete ===
```

---

## Step 5: Save Results

```bash
# Save results locally
kubectl logs gpu-benchmark > /mnt/data/k8s-baseline-results.txt

# Compare with bare-metal results
echo "=== Bare Metal ==="
cat /mnt/data/baseline.txt | grep "Throughput:"

echo ""
echo "=== Kubernetes ==="
cat /mnt/data/k8s-baseline-results.txt | grep "Throughput:"
```

---

## Architecture

```
┌─────────────────────────────────────────┐
│   GPU VM (n1-standard-4 + Tesla T4)   │
│                                         │
│  ┌───────────────────────────────────┐ │
│  │   K3s (Kubernetes)                │ │
│  │                                   │ │
│  │  ┌─────────────────────────────┐ │ │
│  │  │  Pod: gpu-benchmark         │ │ │
│  │  │                             │ │ │
│  │  │  NVIDIA CUDA Runtime        │ │ │
│  │  │         ↓                   │ │ │
│  │  │  /dev/nvidia0 (passthrough) │ │ │
│  │  └─────────────────────────────┘ │ │
│  └───────────────────────────────────┘ │
│                 ↓                       │
│          NVIDIA Driver                  │
│                 ↓                       │
│           Tesla T4 GPU                  │
└─────────────────────────────────────────┘
```

Later, the hypervisor will sit between the pod and the GPU driver.

---

## Cleanup

```bash
# Delete the pod
kubectl delete pod gpu-benchmark

# If you want to remove K8s entirely
sudo /usr/local/bin/k3s-uninstall.sh
```

---

## Next Steps

1. **Baseline in K8s**: Measure GPU performance in K8s pod (this guide)
2. **Compare overhead**: K8s vs bare metal
3. **Deploy GPU proxy as DaemonSet**: Run GPU isolation layer in K8s
4. **Add Xen hypervisor**: Final integration with full isolation
5. **Re-benchmark**: Measure total overhead with hypervisor

---

## Troubleshooting

### Pod stuck in Pending
```bash
kubectl describe pod gpu-benchmark

# Check if GPU is available
kubectl get nodes -o json | grep nvidia.com/gpu
```

### GPU not detected
```bash
# Restart NVIDIA device plugin
kubectl delete pod -n kube-system -l name=nvidia-device-plugin-ds
```

### Can't access /mnt/data from pod
Make sure the hostPath volume mount is correct in the YAML.

---

## Why K3s Instead of Full Kubernetes?

- **Lightweight**: Perfect for single-node setups
- **GPU support**: Full NVIDIA device plugin compatibility
- **Fast setup**: 2 minutes vs 20 minutes for kubeadm
- **Production-ready**: Used by many edge/IoT GPU deployments
- **Easy cleanup**: Single command to uninstall

---

You're now running GPU benchmarks in the same environment where your hypervisor will operate!
