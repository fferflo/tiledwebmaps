#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <tiledwebmaps/tiledwebmaps.h>
#include <xtensor-python/pytensor.hpp>
#include <filesystem>

namespace py = pybind11;

thread_local std::shared_ptr<cosy::proj::Context> proj_context = std::make_shared<cosy::proj::Context>();

PYBIND11_MODULE(backend, m)
{
  py::class_<tiledwebmaps::Layout, std::shared_ptr<tiledwebmaps::Layout>>(m, "Layout", py::dynamic_attr())
    .def(py::init<std::shared_ptr<cosy::proj::CRS>, xti::vec2s, cosy::geo::CompassAxes, std::optional<std::pair<xti::vec2d, xti::vec2d>>, double, bool>(),
      py::arg("crs"),
      py::arg("tile_shape"),
      py::arg("tile_axes"),
      py::arg("bounds_crs") = std::optional<std::pair<xti::vec2d, xti::vec2d>>(),
      py::arg("zoom0_scale") = 0.0,
      py::arg("use_only_first_bound_axis") = true
    )
    .def("crs_to_tile", [](const tiledwebmaps::Layout& layout, xti::vec2d coords_crs, double scale){
        return layout.crs_to_tile(coords_crs, scale);
      },
      py::arg("coords"),
      py::arg("scale")
    )
    .def("crs_to_tile", [](const tiledwebmaps::Layout& layout, xti::vec2d coords_crs, size_t zoom){
        return layout.crs_to_tile(coords_crs, zoom);
      },
      py::arg("coords"),
      py::arg("zoom")
    )
    .def("tile_to_crs", [](const tiledwebmaps::Layout& layout, xti::vec2d coords_tile, double scale){
        return layout.tile_to_crs(coords_tile, scale);
      },
      py::arg("coords"),
      py::arg("scale")
    )
    .def("tile_to_crs", [](const tiledwebmaps::Layout& layout, xti::vec2d coords_tile, size_t zoom){
        return layout.tile_to_crs(coords_tile, zoom);
      },
      py::arg("coords"),
      py::arg("zoom")
    )
    .def("tile_to_pixel", [](const tiledwebmaps::Layout& layout, xti::vec2d coords_tile, double scale){
        return layout.tile_to_pixel(coords_tile, scale);
      },
      py::arg("coords"),
      py::arg("scale")
    )
    .def("tile_to_pixel", [](const tiledwebmaps::Layout& layout, xti::vec2d coords_tile, size_t zoom){
        return layout.tile_to_pixel(coords_tile, zoom);
      },
      py::arg("coords"),
      py::arg("zoom")
    )
    .def("pixel_to_tile", [](const tiledwebmaps::Layout& layout, xti::vec2d coords_pixel, double scale){
        return layout.pixel_to_tile(coords_pixel, scale);
      },
      py::arg("coords"),
      py::arg("scale")
    )
    .def("pixel_to_tile", [](const tiledwebmaps::Layout& layout, xti::vec2d coords_pixel, size_t zoom){
        return layout.pixel_to_tile(coords_pixel, zoom);
      },
      py::arg("coords"),
      py::arg("zoom")
    )
    .def("epsg4326_to_tile", [](const tiledwebmaps::Layout& layout, xti::vec2d coords_epsg4326, double scale){
        return layout.epsg4326_to_tile(coords_epsg4326, scale);
      },
      py::arg("coords"),
      py::arg("scale")
    )
    .def("epsg4326_to_tile", [](const tiledwebmaps::Layout& layout, xti::vec2d coords_epsg4326, size_t zoom){
        return layout.epsg4326_to_tile(coords_epsg4326, zoom);
      },
      py::arg("coords"),
      py::arg("zoom")
    )
    .def("tile_to_epsg4326", [](const tiledwebmaps::Layout& layout, xti::vec2d coords_tile, double scale){
        return layout.tile_to_epsg4326(coords_tile, scale);
      },
      py::arg("coords"),
      py::arg("scale")
    )
    .def("tile_to_epsg4326", [](const tiledwebmaps::Layout& layout, xti::vec2d coords_tile, size_t zoom){
        return layout.tile_to_epsg4326(coords_tile, zoom);
      },
      py::arg("coords"),
      py::arg("zoom")
    )
    .def("epsg4326_to_pixel", [](const tiledwebmaps::Layout& layout, xti::vec2d coords_epsg4326, double scale){
        return layout.epsg4326_to_pixel(coords_epsg4326, scale);
      },
      py::arg("coords"),
      py::arg("scale")
    )
    .def("epsg4326_to_pixel", [](const tiledwebmaps::Layout& layout, xti::vec2d coords_epsg4326, size_t zoom){
        return layout.epsg4326_to_pixel(coords_epsg4326, zoom);
      },
      py::arg("coords"),
      py::arg("zoom")
    )
    .def("pixel_to_epsg4326", [](const tiledwebmaps::Layout& layout, xti::vec2d coords_pixel, double scale){
        return layout.pixel_to_epsg4326(coords_pixel, scale);
      },
      py::arg("coords"),
      py::arg("scale")
    )
    .def("pixel_to_epsg4326", [](const tiledwebmaps::Layout& layout, xti::vec2d coords_pixel, size_t zoom){
        return layout.pixel_to_epsg4326(coords_pixel, zoom);
      },
      py::arg("coords"),
      py::arg("zoom")
    )
    .def("pixels_per_meter_at_latlon", [](const tiledwebmaps::Layout& layout, xti::vec2d latlon, double scale){
        return layout.pixels_per_meter_at_latlon(latlon, scale);
      },
      py::arg("latlon"),
      py::arg("scale")
    )
    .def("pixels_per_meter_at_latlon", [](const tiledwebmaps::Layout& layout, xti::vec2d latlon, size_t zoom){
        return layout.pixels_per_meter_at_latlon(latlon, zoom);
      },
      py::arg("latlon"),
      py::arg("zoom")
    )
    .def_property_readonly("crs", &tiledwebmaps::Layout::get_crs)
    .def_property_readonly("tile_shape", [](const tiledwebmaps::Layout& layout) -> xti::vec2i {return xt::cast<int>(layout.get_tile_shape());})
    .def_property_readonly("tile_axes", &tiledwebmaps::Layout::get_tile_axes)
    .def_property_readonly("epsg4326_to_crs", &tiledwebmaps::Layout::get_epsg4326_to_crs)
    .def_static("XYZ", [](){
        return tiledwebmaps::Layout::XYZ(proj_context);
      },
      "Creates a new XYZ tiles layout.\n"
      "\n"
      "Uses the epsg:3857 map projection and axis order east-south.\n"
      "\n"
      "Returns:\n"
      "    The XYZ tiles layout."
    )
    .def_static("TMS", [](){
        return tiledwebmaps::Layout::TMS(proj_context);
      },
      "Creates a new TMS tiles layout.\n"
      "\n"
      "Uses the epsg:3857 map projection and axis order east-north.\n"
      "\n"
      "Returns:\n"
      "    The TMS tiles layout."
    )
  ;

  py::class_<tiledwebmaps::TileLoader, std::shared_ptr<tiledwebmaps::TileLoader>>(m, "TileLoader", py::dynamic_attr())
    .def("load", [](tiledwebmaps::TileLoader& tile_loader, xti::vec2s tile, size_t zoom){
        py::gil_scoped_release gil;
        return tile_loader.load(tile, zoom);
      },
      py::arg("tile"),
      py::arg("zoom")
    )
    .def("load", [](tiledwebmaps::TileLoader& tile_loader, xti::vec2s min_tile, xti::vec2s max_tile, size_t zoom){
        py::gil_scoped_release gil;
        return tiledwebmaps::load(tile_loader, min_tile, max_tile, zoom);
      },
      py::arg("min_tile"),
      py::arg("max_tile"),
      py::arg("zoom")
    )
    .def("load", [](tiledwebmaps::TileLoader& tile_loader, xti::vec2d latlon, double bearing, double meters_per_pixel, xti::vec2s shape, std::optional<size_t> zoom){
        py::gil_scoped_release gil;
        if (zoom)
        {
          return tiledwebmaps::load_metric(tile_loader, latlon, bearing, meters_per_pixel, shape, *zoom);
        }
        else
        {
          return tiledwebmaps::load_metric(tile_loader, latlon, bearing, meters_per_pixel, shape);
        }
      },
      py::arg("latlon"),
      py::arg("bearing"),
      py::arg("meters_per_pixel"),
      py::arg("shape"),
      py::arg("zoom") = std::optional<size_t>(),
      "Load an image with the given location, bearing and resolution.\n"
      "\n"
      "Parameters:\n"
      "    latlon: Latitude and longitude, center of the returned image\n"
      "    bearing: Orientation of the returned image, in degrees from north clockwise\n"
      "    meters_per_pixel: Pixel resolution in meters per pixel\n"
      "    shape: Shape of the returned image\n"
      "    zoom: Zoom level at which images are retrieved from the tileloader. If None, chooses the next zoom level above 2 * meters_per_pixel. Defaults to None.\n"
      "Returns:\n"
      "    The loaded image.\n"
    )
    .def_property_readonly("layout", &tiledwebmaps::TileLoader::get_layout)
  ;

  py::class_<tiledwebmaps::Http, std::shared_ptr<tiledwebmaps::Http>, tiledwebmaps::TileLoader>(m, "Http")
    .def(py::init([](std::string url, tiledwebmaps::Layout layout, size_t retries, float wait_after_error, bool verify_ssl, std::optional<std::string> capath, std::optional<std::string> cafile, std::map<std::string, std::string> header, bool allow_multithreading){
        if (!capath && !cafile)
        {
          auto ssl = py::module::import("ssl");
          auto default_verify_paths = ssl.attr("get_default_verify_paths")();
          std::vector<std::string> capaths;
          auto capath_py = default_verify_paths.attr("capath");
          if (!capath_py.is_none())
          {
            capaths.push_back(capath_py.cast<std::string>());
          }
          auto openssl_capath_py = default_verify_paths.attr("openssl_capath");
          if (!openssl_capath_py.is_none())
          {
            capaths.push_back(openssl_capath_py.cast<std::string>());
          }
          for (auto& capath2 : capaths)
          {
            if (std::filesystem::exists(capath2))
            {
              capath = capath2;
              break;
            }
          }
        }
        if (!capath && !cafile)
        {
          auto ssl = py::module::import("ssl");
          auto default_verify_paths = ssl.attr("get_default_verify_paths")();
          std::vector<std::string> cafiles;
          auto cafile_py = default_verify_paths.attr("cafile");
          if (!cafile_py.is_none())
          {
            cafiles.push_back(cafile_py.cast<std::string>());
          }
          auto openssl_cafile_py = default_verify_paths.attr("openssl_cafile");
          if (!openssl_cafile_py.is_none())
          {
            cafiles.push_back(openssl_cafile_py.cast<std::string>());
          }
          for (auto& cafile2 : cafiles)
          {
            if (std::filesystem::exists(cafile2))
            {
              cafile = cafile2;
              break;
            }
          }
        }
        return tiledwebmaps::Http(url, layout, retries, wait_after_error, verify_ssl, capath, cafile, header, allow_multithreading);
      }),
      py::arg("url"),
      py::arg("layout") = tiledwebmaps::Layout::XYZ(proj_context),
      py::arg("retries") = 10,
      py::arg("wait_after_error") = 1.5,
      py::arg("verify_ssl") = true,
      py::arg("capath") = std::optional<std::string>(),
      py::arg("cafile") = std::optional<std::string>(),
      py::arg("header") = std::map<std::string, std::string>(),
      py::arg("allow_multithreading") = false,
      "Create an Http tileloader that loads images from the given url.\n"
      "\n"
      "The url can contain the following placeholders that will be replaced by the parameters of the loaded tile:\n"
      "    {crs_lower_x} {crs_lower_y}: Lower corner of the tile in the crs of the tileloader\n"
      "    {crs_upper_x} {crs_upper_y}: Upper corner of the tile in the crs of the tileloader\n"
      "    {crs_center_x} {crs_center_y}: Center of the tile in the crs of the tileloader\n"
      "    {crs_size_x} {crs_size_y}: Size of the tile in the crs of the tileloader\n"
      "    {px_lower_x} {px_lower_y}: Lower corner of the tile in pixels\n"
      "    {px_upper_x} {px_upper_y}: Upper corner of the tile in pixels\n"
      "    {px_center_x} {px_center_y}: Center of the tile in pixels\n"
      "    {px_size_x} {px_size_y}: Size of the tile in pixels\n"
      "    {tile_lower_x} {tile_lower_y}: Lower corner of the tile in tile coordinates at the given zoom level\n"
      "    {tile_upper_x} {tile_upper_y}: Upper corner of the tile in tile coordinates at the given zoom level (equal to lower corner plus 1)\n"
      "    {tile_center_x} {tile_center_y}: Center of the tile in tile coordinates at the given zoom level (equal to lower corner plus 0.5)\n"
      "    {lat_lower} {lon_lower}: Lower corner of the tile as latitude and longitude\n"
      "    {lat_upper} {lon_upper}: Upper corner of the tile as latitude and longitude\n"
      "    {lat_center} {lon_center}: Center of the tile as latitude and longitude\n"
      "    {lat_size} {lon_size}: Size of the tile as latitude and longitude\n"
      "    {zoom}: Zoom level of the tile\n"
      "    {quad}: Tile identifier defined by Bing Maps\n"
      "    {crs}: String id of the crs\n"
      "    {x}: Alias for {tile_lower_x}\n"
      "    {y}: Alias for {tile_lower_y}\n"
      "    {z}: Alias for {zoom}\n"
      "    {width}: Alias for {px_size_x}\n"
      "    {height}: Alias for {px_size_y}\n"
      "    {proj}: Alias for {crs}\n"
      "    {bbox}: Alias for {crs_lower_x},{crs_lower_y},{crs_upper_x},{crs_upper_y}"
      "\n"
      "Parameters:\n"
      "    url: The string url with placeholders.\n"
      "    layout: The layout of the tiles loaded by this tileloader. Defaults to tiledwebmaps.Layout.XYZ().\n"
      "    retries: Number of times that the http request will be retried before throwing an error. Defaults to 10.\n"
      "    wait_after_error: Seconds to wait before retrying the http request. Defaults to 1.5.\n"
      "    verify_ssl: Whether to verify the ssl host/peer. Defaults to True.\n"
      "    capath: Set the capath of the curl request if given. Defaults to None.\n"
      "    cafile: Set the cafile of the curl request if given. Defaults to None.\n"
      "    header: Header of the curl request. Defaults to {}.\n"
      "    allow_multithreading: True if multiple threads are allowed to use this tileloader concurrently. Defaults to False.\n"
      "\n"
      "Returns:\n"
      "    The created Http tileloader.\n"
    )
  ;

  py::class_<tiledwebmaps::Cache, std::shared_ptr<tiledwebmaps::Cache>, tiledwebmaps::TileLoader>(m, "Cache")
    .def_property_readonly("layout", [](const tiledwebmaps::Cache& cache){return cache.get_layout();})
    .def("save", &tiledwebmaps::Cache::save,
      py::arg("image"),
      py::arg("tile"),
      py::arg("zoom")
    )
    .def("contains", &tiledwebmaps::Cache::contains,
      py::arg("tile"),
      py::arg("zoom")
    )
  ;
  py::class_<tiledwebmaps::CachedTileLoader, std::shared_ptr<tiledwebmaps::CachedTileLoader>, tiledwebmaps::TileLoader>(m, "CachedTileLoader", py::dynamic_attr())
    .def(py::init<std::shared_ptr<tiledwebmaps::TileLoader>, std::shared_ptr<tiledwebmaps::Cache>>())
    .def_property_readonly("cache", &tiledwebmaps::CachedTileLoader::get_cache)
  ;

  py::class_<tiledwebmaps::Disk, std::shared_ptr<tiledwebmaps::Disk>, tiledwebmaps::Cache>(m, "Disk", py::dynamic_attr())
    .def(py::init<std::string, tiledwebmaps::Layout, float>(),
      py::arg("path"),
      py::arg("layout") = tiledwebmaps::Layout::XYZ(proj_context),
      py::arg("wait_after_last_modified") = 1.0,
      "Returns a new tileloader that loads tiles from disk.\n"
      "\n"
      "Parameters:\n"
      "    path: The path to the saved tiles, including placeholders. If it does not include placeholders, appends \"/zoom/x/y.jpg\".\n"
      "    layout: The layout of the tiles loaded by this tileloader. Defaults to tiledwebmaps.Layout.XYZ().\n"
      "    wait_after_last_modified: Waits this many seconds after the last modification of a tile before loading it. Defaults to 1.0.\n"
      "\n"
      "Returns:\n"
      "    A new tileloader that loads tiles from disk.\n"
    )
    .def_property_readonly("path", [](const tiledwebmaps::Disk& disk){return disk.get_path().string();})
  ;
  m.def("DiskCached", [](std::shared_ptr<tiledwebmaps::TileLoader> loader, std::string path, float wait_after_last_modified, int load_zoom_up){
      int factor = (1 << load_zoom_up);
      tiledwebmaps::Layout load_layout = loader->get_layout();
      tiledwebmaps::Layout cache_layout(
        load_layout.get_crs(),
        load_layout.get_epsg4326_to_crs(),
        load_layout.get_tile_shape() / factor,
        load_layout.get_tile_axes(),
        load_layout.get_bounds_crs(),
        load_layout.get_zoom0_scale(),
        false
      );
      return std::make_shared<tiledwebmaps::CachedTileLoader>(loader, std::make_shared<tiledwebmaps::Disk>(path, cache_layout, wait_after_last_modified));
    },
    py::arg("loader"),
    py::arg("path"),
    py::arg("wait_after_last_modified") = 1.0,
    py::arg("load_zoom_up") = 0,
    "Returns a new tileloader that caches tiles from the given tileloader on disk.\n"
    "\n"
    "Parameters:\n"
    "    loader: The tileloader whose tiles will be cached.\n"
    "    path: The path to where the cached tiles will be saved, including placeholders. If it does not include placeholders, appends \"/zoom/x/y.jpg\".\n"
    "    wait_after_last_modified: Waits this many seconds after the last modification of a tile before loading it. Defaults to 1.0.\n"
    "    load_zoom_up: Load tiles at this many zoom levels coarser on cache failure. Defaults to 0.\n"
    "\n"
    "Returns:\n"
    "    A new tileloader that caches tiles from the given tileloader on disk.\n"
  );
  py::class_<tiledwebmaps::LRU, std::shared_ptr<tiledwebmaps::LRU>, tiledwebmaps::Cache>(m, "LRU", py::dynamic_attr())
    .def(py::init<tiledwebmaps::Layout, int>(),
      py::arg("layout"),
      py::arg("size")
    )
  ;
  m.def("LRUCached", [](std::shared_ptr<tiledwebmaps::TileLoader> loader, int size){
      return std::make_shared<tiledwebmaps::CachedTileLoader>(loader, std::make_shared<tiledwebmaps::LRU>(loader->get_layout(), size));
    },
    py::arg("loader"),
    py::arg("size"),
    "Returns a new tileloader that caches tiles from the given tileloader in a LRU (least-recently-used) cache.\n"
    "\n"
    "Parameters:\n"
    "    loader: The tileloader whose tiles will be cached.\n"
    "    size: The maximum number of tiles that will be cached.\n"
    "\n"
    "Returns:\n"
    "    A new tileloader that caches tiles from the given tileloader in a LRU cache.\n"
  );
  py::class_<tiledwebmaps::WithDefault, std::shared_ptr<tiledwebmaps::WithDefault>, tiledwebmaps::TileLoader>(m, "WithDefault", py::dynamic_attr())
    .def(py::init<std::shared_ptr<tiledwebmaps::Cache>, xti::vec3i>(),
      py::arg("loader"),
      py::arg("color") = xti::vec3i({255, 255, 255}),
      "Returns a new tileloader that returns default tiles with the given color if the given tileloader does not contain a tile.\n"
      "\n"
      "Parameters:\n"
      "    loader: The tileloader whose tiles will be returned if they exist.\n"
      "    color: Color that the default tile will be filled with. Defaults to [255, 255, 255].\n"
      "\n"
      "Returns:\n"
      "    A new tileloader that returns default tiles if the given tileloader does not contain a tile.\n"
    )
  ;


  py::register_exception<tiledwebmaps::LoadTileException>(m, "LoadTileException");
  py::register_exception<tiledwebmaps::WriteFileException>(m, "WriteFileException");
  py::register_exception<tiledwebmaps::LoadFileException>(m, "LoadFileException");
  py::register_exception<tiledwebmaps::FileNotFoundException>(m, "FileNotFoundException");
}
