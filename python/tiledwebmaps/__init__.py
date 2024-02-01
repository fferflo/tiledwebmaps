import cosy # Required before PROJ is initialized, since PROJ data is packaged with cosy
import yaml, os
from .backend import Layout, TileLoader, Http, Disk, DiskCached, LRU, LRUCached, WithDefault
from .config import from_config
from . import presets
from .presets import *
from . import util

def _layout_from_yaml(layout):
    kwargs = {}

    if "preset" in layout:
        if any(s in layout for s in ["crs", "tile_axes", "tile_shape_px", "tile_shape_crs", "origin_crs", "size_crs"]):
            raise ValueError("Cannot combine preset with other layout parameters")
        if layout["preset"] == "XYZ":
            return twm.Layout.XYZ()
        else:
            raise ValueError("Unknown preset: " + layout["preset"])

    if "crs" in layout:
        kwargs["crs"] = cosy.proj.CRS(layout["crs"])
    if "tile_axes" in layout:
        kwargs["tile_axes"] = cosy.geo.CompassAxes(layout["tile_axes"][0], layout["tile_axes"][1])
    if "tile_shape_px" in layout:
        kwargs["tile_shape_px"] = layout["tile_shape_px"]
    if "tile_shape_crs" in layout:
        kwargs["tile_shape_crs"] = layout["tile_shape_crs"]
    if "origin_crs" in layout:
        kwargs["origin_crs"] = layout["origin_crs"]
    if "size_crs" in layout:
        kwargs["size_crs"] = layout["size_crs"]

    return twm.Layout(**kwargs)

def layout_from_yaml(path_or_dict):
    if isinstance(path_or_dict, str):
        with open(path_or_dict, "r") as f:
            cfg = yaml.safe_load(f)
    else:
        cfg = path_or_dict
    return _layout_from_yaml(cfg)
setattr(twm.Layout, "from_yaml", layout_from_yaml)

def from_yaml(path, wait_after_last_modified=1.0):
    if not path.endswith(".yaml"):
        path = os.path.join(path, "layout.yaml")
    with open(path, "r") as f:
        cfg = yaml.safe_load(f)

    if "path" in cfg:
        path2 = cfg["path"]
    else:
        path2 = "{zoom}/{x}/{y}.jpg"
    path = os.path.join(os.path.dirname(path), path2)
    layout = _layout_from_yaml(cfg)

    if "url" in cfg:
        tileloader = twm.Http(cfg["url"], layout)
        tileloader = twm.DiskCached(tileloader, path, wait_after_last_modified=wait_after_last_modified)
    else:
        tileloader = twm.Disk(path=path, layout=layout, wait_after_last_modified=wait_after_last_modified)

    return tileloader