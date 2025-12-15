# GPU Benchmark Container
FROM nvidia/cuda:12.0.0-base-ubuntu22.04

# Install Python and dependencies
RUN apt-get update && apt-get install -y \
    python3 \
    python3-pip \
    wget \
    && rm -rf /var/lib/apt/lists/*

# Install PyTorch with CUDA support
RUN pip3 install --no-cache-dir \
    torch==2.0.1 \
    torchvision==0.15.2 \
    numpy \
    && rm -rf /root/.cache/pip

# Copy benchmark script
COPY scripts/gpu_benchmark.py /app/gpu_benchmark.py
RUN chmod +x /app/gpu_benchmark.py

# Create results directory
RUN mkdir -p /results

# Set working directory
WORKDIR /app

# Environment variables
ENV PYTHONUNBUFFERED=1
ENV RESULTS_FILE=/results/benchmark_results.json

# Run benchmark
CMD ["python3", "/app/gpu_benchmark.py"]
