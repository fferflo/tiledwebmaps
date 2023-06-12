import tiledwebmaps as twm
import requests

# As described in https://learn.microsoft.com/en-us/bingmaps/rest-services/directly-accessing-the-bing-maps-tiles
def bingmaps(key, imagerySet="Aerial", **kwargs):
    # imagerySet: https://learn.microsoft.com/en-us/bingmaps/rest-services/imagery/get-imagery-metadata
    url = f"http://dev.virtualearth.net/REST/V1/Imagery/Metadata/Aerial?output=json&include=ImageryProviders&key={key}"
    response = requests.get(url)
    response.raise_for_status()
    response = response.json()["resourceSets"][0]["resources"][0]
    url = response["imageUrl"] # http://ak.dynamic.{subdomain}.tiles.virtualearth.net/comp/ch/{quadkey}?mkt=en-US&it=G,L&shading=hill&og=2210&n=z
    url = url.replace("{subdomain}", response["imageUrlSubdomains"][0])
    url = url.replace("{quadkey}", "{quad}")

    tileloader = twm.Http(url, layout=twm.Layout.XYZ((256, 256)), **kwargs)
    return tileloader
