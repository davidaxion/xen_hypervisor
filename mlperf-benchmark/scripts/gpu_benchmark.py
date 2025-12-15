#!/usr/bin/env python3
"""
GPU Benchmark for Kubernetes
Measures throughput, latency, and GPU utilization
"""

import torch
import torch.nn as nn
import time
import json
import os
from datetime import datetime
import numpy as np

class ResNet50Benchmark:
    """Simplified ResNet-50 inference benchmark"""

    def __init__(self, batch_size=32):
        self.batch_size = batch_size
        self.device = torch.device('cuda' if torch.cuda.is_available() else 'cpu')

        # Use torchvision's ResNet-50
        self.model = torch.hub.load('pytorch/vision:v0.10.0', 'resnet50', pretrained=True)
        self.model = self.model.to(self.device)
        self.model.eval()

        # Dummy input (ImageNet size)
        self.input_size = (self.batch_size, 3, 224, 224)

    def warmup(self, iterations=10):
        """Warm up GPU"""
        print(f"Warming up GPU ({iterations} iterations)...")
        dummy_input = torch.randn(self.input_size, device=self.device)

        with torch.no_grad():
            for _ in range(iterations):
                _ = self.model(dummy_input)

        torch.cuda.synchronize()
        print("Warmup complete")

    def benchmark_throughput(self, duration_sec=30):
        """Offline scenario: Maximum throughput"""
        print(f"\n=== THROUGHPUT BENCHMARK (Offline Scenario) ===")
        print(f"Duration: {duration_sec} seconds")
        print(f"Batch size: {self.batch_size}")

        dummy_input = torch.randn(self.input_size, device=self.device)

        iterations = 0
        start_time = time.time()

        with torch.no_grad():
            while time.time() - start_time < duration_sec:
                _ = self.model(dummy_input)
                iterations += 1

        torch.cuda.synchronize()
        elapsed = time.time() - start_time

        total_samples = iterations * self.batch_size
        throughput = total_samples / elapsed

        results = {
            'scenario': 'offline',
            'iterations': iterations,
            'total_samples': total_samples,
            'duration_sec': elapsed,
            'throughput_samples_per_sec': throughput,
            'throughput_images_per_sec': throughput,
            'batch_size': self.batch_size
        }

        print(f"Iterations: {iterations}")
        print(f"Total samples: {total_samples:,}")
        print(f"Throughput: {throughput:,.0f} samples/sec")

        return results

    def benchmark_latency(self, num_samples=1000):
        """Server scenario: Per-request latency"""
        print(f"\n=== LATENCY BENCHMARK (Server Scenario) ===")
        print(f"Samples: {num_samples}")
        print(f"Batch size: {self.batch_size}")

        dummy_input = torch.randn(self.input_size, device=self.device)
        latencies = []

        with torch.no_grad():
            for _ in range(num_samples):
                start = time.time()
                _ = self.model(dummy_input)
                torch.cuda.synchronize()
                latency = (time.time() - start) * 1000  # Convert to ms
                latencies.append(latency)

        latencies = np.array(latencies)

        results = {
            'scenario': 'server',
            'num_samples': num_samples,
            'batch_size': self.batch_size,
            'latency_mean_ms': float(np.mean(latencies)),
            'latency_median_ms': float(np.median(latencies)),
            'latency_p90_ms': float(np.percentile(latencies, 90)),
            'latency_p95_ms': float(np.percentile(latencies, 95)),
            'latency_p99_ms': float(np.percentile(latencies, 99)),
            'latency_min_ms': float(np.min(latencies)),
            'latency_max_ms': float(np.max(latencies))
        }

        print(f"Mean latency: {results['latency_mean_ms']:.2f} ms")
        print(f"Median latency: {results['latency_median_ms']:.2f} ms")
        print(f"P90 latency: {results['latency_p90_ms']:.2f} ms")
        print(f"P99 latency: {results['latency_p99_ms']:.2f} ms")

        return results

    def benchmark_single_stream(self, num_samples=500):
        """Edge scenario: Single sample at a time"""
        print(f"\n=== SINGLE-STREAM BENCHMARK (Edge Scenario) ===")
        print(f"Samples: {num_samples}")
        print("Batch size: 1")

        dummy_input = torch.randn((1, 3, 224, 224), device=self.device)
        latencies = []

        with torch.no_grad():
            for _ in range(num_samples):
                start = time.time()
                _ = self.model(dummy_input)
                torch.cuda.synchronize()
                latency = (time.time() - start) * 1000  # Convert to ms
                latencies.append(latency)

        latencies = np.array(latencies)

        results = {
            'scenario': 'single_stream',
            'num_samples': num_samples,
            'batch_size': 1,
            'latency_mean_ms': float(np.mean(latencies)),
            'latency_p90_ms': float(np.percentile(latencies, 90)),
            'latency_p99_ms': float(np.percentile(latencies, 99))
        }

        print(f"Mean latency: {results['latency_mean_ms']:.2f} ms")
        print(f"P90 latency: {results['latency_p90_ms']:.2f} ms")

        return results

def get_gpu_info():
    """Get GPU information"""
    if not torch.cuda.is_available():
        return {'available': False}

    return {
        'available': True,
        'name': torch.cuda.get_device_name(0),
        'compute_capability': '.'.join(str(x) for x in torch.cuda.get_device_capability(0)),
        'total_memory_gb': torch.cuda.get_device_properties(0).total_memory / 1e9,
        'cuda_version': torch.version.cuda,
        'pytorch_version': torch.__version__
    }

def main():
    print("=" * 80)
    print("GPU BENCHMARK - ResNet-50 Inference")
    print("=" * 80)

    # GPU Info
    gpu_info = get_gpu_info()
    print(f"\nGPU: {gpu_info.get('name', 'N/A')}")
    print(f"CUDA: {gpu_info.get('cuda_version', 'N/A')}")
    print(f"PyTorch: {gpu_info.get('pytorch_version', 'N/A')}")
    print(f"Memory: {gpu_info.get('total_memory_gb', 0):.1f} GB")

    if not gpu_info['available']:
        print("ERROR: No GPU available!")
        return

    # Run benchmarks
    batch_sizes = [1, 8, 32]
    all_results = {
        'timestamp': datetime.now().isoformat(),
        'gpu_info': gpu_info,
        'benchmarks': []
    }

    for batch_size in batch_sizes:
        print(f"\n{'=' * 80}")
        print(f"BATCH SIZE: {batch_size}")
        print(f"{'=' * 80}")

        benchmark = ResNet50Benchmark(batch_size=batch_size)
        benchmark.warmup()

        # Throughput (Offline)
        throughput_results = benchmark.benchmark_throughput(duration_sec=30)
        all_results['benchmarks'].append(throughput_results)

        # Latency (Server)
        latency_results = benchmark.benchmark_latency(num_samples=1000)
        all_results['benchmarks'].append(latency_results)

    # Single stream (batch_size=1)
    print(f"\n{'=' * 80}")
    benchmark = ResNet50Benchmark(batch_size=1)
    benchmark.warmup(iterations=5)
    single_stream_results = benchmark.benchmark_single_stream(num_samples=500)
    all_results['benchmarks'].append(single_stream_results)

    # Save results
    output_file = os.getenv('RESULTS_FILE', '/results/benchmark_results.json')
    os.makedirs(os.path.dirname(output_file), exist_ok=True)

    with open(output_file, 'w') as f:
        json.dump(all_results, f, indent=2)

    print(f"\n{'=' * 80}")
    print(f"Results saved to: {output_file}")
    print(f"{'=' * 80}")

    # Print summary
    print("\n=== SUMMARY ===\n")
    for result in all_results['benchmarks']:
        scenario = result['scenario']
        batch_size = result.get('batch_size', 'N/A')

        if scenario == 'offline':
            throughput = result['throughput_samples_per_sec']
            print(f"{scenario.upper()} (batch={batch_size}): {throughput:,.0f} samples/sec")
        elif scenario == 'server':
            p99 = result['latency_p99_ms']
            print(f"{scenario.upper()} (batch={batch_size}): p99={p99:.2f}ms")
        elif scenario == 'single_stream':
            p90 = result['latency_p90_ms']
            print(f"{scenario.upper()}: p90={p90:.2f}ms")

if __name__ == '__main__':
    main()
