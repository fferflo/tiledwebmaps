#!/usr/bin/env python3

import argparse, os, tqdm, shutil
import numpy as np
import tinypl as pl

parser = argparse.ArgumentParser()
parser.add_argument("--input", type=str, required=True)
parser.add_argument("--output", type=str, required=True)
parser.add_argument("--workers", type=int, default=8)
args = parser.parse_args()

print("Finding tiles...")
tiles = []
for zoom in os.listdir(args.input):
    zoom_path = os.path.join(args.input, zoom)
    if not os.path.isdir(zoom_path):
        continue
    for x in os.listdir(zoom_path):
        x_path = os.path.join(zoom_path, x)
        if not os.path.isdir(x_path):
            continue
        for y in os.listdir(x_path):
            file = os.path.join(x_path, y)
            if os.path.isfile(file) and file.endswith(".jpg"):
                size = os.path.getsize(file)
                tiles.append((int(zoom), int(x), int(y[:-4]), size))

print("Sorting tiles...")
tiles = sorted(tiles)

print("Computing tile offsets...")
sizes = np.asarray([tile[3] for tile in tiles])
tile_starts = np.concatenate(([0], np.cumsum(sizes)))
tiles = [(zoom, x, y, size, offset) for (zoom, x, y, size), offset in zip(tiles, tile_starts)]
total_size = tile_starts[-1]

print(f"Allocating {total_size / 1024 ** 3:.2f} GB of memory...")
if not os.path.exists(args.output):
    os.makedirs(args.output)
shutil.copy(os.path.join(args.input, "layout.yaml"), os.path.join(args.output, "layout.yaml"))
images_bin = np.memmap(os.path.join(args.output, "images.dat"), dtype="uint8", mode="w+", shape=(total_size,))

np.savez(
    os.path.join(args.output, "images-meta.npz"),
    zoom=np.asarray([tile[0] for tile in tiles]).astype("int64"),
    x=np.asarray([tile[1] for tile in tiles]).astype("int64"),
    y=np.asarray([tile[2] for tile in tiles]).astype("int64"), 
    offset=tile_starts.astype("int64"),
)

pipe = tiles
pipe = pl.thread.mutex(pipe)

@pl.unpack
def process(zoom, x, y, size, offset):
    file = os.path.join(args.input, str(zoom), str(x), str(y) + ".jpg")
    data = np.fromfile(file, dtype="uint8")
    return data, offset
pipe = pl.process.map(pipe, process, workers=args.workers)

for data, offset in tqdm.tqdm(pipe, total=len(tiles), desc="Copying to binary..."):
    images_bin[offset:offset + len(data)] = data