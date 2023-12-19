#!/usr/bin/env python3

import argparse, os, requests, tqdm, pandas, pyunpack, cosy, shutil, multiprocessing

parser = argparse.ArgumentParser()
parser.add_argument("--path", type=str, required=True)
parser.add_argument("--shape", type=int, default=None)
parser.add_argument("--workers", type=int, default=1)
args = parser.parse_args()

import tiledwebmaps as twm
import xml.etree.ElementTree as ET
from PIL import Image
Image.MAX_IMAGE_PIXELS = None
import imageio.v2 as imageio
import tinypl as pl

download_path = os.path.join(args.path, "download")
if not os.path.exists(download_path):
    os.makedirs(download_path)

utm18_path = os.path.join(args.path, "utm18")
if not os.path.exists(utm18_path):
    os.makedirs(utm18_path)
utm19_path = os.path.join(args.path, "utm19")
if not os.path.exists(utm19_path):
    os.makedirs(utm19_path)



url = "https://s3.us-east-1.amazonaws.com/download.massgis.digital.mass.gov/images/coq2021_15cm_jp2/COQ2021INDEX_POLY.xlsx"
metafile = os.path.join(download_path, "COQ2021INDEX_POLY.xlsx")
twm.util.download(url, metafile)
urls = pandas.read_excel(metafile)["URL"].tolist()

if args.shape is None:
    partition = 1
else:
    partition = 10000 // args.shape
    if partition * args.shape != 10000:
        print("--shape must be a divisor of 10000")
        sys.exit(-1)
shape = (10000 // partition, 10000 // partition)

layout18 = twm.Layout(
    crs=cosy.proj.CRS("epsg:6347"),
    tile_shape=shape,
    tile_axes=cosy.geo.CompassAxes("east", "north"),
    bounds_crs=([0.0, 0.0], [10000000.0, 10000000.0]),
    zoom0_scale=0.001 / 1.5 * partition,
)

layout19 = twm.Layout(
    crs=cosy.proj.CRS("epsg:6348"),
    tile_shape=shape,
    tile_axes=cosy.geo.CompassAxes("east", "north"),
    bounds_crs=([0.0, 0.0], [10000000.0, 10000000.0]),
    zoom0_scale=0.001 / 1.5 * partition,
)

import yaml
layout_yaml = {
    "crs": "epsg:6347",
    "tile_shape": [shape[0], shape[1]],
    "tile_axes": ["east", "north"],
    "zoom0_scale": 0.001 / 1.5 * partition,
    "path": "{x}/{y}.jpg"
}
with open(os.path.join(utm18_path, "layout.yaml"), "w") as f:
    yaml.dump(layout_yaml, f, default_flow_style=False)

layout_yaml = {
    "crs": "epsg:6348",
    "tile_shape": [shape[0], shape[1]],
    "tile_axes": ["east", "north"],
    "zoom0_scale": 0.001 / 1.5 * partition,
    "path": "{x}/{y}.jpg"
}
with open(os.path.join(utm19_path, "layout.yaml"), "w") as f:
    yaml.dump(layout_yaml, f, default_flow_style=False)

print(f"Partitioning into {partition} tiles per side")


utm18_to_epsg4326 = cosy.proj.Transformer("epsg:6347", "epsg:4326")
utm19_to_epsg4326 = cosy.proj.Transformer("epsg:6348", "epsg:4326")


pipe = urls
lock = multiprocessing.Lock()
lock2 = multiprocessing.Lock()
def process(url):
    file = os.path.join(download_path, os.path.basename(url))

    if file.split("/")[-1].split(".")[0] in ["18TXN720210", "18TYN020210", "18TXN420210", "18TYN320210", "19TBH820210", "19TCH120210", "19TCH420210"]:
        return

    imagefile = file.replace('.zip', '.jp2')
    metafile = f"{imagefile}.aux.xml"

    if not os.path.isfile(metafile) or os.path.isfile(imagefile):
        is_utm_18 = file.split("/")[-1].startswith("18")
        with lock:
            twm.util.download(url, file)
        twm.util.extract(file, download_path)
        image = imageio.imread(imagefile)[:, :, :3]

        tree = ET.parse(f"{imagefile}.aux.xml")
        root = tree.getroot()

        lower_utm = [float(x) for x in list(root.iter("{http://www.opengis.net/gml}lowerCorner"))[0].text.split(" ")]
        upper_utm = [float(x) for x in list(root.iter("{http://www.opengis.net/gml}upperCorner"))[0].text.split(" ")]

        utm_to_epsg4326 = utm18_to_epsg4326 if is_utm_18 else utm19_to_epsg4326

        lower_latlon = utm_to_epsg4326(lower_utm)
        upper_latlon = utm_to_epsg4326(upper_utm)
        latlon = 0.5 * (lower_latlon + upper_latlon)

        for image, tile in twm.util.to_tiles(image, latlon, layout18 if is_utm_18 else layout19, partition):
            path = os.path.join(utm18_path if is_utm_18 else utm19_path, f"{tile[0]}")
            if not os.path.isdir(path):
                with lock2:
                    if not os.path.isdir(path):
                        os.makedirs(path)
            imageio.imwrite(os.path.join(path, f"{tile[1]}.jpg"), image, quality=100)

        os.remove(imagefile)
pipe = pl.process.map(pipe, process, workers=args.workers)

for _ in tqdm.tqdm(pipe, total=len(urls)):
    pass