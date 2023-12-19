import yaml, cosy
import tiledwebmaps as twm
import numpy as np

def from_config(config, wait_after_error=5.0, retries=100):
    if isinstance(config, str):
        if config.endswith(".yaml"):
            with open(config, "r") as f:
                config = yaml.safe_load(f)
        else:
            raise ValueError(f"Invalid file type {config}")
    if not isinstance(config, dict):
        raise ValueError(f"Invalid config type {type(config)}")

    if "http-header" in config:
        header = config["http-header"]
    else:
        header = {}

    config = config["tileloaders"]

    tileloaders = {}
    presets = vars(twm.presets)
    for name, values in config.items():
        kwargs = {**values}
        if "path" in kwargs:
            del kwargs["path"]
        if "zoom" in kwargs:
            del kwargs["zoom"]

        if "load_zoom_up" in values:
            load_zoom_up = int(values["load_zoom_up"])
            del kwargs["load_zoom_up"]
        else:
            load_zoom_up = 0

        if name in presets and "key" in values:
            assert not "load_zoom_up" in values
            tileloader = presets[name](wait_after_error=wait_after_error, retries=retries, **kwargs)
        elif "uri" in values:
            del kwargs["uri"]
            layout = twm.Layout(
                crs=cosy.proj.CRS("epsg:3857"),
                tile_shape=((2 ** load_zoom_up) * 256, (2 ** load_zoom_up) * 256),
                tile_axes=cosy.geo.CompassAxes("east", "south"),
            )
            tileloader = twm.Http(values["uri"], layout=layout, wait_after_error=wait_after_error, retries=retries, header=header, **kwargs)
        else:
            tileloader = None

        # Disk
        if "path" in values:
            if tileloader is None:
                if values["path"].endswith(".yaml"):
                    tileloader = twm.Disk.from_yaml(values["path"])
                else:
                    tileloader = twm.Disk(values["path"])
            else:
                tileloader = twm.DiskCached(
                    tileloader,
                    values["path"],
                    load_zoom_up=load_zoom_up,
                )

        if "default" in values:
            tileloader = twm.WithDefault(tileloader, color=np.array(values["default"]))

        if tileloader is None:
            raise ValueError("Invalid config")
        tileloaders[name] = (tileloader, int(values["zoom"]) if "zoom" in values else 0)

    return tileloaders