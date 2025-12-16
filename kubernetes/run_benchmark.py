import onnxruntime as ort
import numpy as np
import time
from PIL import Image
import glob

print('\nONNX Runtime Execution Providers:', ort.get_available_providers())

# Load model
print('\nLoading ResNet50 model...')
session = ort.InferenceSession('resnet50_v1.onnx', providers=['CUDAExecutionProvider'])

# Prepare input
images = glob.glob('images/*.jpg')[:100]
def preprocess(img_path):
    img = Image.open(img_path).resize((224, 224))
    img_array = np.array(img).astype(np.float32)
    img_array = np.transpose(img_array, (2, 0, 1))  # HWC to CHW
    img_array = np.expand_dims(img_array, axis=0)   # Add batch dimension
    img_array = (img_array / 255.0 - 0.5) / 0.5     # Normalize
    return img_array

print(f'\nLoaded {len(images)} images')

# Warmup
print('\nWarming up GPU...')
for _ in range(10):
    input_data = preprocess(images[0])
    session.run(None, {session.get_inputs()[0].name: input_data})

# Benchmark throughput
print('\n' + '='*60)
print('THROUGHPUT TEST (60 seconds)')
print('='*60)

start = time.time()
count = 0
latencies = []

while time.time() - start < 60:
    img = images[count % len(images)]
    input_data = preprocess(img)

    iter_start = time.time()
    session.run(None, {session.get_inputs()[0].name: input_data})
    latency = (time.time() - iter_start) * 1000  # ms
    latencies.append(latency)

    count += 1

elapsed = time.time() - start
throughput = count / elapsed

print(f'\nResults:')
print(f'  Total images processed: {count}')
print(f'  Duration: {elapsed:.1f} seconds')
print(f'  Throughput: {throughput:.1f} images/sec')
print(f'  Average latency: {np.mean(latencies):.2f} ms')

# Latency percentiles
latencies_sorted = sorted(latencies)
p50 = latencies_sorted[len(latencies)//2]
p90 = latencies_sorted[int(len(latencies)*0.9)]
p99 = latencies_sorted[int(len(latencies)*0.99)]

print(f'\nLatency Percentiles:')
print(f'  p50: {p50:.2f} ms')
print(f'  p90: {p90:.2f} ms')
print(f'  p99: {p99:.2f} ms')

print('\n' + '='*60)
print('BENCHMARK COMPLETE')
print('='*60)

# Save results
with open('/results/benchmark.txt', 'w') as f:
    f.write(f'MLPerf ResNet50 Benchmark - Kubernetes\n')
    f.write(f'Throughput: {throughput:.1f} images/sec\n')
    f.write(f'Average Latency: {np.mean(latencies):.2f} ms\n')
    f.write(f'p50 Latency: {p50:.2f} ms\n')
    f.write(f'p90 Latency: {p90:.2f} ms\n')
    f.write(f'p99 Latency: {p99:.2f} ms\n')

print('\nResults saved to /results/benchmark.txt')
