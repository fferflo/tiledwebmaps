import numpy as np
import numpy
from tiledwebmaps.backend.geo import CompassAxes

EARTH_RADIUS_METERS = 6.378137e6

def distance(latlon1, latlon2, np=numpy):
    def to_np(x):
        return np.asarray(x).astype("float64")  
    latlon1 = latlon1.astype("float64") * to_np(np.pi / 180)
    latlon2 = latlon2.astype("float64") * to_np(np.pi / 180)

    a = np.sin((latlon1[..., 0] - latlon2[..., 0]) / to_np(2)) ** 2 + np.cos(latlon1[..., 0]) * np.cos(latlon2[..., 0]) * (np.sin((latlon1[..., 1] - latlon2[..., 1]) / to_np(2)) ** 2)
    return to_np(EARTH_RADIUS_METERS * 2) * np.arctan2(np.sqrt(a), np.sqrt(to_np(1) - a))

def bearing(latlon1, latlon2):
    latlon1 = np.radians(np.asarray(latlon1).astype("float64"))
    latlon2 = np.radians(np.asarray(latlon2).astype("float64"))
    dlon = latlon2[..., 1] - latlon1[..., 1]
    x = np.cos(latlon2[..., 0]) * np.sin(dlon)
    y = np.cos(latlon1[..., 0]) * np.sin(latlon2[..., 0]) - np.sin(latlon1[..., 0]) * np.cos(latlon2[..., 0]) * np.cos(dlon)
    return np.degrees(np.arctan2(x, y))

def pixels_per_meter(latlon, zoom, tile_size):
    latlon = np.asarray(latlon).astype("float64")

    center_tile = epsg4326_to_xyz(latlon, zoom).astype("int32") # TODO: convert this to int first? Or -0.5, +0.5 below?
    tile_size_deg = np.absolute(xyz_to_epsg4326(center_tile, zoom) - xyz_to_epsg4326(center_tile + 1, zoom))
    tile_size_meter = tile_size_deg * meters_per_deg(latlon)
    return tile_size / tile_size_meter

# bearing: radians
# distance: meters
def move_from_latlon(latlon, bearing, distance):
    bearing = np.radians(bearing)
    latlon_rad = np.radians(latlon)

    angular_distance = distance / EARTH_RADIUS_METERS

    target_lat = np.arcsin(
        np.sin(latlon_rad[..., 0]) * np.cos(angular_distance) +
        np.cos(latlon_rad[..., 0]) * np.sin(angular_distance) * np.cos(bearing)
    )
    target_lon = latlon_rad[..., 1] + np.arctan2(
        np.sin(bearing) * np.sin(angular_distance) * np.cos(latlon_rad[..., 0]),
        np.cos(angular_distance) - np.sin(latlon_rad[..., 0]) * np.sin(target_lat)
    )
    while target_lon < -np.pi:
        target_lon += 2 * np.pi
    while target_lon > np.pi:
        target_lon -= 2 * np.pi

    return np.array([np.degrees(target_lat), np.degrees(target_lon)])

def meters_per_deg_at_latlon(latlon):
    distance = 1.0
    latlon2 = move_from_latlon(move_from_latlon(latlon, 90.0, distance), 0.0, distance)
    diff_deg = np.absolute(latlon - latlon2)
    return distance / diff_deg
