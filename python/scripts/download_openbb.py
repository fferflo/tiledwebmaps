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



from bs4 import BeautifulSoup
url = "https://data.geobasis-bb.de/geobasis/daten/dop/rgb_jpg/"
r = requests.get(url)
soup = BeautifulSoup(r.content, "html.parser")

urls = [x["href"] for x in soup.select("#indexlist tr td.indexcolname a")]
urls = ["https://data.geobasis-bb.de/geobasis/daten/dop/rgb_jpg/" + u for u in urls if u.startswith("dop")][10:]

if args.shape is None:
    partition = 1
else:
    partition = 5000 // args.shape
    if partition * args.shape != 5000:
        print("--shape must be a divisor of 5000")
        sys.exit(-1)
shape = (5000 // partition, 5000 // partition)

layout = twm.Layout(
    crs=cosy.proj.CRS("epsg:25833"),
    tile_shape=shape,
    tile_axes=cosy.geo.CompassAxes("east", "north"),
    bounds_crs=([0.0, 0.0], [10000000.0, 10000000.0]),
    zoom0_scale=0.01 / 10 * partition,
)

import yaml
layout_yaml = {
    "crs": "epsg:25833",
    "tile_shape": [shape[0], shape[1]],
    "tile_axes": ["east", "north"],
    "zoom0_scale": 0.01 / 10 * partition,
    "path": "{x}/{y}.jpg"
}
with open(os.path.join(args.path, "layout.yaml"), "w") as f:
    yaml.dump(layout_yaml, f, default_flow_style=False)

print(f"Partitioning into {partition} tiles per side")


utm_to_epsg4326 = cosy.proj.Transformer("epsg:25833", "epsg:4326")

pipe = urls
lock = multiprocessing.Lock()
lock2 = multiprocessing.Lock()
def process(url):
    file = os.path.join(download_path, url.split("/")[-1])
    line = os.path.basename(file).split("_33")[1].split(".")[0].split("-")

    with lock:
        twm.util.download(url, file)
    twm.util.extract(file, download_path)
    file = file.replace(".zip", ".jpg")
    image = imageio.imread(file)[:, :, :3]

    lower_utm = np.asarray([float(line[0]), float(line[1])]) * 1000
    upper_utm = lower_utm + 1000

    lower_latlon = utm_to_epsg4326(lower_utm)
    upper_latlon = utm_to_epsg4326(upper_utm)
    latlon = 0.5 * (lower_latlon + upper_latlon)

    for image, tile in twm.util.to_tiles(image, latlon, layout, partition):
        path = os.path.join(args.path, f"{tile[0]}")
        if not os.path.isdir(path):
            with lock2:
                if not os.path.isdir(path):
                    os.makedirs(path)
        imageio.imwrite(os.path.join(path, f"{tile[1]}.jpg"), image, quality=100)

    os.remove(file)
pipe = pl.process.map(pipe, process, workers=args.workers)

for _ in tqdm.tqdm(pipe, total=len(urls)):
    pass

shutil.rmtree(download_path)