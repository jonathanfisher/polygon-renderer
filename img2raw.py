#!/usr/bin/python

from PIL import Image

img = Image.open('mona_lisa_crop.jpg')
(width, height) = img.size

f = open('raw.dat', 'w')

for y in range(0, height):
    for x in range(0, width):
        (r, g, b) = img.getpixel((x, y))
        f.write(str(r) + ', ' + str(g) + ', ' + str(b) + '\n')

f.close()
