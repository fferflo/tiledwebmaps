import os
import threading
import time
import numpy as np
import tiledwebmaps as twm
import multiprocessing
import yaml

def download(url, file, retries=100, timeout=10.0):
    import requests
    dir = os.path.dirname(os.path.abspath(file))
    if not os.path.isdir(dir):
        os.makedirs(dir)

    for _ in range(retries):
        if os.path.isfile(file):
            os.remove(file)
        elif os.path.isdir(file):
            raise ValueError("Target path is a directory")
        try:
            resp = requests.get(url, stream=True, timeout=timeout)
            total = int(resp.headers.get("content-length", 0))
            received = 0
            with open(file, "wb") as f:
                for data in resp.iter_content(chunk_size=1024):
                    size = f.write(data)
                    received += size
            if received < total:
                error = requests.exceptions.RequestException("Content too short", response=resp)
                continue
        except requests.exceptions.RequestException as e:
            error = e
            time.sleep(timeout)
            continue
        break
    else:
        if os.path.isfile(file):
            os.remove(file)
        raise error

def extract(src, dest=None):
    import pyunpack
    if dest is None:
        dest = os.path.dirname(src)
    if not os.path.isdir(dest):
        os.makedirs(dest)
    pyunpack.Archive(src).extractall(dest)
    os.remove(src)

def to_tiles(image, latlon, layout, partition):
    tile = (layout.epsg4326_to_tile(latlon, zoom=0) / partition).astype("int32") * partition

    px0 = layout.tile_to_pixel(tile, zoom=0).astype("int32")
    px1 = layout.tile_to_pixel((tile + partition), zoom=0).astype("int32")
    min_px = np.minimum(px0, px1)

    for tx in range(partition):
        for ty in range(partition):
            px0 = layout.tile_to_pixel(tile + np.asarray([tx, ty]), zoom=0).astype("int32") - min_px
            px1 = layout.tile_to_pixel(tile + np.asarray([tx, ty]) + 1, zoom=0).astype("int32") - min_px
            px0, px1 = np.minimum(px0, px1), np.maximum(px0, px1)

            subimage = image[px0[0]:px1[0], px0[1]:px1[1]]
            subtile = tile + np.asarray([tx, ty])
            assert np.prod(subimage.shape) != 0

            yield subimage, subtile

class RunException(BaseException):
    def __init__(self, message, code):
        self.message = message
        self.code = code

def run(command):
    print("> " + command)
    returncode = os.system(f"bash -c '{command}'")
    if returncode != 0:
        raise RunException("Failed to run " + command + ". Got return code " + str(returncode), returncode)

class Ratelimit:
    def __init__(self, num, period):
        self.num = num
        self.period = period
        self.lock = threading.RLock()
        self.last_times = []

    @property
    def time_to_next_drop(self):
        with self.lock:
            if len(self.last_times) == 0:
                return 0.0
            return max(self.last_times[0] + self.period - time.time(), 0.0)

    def __enter__(self):
        while True:
            with self.lock:
                t = time.time() - self.period
                while len(self.last_times) > 0 and self.last_times[0] < t:
                    self.last_times.pop(0)
                if len(self.last_times) >= self.num:
                    wait = self.time_to_next_drop
                else:
                    self.last_times.append(time.time())
                    break
            time.sleep(wait)
        return self

    def __exit__(self, exc_type, exc_val, exc_tb):
        pass

def add_zooms(path, min_zoom=None, workers=16, min_tiles=8):
    print(f"Adding zoom levels to tiles at {path}")
    import cv2
    import tinypl as pl
    import tqdm
    zooms = []
    for z in os.listdir(path):
        try:
            zooms.append(int(z))
        except:
            pass
    x = os.listdir(os.path.join(path, str(zooms[0])))[0]
    y = os.listdir(os.path.join(path, str(zooms[0]), x))[0]
    input_shape = cv2.imread(os.path.join(path, str(zooms[0]), x, y)).shape[:2]
    assert input_shape[0] == input_shape[1]
    input_shape = input_shape[0]
    filetype = y.split(".")[-1]

    with open(os.path.join(path, "layout.yaml"), "r") as f:
        config = yaml.safe_load(f)
    layout = twm.Layout.from_yaml(os.path.join(path, "layout.yaml"))

    max_zoom = int(np.max(zooms))
    if max_zoom != config["max_zoom"]:
        raise ValueError(f"max_zoom in layout.yaml is {config['max_zoom']} but the highest zoom found is {max_zoom}")
    lock = multiprocessing.Lock()

    prev_tiles = None

    zoom = max_zoom

    while min_zoom is None or zoom >= min_zoom:
        if min_zoom is None and prev_tiles is not None and len(prev_tiles) <= min_tiles:
            min_zoom = zoom + 1
            print(f"Reached minimum zoom {min_zoom} with {len(prev_tiles)} tiles")
            break

        print(f"########## zoom {zoom} ##########")
        zoom_path = os.path.join(path, str(zoom))
        if os.path.isdir(zoom_path):
            print("Collecting existing tiles...", end="", flush=True)
            tiles = [(int(x), int(y.split(".")[0])) for x in os.listdir(zoom_path) for y in os.listdir(os.path.join(zoom_path, x))]
            print(f" got {len(tiles)} tiles")
        else:
            tiles = []

        if prev_tiles is None:
            if len(tiles) == 0:
                raise ValueError("No tiles found at the highest zoom level")
        else:
            tiles = sorted(list(set([(x // 2, y // 2) for x, y in prev_tiles])))

            pipe = tiles
            pipe = pl.thread.mutex(pipe)

            @pl.unpack
            def process(tile_out_x, tile_out_y):
                output_path = os.path.join(path, str(zoom), str(tile_out_x))
                output_file = os.path.join(output_path, str(tile_out_y) + "." + filetype)
                if not os.path.isfile(output_file):
                    image = np.zeros((2 * input_shape, 2 * input_shape, 3), dtype=np.uint8) + 255

                    image_min_px = layout.tile_to_pixel([tile_out_x * 2, tile_out_y * 2], zoom + 1).astype("int32")
                    image_max_px = layout.tile_to_pixel([(tile_out_x + 1) * 2, (tile_out_y + 1) * 2], zoom + 1).astype("int32")
                    image_min_px, image_max_px = np.minimum(image_min_px, image_max_px), np.maximum(image_min_px, image_max_px)

                    found = 0
                    for x in range(2):
                        tile_in_x = 2 * tile_out_x + x
                        for y in range(2):
                            tile_in_y = 2 * tile_out_y + y
                            input_path = os.path.join(path, str(zoom + 1), str(tile_in_x), str(tile_in_y) + "." + filetype)
                            if os.path.isfile(input_path):
                                found += 1
                                subimage = cv2.imread(input_path)
                                assert subimage.shape[0] == subimage.shape[1] and subimage.shape[0] == input_shape

                                min_px = layout.tile_to_pixel([tile_in_x, tile_in_y], zoom + 1).astype("int32")
                                max_px = layout.tile_to_pixel([tile_in_x + 1, tile_in_y + 1], zoom + 1).astype("int32")
                                min_px, max_px = np.minimum(min_px, max_px), np.maximum(min_px, max_px)

                                min_px -= image_min_px
                                max_px -= image_min_px

                                image[min_px[0]:max_px[0], min_px[1]:max_px[1]] = subimage

                    image = cv2.resize(image, (input_shape, input_shape), interpolation=cv2.INTER_AREA)

                    if not os.path.exists(output_path):
                        with lock:
                            if not os.path.exists(output_path):
                                os.makedirs(output_path)

                    cv2.imwrite(os.path.join(output_path, str(tile_out_y) + "." + filetype), image)
                    return found
                else:
                    return None

            pipe = pl.process.map(pipe, process, workers=workers)

            total = 0
            missing = 0
            for found in tqdm.tqdm(pipe, total=len(tiles), desc="Processing tiles"):
                if not found is None:
                    total += 4
                    missing += 4 - found
            print(f"{missing} of {total} tiles were filled with white pixels")

        prev_tiles = tiles
        zoom -= 1

    # Overwrite min_zoom
    config["min_zoom"] = int(min_zoom)
    with open(os.path.join(path, "layout.yaml"), "w") as f:
        yaml.safe_dump(config, f)