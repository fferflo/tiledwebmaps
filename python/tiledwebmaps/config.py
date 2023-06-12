import yaml
import tiledwebmaps as twm

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
        if name in presets:
            kwargs = {**values}
            del kwargs["path"]
            del kwargs["zoom"]
            tileloader = presets[name](wait_after_error=wait_after_error, retries=retries, **kwargs)
        else:
            tileloader = twm.Http(values["uri"], layout=twm.Layout.XYZ((256, 256)), wait_after_error=wait_after_error, retries=retries, header=header)
        if "path" in values:
            tileloader = twm.DiskCached(tileloader, values["path"])
        tileloaders[name] = (tileloader, int(values["zoom"]))

    return tileloaders