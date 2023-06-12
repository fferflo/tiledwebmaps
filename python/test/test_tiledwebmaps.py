import tiledwebmaps as twm
import numpy as np
import math

def test_tile_loader():
    tile_loader = twm.Http("https://wms.openstreetmap.fr/tms/1.0.0/bayonne_2016/{zoom}/{tile_x}/{tile_y}", layout=twm.Layout.XYZ((256, 256)), wait_after_error=1.5, retries=1)

    tile = tile_loader.load((519997, 383334), 20)
    assert tile.shape[0] == 256 and tile.shape[1] == 256

    tile = tile_loader.load(latlon=(43.49111200344394, -1.4730902418166352), bearing=90.0, meters_per_pixel=0.2, shape=(512, 512), zoom=20)
    assert tile.shape[0] == 512 and tile.shape[1] == 512

    tile_loader = twm.Http("https://imagery.tnris.org/server/rest/services/StratMap/StratMap21_NCCIR_CapArea_Brazos_Kerr/ImageServer/exportImage?f=image&bbox={crs_lower_x}%2C{crs_lower_y}%2C{crs_upper_x}%2C{crs_upper_y}&imageSR=102100&bboxSR=102100&size={tile_size_x}%2C{tile_size_y}", layout=twm.Layout.XYZ((256, 256)))
    tile = tile_loader.load((479274, 863078), 21)
    assert tile.shape[0] == 256 and tile.shape[1] == 256
