# GPU Hypervisor Deployment Scripts

This directory contains scripts for deploying the full CRI integration with Xen hypervisor.

## Architecture Overview

```
┌─────────────────────────────────────────────────────────────┐
│                    Kubernetes Cluster                        │
│                                                              │
│  ┌────────────────────────────────────────────────────┐    │
│  │ GPU Worker Node (GCP with T4 GPU)                 │    │
│  │                                                     │    │
│  │  ┌──────────────────────────────────────────────┐ │    │
│  │  │ Xen Hypervisor (Dom0)                        │ │    │
│  │  │                                               │ │    │
│  │  │  ├─ kubelet                                   │ │    │
│  │  │  ├─ vgpu-runtime (CRI)                        │ │    │
│  │  │  ├─ gpu-proxy daemon                          │ │    │
│  │  │  └─ NVIDIA GPU (PCI passthrough)              │ │    │
│  │  │                                               │ │    │
│  │  │  User Domains (DomU) - One per Pod:          │ │    │
│  │  │  ┌────────────┐  ┌────────────┐              │ │    │
│  │  │  │ Pod 1      │  │ Pod 2      │              │ │    │
│  │  │  │ - libvgpu  │  │ - libvgpu  │              │ │    │
│  │  │  │ - App      │  │ - App      │              │ │    │
│  │  │  └────────────┘  └────────────┘              │ │    │
│  │  │       ↕ IDM          ↕ IDM                    │ │    │
│  │  │  [Xen Grant Tables - Shared Memory]          │ │    │
│  │  └──────────────────────────────────────────────┘ │    │
│  └────────────────────────────────────────────────────┘    │
└─────────────────────────────────────────────────────────────┘
```

## Phase 1: Xen Setup and Testing (Week 1)

### Scripts

1. **install-xen.sh** - Install Xen hypervisor on GCP instance
   - Installs Xen packages
   - Configures GRUB for Xen boot
   - Enables IOMMU for GPU PCI passthrough
   - Creates GPU proxy systemd service
   - Sets up network bridge

2. **build-with-xen.sh** - Compile with Xen support
   - Builds IDM protocol with `-DUSE_XEN` flag
   - Compiles GPU proxy with Xen grant tables
   - Links against xenctrl and xenstore libraries
   - Produces binaries for deployment

3. **deploy-to-gcp.sh** - Deploy to GCP instance
   - Copies built binaries to `/opt/gpu-proxy/`
   - Deploys source code to `/mnt/data/gpu-hypervisor/`
   - Sets up file permissions

4. **test-xen-integration.sh** - Validate Xen setup
   - Verifies Xen hypervisor is running
   - Tests GPU accessibility in Dom0
   - Attempts test domain creation
   - Validates GPU proxy can start

### Usage

#### Step 1: Build locally with Xen support
```bash
cd /Users/davidengstler/Projects/Hack_the_planet/GPU_Hypervisor_Xen
chmod +x deployment/*.sh
./deployment/build-with-xen.sh
```

#### Step 2: Deploy to GCP instance
```bash
./deployment/deploy-to-gcp.sh
```

#### Step 3: Install Xen on GCP instance
```bash
gcloud compute ssh gpu-k8s-benchmark \
  --zone=us-central1-a \
  --project=robotic-tide-459208-h4

sudo bash /tmp/install-xen.sh
sudo reboot
```

#### Step 4: After reboot, verify installation
```bash
gcloud compute ssh gpu-k8s-benchmark \
  --zone=us-central1-a \
  --project=robotic-tide-459208-h4

# Verify Xen is running
sudo xl info

# Check GPU
nvidia-smi

# Run integration test
cd /mnt/data/gpu-hypervisor
sudo bash deployment/test-xen-integration.sh
```

## Phase 2: Minimal Kernel (Week 1-2)

Scripts to be created:
- `build-minimal-kernel.sh` - Build tiny Xen PVH kernel (~50MB)
- `build-rootfs.sh` - Create minimal rootfs with libvgpu
- `test-minimal-boot.sh` - Test kernel boots in <2 seconds

## Phase 3: CRI Runtime (Week 2-3)

Go implementation of Kubernetes CRI:
- Located in: `/cri-runtime/`
- Implements RunPodSandbox (creates Xen domain)
- Implements StopPodSandbox (destroys domain)
- Manages container lifecycle inside domains

## Phase 4: Kubernetes Integration (Week 3-4)

- RuntimeClass configuration
- Pod deployment with `runtimeClassName: gpu-isolated`
- Performance benchmarking vs baseline
- Security validation

## Current Status

### Completed
- ✅ Xen installation script
- ✅ Xen build script
- ✅ Deployment automation
- ✅ Integration test script

### In Progress
- 🔄 Testing on real Xen (pending GCP deployment)

### Pending
- ⏳ Minimal kernel builder
- ⏳ CRI runtime implementation
- ⏳ Kubernetes integration

## Success Criteria

Phase 1 complete when:
1. Xen hypervisor boots successfully on GCP instance
2. GPU accessible in Dom0
3. GPU proxy daemon starts in Dom0
4. Test domain can be created (after minimal kernel)
5. IDM messages flow over Xen grant tables

## Troubleshooting

### Xen fails to boot
- Check GRUB configuration: `cat /boot/grub/grub.cfg | grep xen`
- Verify IOMMU: `dmesg | grep -i iommu`
- Check kernel parameters: `cat /proc/cmdline`

### GPU not accessible
- Verify PCI device: `lspci | grep NVIDIA`
- Check driver: `nvidia-smi`
- Verify Dom0 has access: `ls /dev/nvidia*`

### Grant table errors
- Check Xen version: `xl info | grep xen_version`
- Verify libraries: `ldconfig -p | grep xen`
- Check permissions: `ls -l /dev/xen/*`

## References

- [Xen Project Documentation](https://wiki.xenproject.org/)
- [Xen PCI Passthrough Guide](https://wiki.xenproject.org/wiki/Xen_PCI_Passthrough)
- [Kubernetes CRI Spec](https://github.com/kubernetes/cri-api)
- [DEPLOYMENT.md](../gpu-proxy/docs/DEPLOYMENT.md) - Full architecture documentation
