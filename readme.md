# TiledWebMaps

[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT) [![PyPI version shields.io](https://img.shields.io/pypi/v/tiledwebmaps.svg)](https://pypi.python.org/pypi/tiledwebmaps/)

> A lightweight library for retrieving map images from a [tile provider](https://en.wikipedia.org/wiki/Tiled_web_map) with arbitrary resolution, location and bearing.

## Install

```
pip install tiledwebmaps
```

## Usage

### Example

```python
import tiledwebmaps as twm

tileloader = twm.Http("https://tiles.arcgis.com/tiles/hGdibHYSPO59RG1h/arcgis/rest" \
                      "/services/orthos2021/MapServer/tile/{zoom}/{y}/{x}") # MassGIS

image = tileloader.load(
    latlon=(42.360995, -71.051685), # Center of the image
    bearing=0.0, # Bearing pointing upwards in the image
    meters_per_pixel=0.5,
    shape=(512, 512),
    zoom=20, # Zoom level of the fetched tiles
)

import imageio
imageio.imwrite("map.jpg", image)
```

The tileloader fetches multiple tiles from [MassGIS](https://www.mass.gov/orgs/massgis-bureau-of-geographic-information) and combines and transforms them to the correct location, bearing and resolution. This creates the following image ([same location in Bing Maps](https://www.bing.com/maps/?cp=42.360995%7E-71.051683&lvl=18.5&style=a)):

<img src="images/map.jpg" width="256" height="256"/>

### Caching

:warning: **Not all tile providers allow caching or storing tiles on disk! Please check the terms of use of the tile provider before using this feature.**

Tiles can be saved on disk to avoid downloading the same tile multiple times:

```python
http_tileloader = twm.Http("https://tiles.arcgis.com/tiles/hGdibHYSPO59RG1h/arcgis/rest" \
                      "/services/orthos2021/MapServer/tile/{zoom}/{y}/{x}")
cached_tileloader = twm.DiskCached(http_tileloader, "/path/to/map/folder")
```

`cached_tileloader` will check if a tile is already present on disk before calling `http_tileloader`.

Tiles can also be cached in memory using an [LRU cache](https://en.wikipedia.org/wiki/Cache_replacement_policies#LRU):

```python
cached_tileloader = twm.LRUCached(http_tileloader, size=100)
```

## Finding tile providers

:warning: **Some tile providers like Google Maps, Bing Maps and MapBox charge payment for tile requests. We are not responsible for charges incured when using this library!**

:warning: **This library does not enforce usage limits. Make to sure to stay within the allowed quota for each tile provider!**

:warning: **Some tile providers like Google Maps and Bing Maps require attribution when using/ displaying images (see e.g. [this](https://about.google/brand-resource-center/products-and-services/geo-guidelines/#required-attribution)).**

Tile providers can for example be found at https://osmlab.github.io/editor-layer-index [1]. The url to the tile server contains placeholders that are replaced with the tile parameters when the http request is made.<br>
Some examples:

- US / **MassGIS** 2021 Aerial Imagery [1]:
    ```python
    tileloader = twm.Http("https://tiles.arcgis.com/tiles/hGdibHYSPO59RG1h/arcgis/rest/services/orthos2021/MapServer/tile/{zoom}/{y}/{x}")
    ```
- US / **StratMap** CapArea, Brazos & Kerr Imagery (Natural Color 2021) [1]:
    ```python
    tileloader = twm.Http("https://imagery.tnris.org/server/services/StratMap/StratMap21_NCCIR_CapArea_Brazos_Kerr/ImageServer/WMSServer?FORMAT=image/jpeg&VERSION=1.3.0&SERVICE=WMS&REQUEST=GetMap&LAYERS=0&STYLES=&CRS={proj}&WIDTH={width}&HEIGHT={height}&BBOX={bbox}")
    ```

- **Bing Maps**:
    ```python
    tileloader = twm.bingmaps(key="YOUR_API_KEY")
    ```
    - Accessing Bing Maps tiles requires fetching the correct url from their REST service first as outlined [here](https://learn.microsoft.com/en-us/bingmaps/rest-services/directly-accessing-the-bing-maps-tiles). We provide the utility function `twm.bingmaps` for this purpose.
    - This requires an API key as outlined [here](https://learn.microsoft.com/en-us/bingmaps/getting-started/bing-maps-dev-center-help/getting-a-bing-maps-key). Bing Maps provides API keys for educational/ non-profit use with a daily free quota. See https://www.bingmapsportal.com/Application for more information.
    - Make sure to follow Bing Map's [Terms of Use](https://www.microsoft.com/en-us/maps/product) and [usage limits](https://www.microsoft.com/en-us/maps/product#section-pst-limited-license).
- **MapBox** Raster Tiles API:
    ```python
    tileloader = twm.Http("https://api.mapbox.com/v4/mapbox.satellite/{zoom}/{x}/{y}.png?access_token=YOUR_MAPBOX_TOKEN")
    ```
    - This requires an API token as outlined [here](https://docs.mapbox.com/help/getting-started/access-tokens/). MapBox has a daily free use quota as outlined [here](https://www.mapbox.com/pricing/#tile).
    - Make sure to follow MapBox's [Terms of Service](https://www.mapbox.com/legal/tos) and [restrictions and limits](https://docs.mapbox.com/api/maps/raster-tiles/#raster-tiles-api-restrictions-and-limits).

- For map tiles already stored on disk, use:
    ```python
    tileloader = twm.Disk("/path/{zoom}/{x}/{y}.jpg")
    ```

## Notes

- The [GIL](https://en.wikipedia.org/wiki/Global_interpreter_lock) is released for all operations, such that multiple calls can be made concurrently.

## Paper
If you find this library useful for your research, please consider citing: 
```
@InProceedings{Fervers_2023_CVPR,
    author    = {Fervers, Florian and Bullinger, Sebastian and Bodensteiner, Christoph and Arens, Michael and Stiefelhagen, Rainer},
    title     = {Uncertainty-Aware Vision-Based Metric Cross-View Geolocalization},
    booktitle = {Proceedings of the IEEE/CVF Conference on Computer Vision and Pattern Recognition (CVPR)},
    month     = {June},
    year      = {2023},
    pages     = {21621-21631}
}
```