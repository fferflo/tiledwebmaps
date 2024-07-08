#!/usr/bin/env python3

import argparse, os, requests, tqdm, pyunpack, shutil, sys, multiprocessing

parser = argparse.ArgumentParser()
parser.add_argument("--path", type=str, required=True)
parser.add_argument("--shape", type=int, default=None)
parser.add_argument("--workers", type=int, default=32)
args = parser.parse_args()

import tiledwebmaps as twm
import numpy as np
from PIL import Image
Image.MAX_IMAGE_PIXELS = None
import tinypl as pl
import imageio.v2 as imageio

if shutil.which("gdal_retile.py") is None:
    print("This script requires gdal to run. On ubuntu, please run: sudo apt install gdal-bin python3-gdal")
    sys.exit(-1)

file = os.path.join(args.path, "OpenDataDC_Orthophoto_2021_JPEG2000.zip")
if not os.path.exists(file):
    print("Please download the file 'OpenDataDC_Orthophoto_2021_JPEG2000.zip' from https://dcgov.app.box.com/v/orthophoto2021jpeg"
    f"and place it in the directory:\n{args.path}")
    sys.exit(-1)

download_path = os.path.join(args.path, "download")
if not os.path.exists(download_path):
    os.makedirs(download_path)

print("Extracting...")
twm.util.extract(file, download_path)
twm.util.run(f"gdal_retile.py -ps 1000 1000 -targetDir {download_path} {download_path}/DCOCTO-2021.jp2")

if args.shape is None:
    partition = 1
else:
    partition = 1000 // args.shape
    if partition * args.shape != 1000:
        print("--shape must be a divisor of 1000")
        sys.exit(-1)

shape = (1000 // partition, 1000 // partition)
tile_shape_crs = [80.0 / partition, 80.0 / partition]
origin_crs = [800 * 0.75, 800 * 0.25]

layout = twm.Layout(
    crs=twm.proj.CRS("epsg:26985"),
    tile_shape_px=shape,
    tile_shape_crs=tile_shape_crs,
    origin_crs=origin_crs,
    tile_axes=twm.geo.CompassAxes("east", "north"),
)

import yaml
layout_yaml = {
    "crs": "epsg:26985",
    "tile_shape_px": [shape[0], shape[1]],
    "tile_shape_crs": tile_shape_crs,
    "origin_crs": origin_crs,
    "tile_axes": ["east", "north"],
    "path": "{zoom}/{x}/{y}.jpg",
    "min_zoom": 0,
    "max_zoom": 0,
}
with open(os.path.join(args.path, "layout.yaml"), "w") as f:
    yaml.dump(layout_yaml, f, default_flow_style=False)

crs_to_epsg4326 = twm.proj.Transformer("epsg:26985", "epsg:4326")




files = sorted([f for f in os.listdir(download_path) if f.startswith("DCOCTO-2021_")])

pipe = files
pipe = pl.thread.mutex(pipe)

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
        path = os.path.join(args.path, "0", f"{tile[0]}")
        if not os.path.isdir(path):
            with lock:
                if not os.path.isdir(path):
                    os.makedirs(path)
        imageio.imwrite(os.path.join(path, f"{tile[1]}.jpg"), image, quality=100)
pipe = pl.process.map(pipe, process, workers=args.workers)

for _ in tqdm.tqdm(pipe, total=len(files)):
    pass

shutil.rmtree(download_path)

twm.util.add_zooms(args.path, workers=args.workers)