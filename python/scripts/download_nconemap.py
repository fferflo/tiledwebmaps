#!/usr/bin/env python3

import argparse, os, requests, tqdm, shutil, sys, multiprocessing, cv2
import imageio.v2 as imageio
import tiledwebmaps as twm
from datetime import datetime
import tinypl as pl
import numpy as np

parser = argparse.ArgumentParser()
parser.add_argument("--path", type=str, required=True)
parser.add_argument("--shape", type=int, default=None)
parser.add_argument("--workers", type=int, default=32)
args = parser.parse_args()

if shutil.which("gdal_retile.py") is None:
    print("This script requires gdal to run. On ubuntu, please run: sudo apt install gdal-bin python3-gdal")
    sys.exit(-1)

if shutil.which("docker") is None:
    print("This script requires docker to run. Please install docker.")
    sys.exit(-1)

areas = """
<option value="alamance">Alamance</option>
<option value="alexander">Alexander</option>
<option value="alleghany">Alleghany</option>
<option value="anson">Anson</option>
<option value="ashe">Ashe</option>
<option value="avery">Avery</option>
<option value="beaufort">Beaufort</option>
<option value="bertie">Bertie</option>
<option value="bladen">Bladen</option>
<option value="brunswick">Brunswick</option>
<option value="buncombe">Buncombe</option>
<option value="burke">Burke</option>
<option value="cabarrus">Cabarrus</option>
<option value="caldwell">Caldwell</option>
<option value="camden">Camden</option>
<option value="carteret">Carteret</option>
<option value="caswell">Caswell</option>
<option value="catawba">Catawba</option>
<option value="chatham">Chatham</option>
<option value="cherokee">Cherokee</option>
<option value="chowan">Chowan</option>
<option value="clay">Clay</option>
<option value="cleveland">Cleveland</option>
<option value="columbus">Columbus</option>
<option value="craven">Craven</option>
<option value="cumberland">Cumberland</option>
<option value="currituck">Currituck</option>
<option value="dare">Dare</option>
<option value="davidson">Davidson</option>
<option value="davie">Davie</option>
<option value="duplin">Duplin</option>
<option value="durham">Durham</option>
<option value="edgecombe">Edgecombe</option>
<option value="forsyth">Forsyth</option>
<option value="franklin">Franklin</option>
<option value="gaston">Gaston</option>
<option value="gates">Gates</option>
<option value="graham">Graham</option>
<option value="granville">Granville</option>
<option value="greene">Greene</option>
<option value="guilford">Guilford</option>
<option value="halifax">Halifax</option>
<option value="harnett">Harnett</option>
<option value="haywood">Haywood</option>
<option value="henderson">Henderson</option>
<option value="hertford">Hertford</option>
<option value="hoke">Hoke</option>
<option value="hyde">Hyde</option>
<option value="iredell">Iredell</option>
<option value="jackson">Jackson</option>
<option value="johnston">Johnston</option>
<option value="jones">Jones</option>
<option value="lee">Lee</option>
<option value="lenoir">Lenoir</option>
<option value="lincoln">Lincoln</option>
<option value="mcdowell">McDowell</option>
<option value="macon">Macon</option>
<option value="madison">Madison</option>
<option value="martin">Martin</option>
<option value="mecklenburg">Mecklenburg</option>
<option value="mitchell">Mitchell</option>
<option value="montgomery">Montgomery</option>
<option value="moore">Moore</option>
<option value="nash">Nash</option>
<option value="newhanover">New Hanover</option>
<option value="northampton">Northampton</option>
<option value="onslow">Onslow</option>
<option value="orange">Orange</option>
<option value="pamlico">Pamlico</option>
<option value="pasquotank">Pasquotank</option>
<option value="pender">Pender</option>
<option value="perquimans">Perquimans</option>
<option value="person">Person</option>
<option value="pitt">Pitt</option>
<option value="polk">Polk</option>
<option value="randolph">Randolph</option>
<option value="richmond">Richmond</option>
<option value="robeson">Robeson</option>
<option value="rockingham">Rockingham</option>
<option value="rowan">Rowan</option>
<option value="rutherford">Rutherford</option>
<option value="sampson">Sampson</option>
<option value="scotland">Scotland</option>
<option value="stanly">Stanly</option>
<option value="stokes">Stokes</option>
<option value="surry">Surry</option>
<option value="swain">Swain</option>
<option value="transylvania">Transylvania</option>
<option value="tyrrell">Tyrrell</option>
<option value="union">Union</option>
<option value="vance">Vance</option>
<option value="wake">Wake</option>
<option value="warren">Warren</option>
<option value="washington">Washington</option>
<option value="watauga">Watauga</option>
<option value="wayne">Wayne</option>
<option value="wilkes">Wilkes</option>
<option value="wilson">Wilson</option>
<option value="yadkin">Yadkin</option>
<option value="yancey">Yancey</option>
"""
areas = [a[a.index(">") + 1:a.index("<", 1)] for a in areas.split("\n") if a.startswith("<option value=")]

current_year = datetime.now().year

download_path = os.path.join(args.path, "download")
os.makedirs(download_path, exist_ok=True)







if args.shape is None:
    partition = 1
else:
    partition = 10000 // args.shape
    if partition * args.shape != 10000:
        print("--shape must be a divisor of 10000")
        sys.exit(-1)

shape = (10000 // partition, 10000 // partition)
tile_shape_crs = [5000.0 / partition, 5000.0 / partition]

layout = twm.Layout(
    crs=twm.proj.CRS("epsg:6543"),
    tile_shape_px=shape,
    tile_shape_crs=tile_shape_crs,
    tile_axes=twm.geo.CompassAxes("east", "north"),
)

import yaml
layout_yaml = {
    "crs": "epsg:6543",
    "tile_shape_px": [shape[0], shape[1]],
    "tile_shape_crs": tile_shape_crs,
    "tile_axes": ["east", "north"],
    "path": "{zoom}/{x}/{y}.jpg",
    "min_zoom": 0,
    "max_zoom": 0,
}
with open(os.path.join(args.path, "layout.yaml"), "w") as f:
    yaml.dump(layout_yaml, f, default_flow_style=False)

crs_to_epsg4326 = twm.proj.Transformer("epsg:6543", "epsg:4326")




pipe = areas
pipe = pl.thread.mutex(pipe)

def run_docker(cmd):
    if os.system(f"docker run -ti --rm -v {download_path}:/data klokantech/gdal {cmd} > /dev/null") != 0:
        raise Exception(f"Error running command: {cmd}")

def run(cmd):
    if os.system(f"{cmd} > /dev/null") != 0:
        raise Exception(f"Error running command: {cmd}")

lock1 = multiprocessing.Lock()
lock2 = multiprocessing.Lock()

def process(area):
    area = area.replace(" ", "")

    # Download image of area
    for year in range(current_year, 2010, -1):
        url = f"https://s3.amazonaws.com/dit-cgia-gis-data/orthoimagery-program/county-mosaics/{area}_{year}.zip"
        file = os.path.join(download_path, os.path.basename(url))
        try:
            with lock1:
                twm.util.download(url, file)
        except Exception as e:
            expr = e
            continue
        if os.path.getsize(file) < 10000:
            os.remove(file)
            expr = None
            continue
        try:
            twm.util.extract(file, download_path)
        except Exception as e:
            expr = e
            continue
        break
    else:
        print(f"Could not download {area}" + (f"\nException:\n{expr}" if expr is not None else ""))
        sys.exit(-1)

    files = [f for f in os.listdir(download_path) if f.endswith(".sid") and f.startswith(area)]
    assert len(files) == 1
    name = files[0].split(".")[0]

    # Convert sid to tif
    run_docker(f"gdal_translate --config GDAL_CACHEMAX 512 -co COMPRESS=JPEG -co TILED=YES -co NUM_THREADS=1 -co BIGTIFF=YES -b 1 -b 2 -b 3 {name}.sid {name}.tif")
    os.remove(os.path.join(download_path, name + ".sid"))

    # Split tif into 10000x10000 tiles (-> original tile size)
    os.makedirs(os.path.join(download_path, name), exist_ok=True)
    run(f"gdal_retile.py -ps 10000 10000 -targetDir {os.path.join(download_path, name)} --config GDAL_CACHEMAX 512 -co NUM_THREADS=1 -co COMPRESS=JPEG -co TILED=YES -co BIGTIFF=YES {os.path.join(download_path, name + '.tif')}")
    run_docker(f"rm {name}.tif")

    # Convert and split into specified tile size
    files = sorted([f for f in os.listdir(os.path.join(download_path, name)) if f.endswith(".tif")])
    for file in files:
        file = os.path.join(download_path, name, file)
        image = cv2.imread(file)[:, :, :3]
        image = image[:, :, ::-1]
        if np.mean(image) > 2:
            # Tile is valid

            lines = os.popen(f"gdalinfo {file}").read()
            lines = lines.split("\n")
            def get_point(prefix):
                lines2 = [l for l in lines if l.startswith(prefix)]
                assert len(lines2) == 1
                point = lines2[0].split("(")[1].split(")")[0].split(",")
                point = [float(o) for o in point]
                return point
            center_crs = get_point("Center")
            latlon = crs_to_epsg4326(center_crs)

            for image, tile in twm.util.to_tiles(image, latlon, layout, partition):
                path = os.path.join(args.path, "0", f"{tile[0]}")
                if not os.path.isdir(path):
                    with lock2:
                        if not os.path.isdir(path):
                            os.makedirs(path)
                imageio.imwrite(os.path.join(path, f"{tile[1]}.jpg"), image, quality=100)

        os.remove(file)
pipe = pl.process.map(pipe, process, workers=args.workers)

for _ in tqdm.tqdm(pipe, desc="Downloading areas", total=len(areas)):
    pass



shutil.rmtree(download_path)

twm.util.add_zooms(args.path, workers=args.workers)