from PIL import Image
import sys
import os

if len(sys.argv) != 2:
    raise RuntimeError('Expected path to image as argument')

img = Image.open(sys.argv[1])
pixels = img.load()

asset_name = os.path.basename(sys.argv[1])
asset_name = asset_name[:asset_name.rfind('.')]

print('#include "mi0283qt.h"\n')
print('const struct image img_{} = {{'.format(asset_name))
print('  .width = {},'.format(img.size[0]))
print('  .height = {},'.format(img.size[1]))
print('  .pixels = {')

for y in range(img.size[1]):
    print('    // line {}'.format(y))
    for x in range(img.size[0]):
        p = pixels[x, y]
        line = '    RGB_({}, {}, {}),'.format(p[0] >> 2, p[1] >> 2, p[2] >> 2)
        print(line)

print('}};')
