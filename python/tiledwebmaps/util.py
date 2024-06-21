import os, threading, time
import numpy as np

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
            yield image[px0[0]:px1[0], px0[1]:px1[1]], tile + np.asarray([tx, ty])

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