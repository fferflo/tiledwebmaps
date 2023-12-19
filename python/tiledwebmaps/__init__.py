import cosy # Required before PROJ is initialized, since PROJ data is packaged with cosy
import yaml, os
from .backend import Layout, TileLoader, Http, Disk, DiskCached, LRU, LRUCached, WithDefault
from .config import from_config
from . import presets
from .presets import *
from . import util

def _layout_from_yaml(layout):
    if "bounds_crs" in layout:
        bounds_crs = (layout["bounds_crs"]["min"], layout["bounds_crs"]["max"])
    else:
        bounds_crs = ([0.0, 0.0], [10000000.0, 10000000.0])
    if "use_only_first_bound_axis" in layout:
        use_only_first_bound_axis = layout["use_only_first_bound_axis"]
    else:
        use_only_first_bound_axis = True
    return twm.Layout(
        crs=cosy.proj.CRS(layout["crs"]),
        tile_shape=layout["tile_shape"],
        tile_axes=cosy.geo.CompassAxes(layout["tile_axes"][0], layout["tile_axes"][1]),
        bounds_crs=bounds_crs,
        zoom0_scale=layout["zoom0_scale"],
        use_only_first_bound_axis=use_only_first_bound_axis,
    )

def layout_from_yaml(path):
    with open(path, "r") as f:
        cfg = yaml.safe_load(f)
    return _layout_from_yaml(cfg)
setattr(twm.Layout, "from_yaml", layout_from_yaml)

def disk_from_yaml(path, wait_after_last_modified=1.0):
    with open(path, "r") as f:
        cfg = yaml.safe_load(f)
    return twm.Disk(
        path=os.path.join(os.path.dirname(path), cfg["path"]),
        layout=_layout_from_yaml(cfg),
        wait_after_last_modified=wait_after_last_modified,
    )
setattr(twm.Disk, "from_yaml", disk_from_yaml)