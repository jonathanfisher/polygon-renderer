#!/usr/bin/python

import sys
from math import *
from PIL import Image

WIDTH  = 200
HEIGHT = 200

img = Image.new('RGB', (WIDTH, HEIGHT))
pixels = img.load()

filename = sys.argv[1]

f = open(filename, 'rt');
index = 0

for line in f:
    values = line.split(',')
    red   = int(values[0])
    green = int(values[1])
    blue  = int(values[2])
    y = int(floor(index / WIDTH))
    x = index % WIDTH
    pixels[x, y] = (red, green, blue)
    index += 1

f.close()

img.show()
