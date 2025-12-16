import numpy as np
from PIL import Image
import os

os.makedirs('images', exist_ok=True)
for i in range(100):
    img = Image.fromarray(np.random.randint(0, 255, (224, 224, 3), dtype=np.uint8))
    img.save(f'images/img_{i}.jpg')
print(f'Created 100 sample images')
