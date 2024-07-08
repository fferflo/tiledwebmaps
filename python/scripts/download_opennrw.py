#!/usr/bin/env python3

import argparse, os, requests, tqdm, pandas, pyunpack, shutil, multiprocessing

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
from openjpeg import decode

download_path = os.path.join(args.path, "download")
if not os.path.exists(download_path):
    os.makedirs(download_path)



url = "https://www.opengeodata.nrw.de/produkte/geobasis/lusat/akt/dop/dop_jp2_f10/dop_meta.zip"
metafile = os.path.join(download_path, "dop_meta.zip")
twm.util.download(url, metafile)
twm.util.extract(metafile, download_path)

with open(os.path.join(download_path, "dop_nw.csv")) as f:
    lines = f.readlines()
lines = [l for l in lines if l.startswith("dop10rgbi")]
if len(lines) > 0:
    def parse_line(line):
        # dop10rgbi_32_470_5786_1_nw_2022;0;2022-06-23;1386/22 Minden Lübbecke;UCEM3-431S92908X210339-f100_UCE-M3;10;RGBI;25832;7837;bDOM;470000;5786000;10000;10000;8;20;JPEG2000;0;1;1;GDAL_JP2ECW, 90;3;keine
        line = line.split(";")
        assert line[7] == "25832" and line[12] == "10000" and line[13] == "10000"
        name = line[0]
        lower_utm = np.asarray([float(line[10]), float(line[11])])
        return name, lower_utm
    lines = [parse_line(l) for l in tqdm.tqdm(lines, desc="Parsing metadata")]
else:
    print(f"Something is wrong with the file at {url}")
    sys.exit(-1)
    url = "https://www.opengeodata.nrw.de/produkte/geobasis/lusat/dop/dop_jp2_f10/dop_j2w.zip"
    metafile = os.path.join(download_path, "dop_j2w.zip")
    twm.util.download(url, metafile)
    twm.util.extract(metafile, download_path)
    def parse_file(file):
        with open(os.path.join(download_path, file)) as f:
            lines = f.readlines()
        name = file[:-4]
        lower_utm = np.asarray([float(lines[4]), float(lines[5])])
        return name, lower_utm
    lines = [parse_file(f) for f in tqdm.tqdm([f for f in os.listdir(download_path) if f.endswith(".j2w")], desc="Parsing metadata")]


if args.shape is None:
    partition = 1
else:
    partition = 10000 // args.shape
    if partition * args.shape != 10000:
        print("--shape must be a divisor of 10000")
        sys.exit(-1)

shape = (10000 // partition, 10000 // partition)
tile_shape_crs = [1000.0 / partition, 1000.0 / partition]

layout = twm.Layout(
    crs=twm.proj.CRS("epsg:25832"),
    tile_shape_px=shape,
    tile_shape_crs=tile_shape_crs,
    tile_axes=twm.geo.CompassAxes("east", "north"),
)

import yaml
layout_yaml = {
    "crs": "epsg:25832",
    "tile_shape_px": [shape[0], shape[1]],
    "tile_shape_crs": tile_shape_crs,
    "tile_axes": ["east", "north"],
    "path": "{zoom}/{x}/{y}.jpg",
    "min_zoom": 0,
    "max_zoom": 0,
}
with open(os.path.join(args.path, "layout.yaml"), "w") as f:
    yaml.dump(layout_yaml, f, default_flow_style=False)

print(f"Partitioning into {partition} tiles per side")

utm_to_epsg4326 = twm.proj.Transformer("epsg:25832", "epsg:4326")



pipe = lines
pipe = pl.thread.mutex(pipe)

lock = multiprocessing.Lock()
lock2 = multiprocessing.Lock()
@pl.unpack
def process(name, lower_utm):
    url = f"https://www.opengeodata.nrw.de/produkte/geobasis/lusat/akt/dop/dop_jp2_f10/{name}.jp2"

    imagefile = os.path.join(download_path, f"{name}.jp2")
    metafile = imagefile.replace(".jp2", ".done")

    if not os.path.isfile(metafile) or os.path.isfile(imagefile):
        with lock:
            twm.util.download(url, imagefile)

        try:
            with open(imagefile, "rb") as f:
                image = decode(f.read())[:, :, :3]
        except Exception as e:
            print(f"Could not read {imagefile}")
            print(e)
            return
        with open(metafile, "w") as f:
            f.write("")

        upper_utm = lower_utm + 500
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

for _ in tqdm.tqdm(pipe, total=len(lines)):
    pass

shutil.rmtree(download_path)

twm.util.add_zooms(args.path, workers=args.workers)