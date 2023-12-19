#!/usr/bin/env python3

import argparse, os, requests, tqdm, pyunpack, cosy, shutil, sys, multiprocessing

parser = argparse.ArgumentParser()
parser.add_argument("--path", type=str, required=True)
parser.add_argument("--shape", type=int, default=None)
parser.add_argument("--workers", type=int, default=1)
args = parser.parse_args()

import tiledwebmaps as twm
import numpy as np
from PIL import Image
Image.MAX_IMAGE_PIXELS = None
import tinypl as pl
import imageio.v2 as imageio

download_path = os.path.join(args.path, "download")
if not os.path.exists(download_path):
    os.makedirs(download_path)

urls = os.path.join(args.path, "urls.txt")
if not os.path.isfile(urls):
    print(f"Please download urls from https://www.geodaten.sachsen.de/batch-download-4719.html and save to {os.path.join(args.path, 'urls.txt')}")
    sys.exit(-1)
with open(urls, "r") as f:
    urls = f.read()
urls = urls.split("\n")
urls = [line.strip() for line in urls]
urls = [line for line in urls if line != ""]

if args.shape is None:
    partition = 1
else:
    partition = 10000 // args.shape
    if partition * args.shape != 10000:
        print("--shape must be a divisor of 10000")
        sys.exit(-1)
shape = (10000 // partition, 10000 // partition)

layout = twm.Layout(
    crs=cosy.proj.CRS("epsg:25833"),
    tile_shape=shape,
    tile_axes=cosy.geo.CompassAxes("east", "north"),
    bounds_crs=([0.0, 0.0], [10000000.0, 10000000.0]),
    zoom0_scale=0.01 / 20 * partition,
)

import yaml
layout_yaml = {
    "crs": "epsg:25833",
    "tile_shape": [shape[0], shape[1]],
    "tile_axes": ["east", "north"],
    "zoom0_scale": 0.01 / 20 * partition,
    "path": "{zoom}/{x}/{y}.jpg"
}
with open(os.path.join(args.path, "layout.yaml"), "w") as f:
    yaml.dump(layout_yaml, f, default_flow_style=False)

print(f"Partitioning into {partition} tiles per side")


utm_to_epsg4326 = cosy.proj.Transformer("epsg:25833", "epsg:4326")

pipe = urls
lock = multiprocessing.Lock()
lock2 = multiprocessing.Lock()
def process(url):
    file = os.path.join(download_path, url.split("=")[-1])
    imagefile = file.replace("_tiff.zip", ".tif")
    metafile = file.replace("_tiff.zip", "_akt.csv")

    if not os.path.isfile(metafile) or os.path.isfile(imagefile):
        for _ in range(10):
            try:
                with lock:
                    twm.util.download(url, file)
                twm.util.extract(file, download_path)
                break
            except pyunpack.PatoolError as ex:
                lastex = ex
        else:
            raise lastex
        image = imageio.imread(file.replace("_tiff.zip", ".tif"))[:, :, :3]

        with open(metafile, "r") as f:
            line = f.read().split("\n")[1]
        line = line.strip().split(";")[1].split(" ")
        lower_utm = [float(line[0]), float(line[1])]
        upper_utm = [float(line[2]), float(line[3])]

        lower_latlon = utm_to_epsg4326(lower_utm)
        upper_latlon = utm_to_epsg4326(upper_utm)
        latlon = 0.5 * (lower_latlon + upper_latlon)

        for image, tile in twm.util.to_tiles(image, latlon, layout, partition):
            path = os.path.join(args.path, "0", f"{tile[0]}")
            if not os.path.isdir(path):
                with lock2:
                    if not os.path.isdir(path):
                        os.makedirs(path)
            imageio.imwrite(os.path.join(path, f"{tile[1]}.jpg"), image, quality=100)

        os.remove(imagefile)
pipe = pl.process.map(pipe, process, workers=args.workers)

for _ in tqdm.tqdm(pipe, total=len(urls)):
    pass
