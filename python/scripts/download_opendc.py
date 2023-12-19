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

print("Extracting...")
twm.util.extract(os.path.join(args.path, "OpenDataDC_Orthophoto_2021_JPEG2000.zip"), download_path)
twm.util.run(f"gdal_retile.py -ps 10000 10000 -targetDir {download_path} {download_path}/DCOCTO-2021.jp2")


if args.shape is None:
    partition = 1
else:
    partition = 10000 // args.shape
    if partition * args.shape != 10000:
        print("--shape must be a divisor of 10000")
        sys.exit(-1)
shape = (10000 // partition, 10000 // partition)

layout = twm.Layout(
    crs=cosy.proj.CRS("epsg:26985"),
    tile_shape=shape,
    tile_axes=cosy.geo.CompassAxes("east", "north"),
    bounds_crs=([800 * 0.75, 800 * 0.25], [10000000.0, 10000000.0]),
    zoom0_scale=0.001 / 0.8 * partition,
    use_only_first_bound_axis=False,
)

import yaml
layout_yaml = {
    "crs": "epsg:26985",
    "tile_shape": [shape[0], shape[1]],
    "tile_axes": ["east", "north"],
    "zoom0_scale": 0.001 / 0.8 * partition,
    "path": "{x}/{y}.jpg",
    "use_only_first_bound_axis": False,
    "bounds_crs": {
        "min": [800 * 0.75, 800 * 0.25],
        "max": [10000000.0, 10000000.0],
    },
}
with open(os.path.join(args.path, "layout.yaml"), "w") as f:
    yaml.dump(layout_yaml, f, default_flow_style=False)

print(f"Partitioning into {partition} tiles per side")

crs_to_epsg4326 = cosy.proj.Transformer("epsg:26985", "epsg:4326")




files = sorted([f for f in os.listdir(download_path) if f.startswith("DCOCTO-2021_")])

pipe = files
lock = multiprocessing.Lock()
def process(file):
    line = os.path.basename(file).split(".")[0].split("_")[1:3] # DCOCTO-2021_11_03.tif
    tile = np.asarray([485 + int(line[1]), 185 - int(line[0])]) * partition

    image = imageio.imread(os.path.join(download_path, file))[:, :, :3]

    if np.min(image) > 220:
        return

    lower_crs = layout.tile_to_crs(tile, zoom=0)
    upper_crs = lower_crs + 300

    lower_latlon = crs_to_epsg4326(lower_crs)
    upper_latlon = crs_to_epsg4326(upper_crs)
    latlon = 0.5 * (lower_latlon + upper_latlon)

    for image, tile in twm.util.to_tiles(image, latlon, layout, partition):
        path = os.path.join(args.path, f"{tile[0]}")
        if not os.path.isdir(path):
            with lock:
                if not os.path.isdir(path):
                    os.makedirs(path)
        imageio.imwrite(os.path.join(path, f"{tile[1]}.jpg"), image, quality=100)

pipe = pl.process.map(pipe, process, workers=args.workers)

for _ in tqdm.tqdm(pipe, total=len(files)):
    pass

shutil.rmtree(download_path)