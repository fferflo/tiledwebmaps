#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <tiledwebmaps/tiledwebmaps.h>
#include <tiledwebmaps/python.h>
#include <xtensor-python/pytensor.hpp>
#include <filesystem>

namespace py = pybind11;

thread_local std::shared_ptr<tiledwebmaps::proj::Context> proj_context = std::make_shared<tiledwebmaps::proj::Context>();

thread_local std::shared_ptr<tiledwebmaps::proj::Transformer> epsg4326_to_epsg3857 = std::make_shared<tiledwebmaps::proj::Transformer>(
  std::make_shared<tiledwebmaps::proj::CRS>(proj_context, "epsg:4326"),
  std::make_shared<tiledwebmaps::proj::CRS>(proj_context, "epsg:3857")
);
thread_local std::shared_ptr<tiledwebmaps::proj::Transformer> epsg3857_to_epsg4326 = epsg4326_to_epsg3857->inverse();

PYBIND11_MODULE(backend, m)
{
  // **********************************************************************************************
  // ******************************************** PROJ ********************************************
  // **********************************************************************************************
  auto proj = m.def_submodule("proj");

  py::class_<tiledwebmaps::proj::CRS, std::shared_ptr<tiledwebmaps::proj::CRS>>(proj, "CRS")
    .def(py::init([](std::string desc){
        return std::make_shared<tiledwebmaps::proj::CRS>(proj_context, desc);
      }),
      py::arg("desc")
    )
    .def("get_vector", &tiledwebmaps::proj::CRS::get_vector,
      py::arg("name")
    )
    .def_property_readonly("area_of_use", [](const tiledwebmaps::proj::CRS& crs){
        auto area_of_use = crs.get_area_of_use();
        return std::make_pair(area_of_use.lower_latlon, area_of_use.upper_latlon);
      }
    )
    .def(py::pickle(
      [](const tiledwebmaps::proj::CRS& x){ // __getstate__
        return py::make_tuple(
          x.get_description()
        );
      },
      [](py::tuple t){ // __setstate__
        if (t.size() != 1)
        {
          throw std::runtime_error("Invalid state");
        }
        return std::make_shared<tiledwebmaps::proj::CRS>(
          proj_context,
          t[0].cast<std::string>()
        );
      }
    ))
  ;

  py::class_<tiledwebmaps::proj::Transformer, std::shared_ptr<tiledwebmaps::proj::Transformer>>(proj, "Transformer")
    .def(py::init([](std::string from_crs, std::string to_crs){
        return std::make_shared<tiledwebmaps::proj::Transformer>(proj_context, from_crs, to_crs);
      }),
      py::arg("from_crs"),
      py::arg("to_crs")
    )
    .def(py::init([](std::shared_ptr<tiledwebmaps::proj::CRS> from_crs, std::string to_crs){
        return std::make_shared<tiledwebmaps::proj::Transformer>(proj_context, from_crs, to_crs);
      }),
      py::arg("from_crs"),
      py::arg("to_crs")
    )
    .def(py::init([](std::string from_crs, std::shared_ptr<tiledwebmaps::proj::CRS> to_crs){
        return std::make_shared<tiledwebmaps::proj::Transformer>(proj_context, from_crs, to_crs);
      }),
      py::arg("from_crs"),
      py::arg("to_crs")
    )
    .def(py::init([](std::shared_ptr<tiledwebmaps::proj::CRS> from_crs, std::shared_ptr<tiledwebmaps::proj::CRS> to_crs){
        return std::make_shared<tiledwebmaps::proj::Transformer>(proj_context, from_crs, to_crs);
      }),
      py::arg("from_crs"),
      py::arg("to_crs")
    )
    .def("transform", [](const tiledwebmaps::proj::Transformer& transformer, xti::vec2d coords){
        return transformer.transform(coords);
      },
      py::arg("coords")
    )
    .def("transform_inverse", [](const tiledwebmaps::proj::Transformer& transformer, xti::vec2d coords){
        return transformer.transform_inverse(coords);
      },
      py::arg("coords")
    )
    .def("transform_angle", [](const tiledwebmaps::proj::Transformer& transformer, double angle){
        return transformer.transform_angle(angle);
      },
      py::arg("angle")
    )
    .def("transform_angle_inverse", [](const tiledwebmaps::proj::Transformer& transformer, double angle){
        return transformer.transform_angle_inverse(angle);
      },
      py::arg("angle")
    )
    .def("__call__", [](const tiledwebmaps::proj::Transformer& transformer, xti::vec2d coords){
        return transformer.transform(coords);
      },
      py::arg("coords")
    )
    .def("inverse", &tiledwebmaps::proj::Transformer::inverse)
    .def_property_readonly("from_crs", &tiledwebmaps::proj::Transformer::get_from_crs)
    .def_property_readonly("to_crs", &tiledwebmaps::proj::Transformer::get_to_crs)
    .def(py::pickle(
      [](const tiledwebmaps::proj::Transformer& x){ // __getstate__
        return py::make_tuple(
          x.get_from_crs(),
          x.get_to_crs()
        );
      },
      [](py::tuple t){ // __setstate__
        if (t.size() != 2)
        {
          throw std::runtime_error("Invalid state");
        }
        return std::make_shared<tiledwebmaps::proj::Transformer>(
          proj_context,
          t[0].cast<std::shared_ptr<tiledwebmaps::proj::CRS>>(),
          t[1].cast<std::shared_ptr<tiledwebmaps::proj::CRS>>()
        );
      }
    ))
  ;

  proj.def("eastnorthmeters_at_latlon_to_epsg3857", [](xti::vec2d latlon){
      return tiledwebmaps::proj::eastnorthmeters_at_latlon_to_epsg3857(latlon, *epsg4326_to_epsg3857).to_matrix();
    },
    py::arg("latlon")
  );
  proj.def("geopose_to_epsg3857", [](xti::vec2d latlon, double bearing){
      return tiledwebmaps::proj::geopose_to_epsg3857(latlon, bearing, *epsg4326_to_epsg3857).to_matrix();
    },
    py::arg("latlon"),
    py::arg("bearing")
  );

  proj.attr("__setattr__")("epsg4326_to_epsg3857", epsg4326_to_epsg3857);
  proj.attr("__setattr__")("epsg3857_to_epsg4326", epsg3857_to_epsg4326);




  // **********************************************************************************************
  // ***************************************** TILEDWEBMAPS ***************************************
  // **********************************************************************************************
  py::class_<tiledwebmaps::NamedAxes<2>>(m, "NamedAxes2")
    .def(py::init([](std::pair<std::string, std::string> axis1, std::pair<std::string, std::string> axis2){
        return tiledwebmaps::NamedAxes<2>({{axis1.first, axis1.second}, {axis2.first, axis2.second}});
      }),
      py::arg("axis1"),
      py::arg("axis2")
    )
    .def("__getitem__", [](const tiledwebmaps::NamedAxes<2>& axes, size_t index){return axes[index];})
  ;
  m.def("NamedAxesTransformation", [](tiledwebmaps::NamedAxes<2> axes1, tiledwebmaps::NamedAxes<2> axes2) -> tiledwebmaps::Rotation<double, 2>{
    return tiledwebmaps::NamedAxesTransformation<double, 2>(axes1, axes2);
  });

  auto geo = m.def_submodule("geo");
  py::class_<tiledwebmaps::geo::CompassAxes, tiledwebmaps::NamedAxes<2>>(geo, "CompassAxes")
    .def(py::init<std::string, std::string>(),
      py::arg("axis1"),
      py::arg("axis2")
    )
    .def_property_readonly("axis1", [](const tiledwebmaps::geo::CompassAxes& axes){return axes[0];})
    .def_property_readonly("axis2", [](const tiledwebmaps::geo::CompassAxes& axes){return axes[1];})
  ;

  py::class_<tiledwebmaps::Layout, std::shared_ptr<tiledwebmaps::Layout>>(m, "Layout", py::dynamic_attr())
    .def(py::init<std::shared_ptr<tiledwebmaps::proj::CRS>, xti::vec2i, xti::vec2d, xti::vec2d, std::optional<xti::vec2d>, tiledwebmaps::geo::CompassAxes>(),
      py::arg("crs") = std::make_shared<tiledwebmaps::proj::CRS>("epsg:3857"),
      py::arg("tile_shape_px") = xti::vec2i({256, 256}),
      py::arg("tile_shape_crs") = xti::vec2d({1.0, 1.0}),
      py::arg("origin_crs") = xti::vec2d({0.0, 0.0}),
      py::arg("size_crs") = std::optional<xti::vec2d>(),
      py::arg("tile_axes") = tiledwebmaps::geo::CompassAxes("east", "south")
    )
    .def("crs_to_tile", [](const tiledwebmaps::Layout& layout, xti::vec2d coords_crs, double scale){
        return layout.crs_to_tile(coords_crs, scale);
      },
      py::arg("coords"),
      py::arg("scale")
    )
    .def("crs_to_tile", [](const tiledwebmaps::Layout& layout, xti::vec2d coords_crs, int zoom){
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
    .def("tile_to_crs", [](const tiledwebmaps::Layout& layout, xti::vec2d coords_tile, int zoom){
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
    .def("tile_to_pixel", [](const tiledwebmaps::Layout& layout, xti::vec2d coords_tile, int zoom){
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
    .def("pixel_to_tile", [](const tiledwebmaps::Layout& layout, xti::vec2d coords_pixel, int zoom){
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
    .def("epsg4326_to_tile", [](const tiledwebmaps::Layout& layout, xti::vec2d coords_epsg4326, int zoom){
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
    .def("tile_to_epsg4326", [](const tiledwebmaps::Layout& layout, xti::vec2d coords_tile, int zoom){
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
    .def("epsg4326_to_pixel", [](const tiledwebmaps::Layout& layout, xt::xarray<double> coords_epsg4326, int zoom){
        if (coords_epsg4326.dimension() == 1)
        {
          coords_epsg4326 = layout.epsg4326_to_pixel(coords_epsg4326, zoom);
        }
        else if (coords_epsg4326.dimension() == 2)
        {
          if (coords_epsg4326.shape()[1] != 2)
          {
            throw std::runtime_error("coords must be a 1D or 2D array with 2 columns");
          }
          for (auto i = 0; i < coords_epsg4326.shape()[0]; ++i)
          {
            xti::vec2d x({coords_epsg4326(i, 0), coords_epsg4326(i, 1)});
            x = layout.epsg4326_to_pixel(x, zoom);
            coords_epsg4326(i, 0) = x[0];
            coords_epsg4326(i, 1) = x[1];
          }
        }
        else
        {
          throw std::runtime_error("coords must be a 1D or 2D array");
        }
        return coords_epsg4326;
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
    .def("pixel_to_epsg4326", [](const tiledwebmaps::Layout& layout, xt::xarray<double> coords_pixel, int zoom){
        if (coords_pixel.dimension() == 1)
        {
          coords_pixel = layout.pixel_to_epsg4326(coords_pixel, zoom);
        }
        else if (coords_pixel.dimension() == 2)
        {
          if (coords_pixel.shape()[1] != 2)
          {
            throw std::runtime_error("coords must be a 1D or 2D array with 2 columns");
          }
          for (auto i = 0; i < coords_pixel.shape()[0]; ++i)
          {
            xti::vec2d x({coords_pixel(i, 0), coords_pixel(i, 1)});
            x = layout.pixel_to_epsg4326(x, zoom);
            coords_pixel(i, 0) = x[0];
            coords_pixel(i, 1) = x[1];
          }
        }
        else
        {
          throw std::runtime_error("coords must be a 1D or 2D array");
        }
        return coords_pixel;
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
    .def("pixels_per_meter_at_latlon", [](const tiledwebmaps::Layout& layout, xti::vec2d latlon, int zoom){
        return layout.pixels_per_meter_at_latlon(latlon, zoom);
      },
      py::arg("latlon"),
      py::arg("zoom")
    )
    .def("get_meridian_convergence", &tiledwebmaps::Layout::get_meridian_convergence,
      py::arg("latlon")
    )
    .def_property_readonly("crs", &tiledwebmaps::Layout::get_crs)
    .def_property_readonly("tile_shape_px", &tiledwebmaps::Layout::get_tile_shape_px)
    .def_property_readonly("tile_shape_crs", &tiledwebmaps::Layout::get_tile_shape_crs)
    .def_property_readonly("origin_crs", &tiledwebmaps::Layout::get_origin_crs)
    .def_property_readonly("size_crs", &tiledwebmaps::Layout::get_size_crs)
    .def_property_readonly("tile_axes", &tiledwebmaps::Layout::get_tile_axes)
    .def_property_readonly("epsg4326_to_crs", &tiledwebmaps::Layout::get_epsg4326_to_crs)
    .def_property_readonly("crs_to_epsg4326", [](const tiledwebmaps::Layout& layout){return layout.get_epsg4326_to_crs()->inverse();})
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
  ;

  py::class_<tiledwebmaps::TileLoader, std::shared_ptr<tiledwebmaps::TileLoader>>(m, "TileLoader", py::dynamic_attr())
    .def("load", [](tiledwebmaps::TileLoader& tile_loader, xti::vec2s tile, int zoom){
        py::gil_scoped_release gil;
        cv::Mat image = tile_loader.load(tile, zoom);
        xt::xtensor<uint8_t, 3> image2 = xti::from_opencv<uint8_t>(image);
        return image2;
      },
      py::arg("tile"),
      py::arg("zoom")
    )
    .def("load", [](tiledwebmaps::TileLoader& tile_loader, xti::vec2s min_tile, xti::vec2s max_tile, int zoom){
        py::gil_scoped_release gil;
        cv::Mat image = tiledwebmaps::load(tile_loader, min_tile, max_tile, zoom);
        xt::xtensor<uint8_t, 3> image2 = xti::from_opencv<uint8_t>(image);
        return image2;
      },
      py::arg("min_tile"),
      py::arg("max_tile"),
      py::arg("zoom")
    )
    .def("load", [](tiledwebmaps::TileLoader& tile_loader, xti::vec2d latlon, double bearing, double meters_per_pixel, xti::vec2s shape, std::optional<int> zoom){
        py::gil_scoped_release gil;
        cv::Mat image;
        if (zoom)
        {
          image = tiledwebmaps::load_metric(tile_loader, latlon, bearing, meters_per_pixel, shape, *zoom);
        }
        else
        {
          image = tiledwebmaps::load_metric(tile_loader, latlon, bearing, meters_per_pixel, shape);
        }
        xt::xtensor<uint8_t, 3> image2 = xti::from_opencv<uint8_t>(image);
        return image2;
      },
      py::arg("latlon"),
      py::arg("bearing"),
      py::arg("meters_per_pixel"),
      py::arg("shape"),
      py::arg("zoom") = std::optional<int>(),
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
    .def("make_forksafe", &tiledwebmaps::TileLoader::make_forksafe)
    .def("get_zoom", &tiledwebmaps::TileLoader::get_zoom,
      py::arg("latlon"),
      py::arg("meters_per_pixel")
    )
    .def_property_readonly("max_zoom", &tiledwebmaps::TileLoader::get_max_zoom)
    .def_property_readonly("min_zoom", &tiledwebmaps::TileLoader::get_min_zoom)
  ;

  py::class_<tiledwebmaps::Http, std::shared_ptr<tiledwebmaps::Http>, tiledwebmaps::TileLoader>(m, "Http")
    .def(py::init([](std::string url, tiledwebmaps::Layout layout, int min_zoom, int max_zoom, int retries, float wait_after_error, bool verify_ssl, std::optional<std::string> capath, std::optional<std::string> cafile, std::map<std::string, std::string> header, bool allow_multithreading){
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
        return tiledwebmaps::Http(url, layout, min_zoom, max_zoom, retries, wait_after_error, verify_ssl, capath, cafile, header, allow_multithreading);
      }),
      py::arg("url"),
      py::arg("layout"),
      py::arg("min_zoom"),
      py::arg("max_zoom"),
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
      "    min_zoom: The minimum zoom level that the tileloader will load.\n"
      "    max_zoom: The maximum zoom level that the tileloader will load.\n"
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

  py::class_<tiledwebmaps::Bin, std::shared_ptr<tiledwebmaps::Bin>, tiledwebmaps::TileLoader>(m, "Bin")
    .def(py::init([](std::string path, tiledwebmaps::Layout layout){
        return tiledwebmaps::Bin(path, layout);
      }),
      py::arg("path"),
      py::arg("layout") = tiledwebmaps::Layout::XYZ(proj_context)
    )
  ;

  py::class_<tiledwebmaps::Cache, std::shared_ptr<tiledwebmaps::Cache>>(m, "Cache")
    .def("load", &tiledwebmaps::Cache::load,
      py::arg("tile"),
      py::arg("zoom")
    )
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

  py::class_<tiledwebmaps::Disk, std::shared_ptr<tiledwebmaps::Disk>, tiledwebmaps::TileLoader, tiledwebmaps::Cache>(m, "Disk", py::dynamic_attr())
    .def(py::init<std::string, tiledwebmaps::Layout, int, int, float>(),
      py::arg("path"),
      py::arg("layout"),
      py::arg("min_zoom"),
      py::arg("max_zoom"),
      py::arg("wait_after_last_modified") = 1.0,
      "Returns a new tileloader that loads tiles from disk.\n"
      "\n"
      "Parameters:\n"
      "    path: The path to the saved tiles, including placeholders. If it does not include placeholders, appends \"/zoom/x/y.jpg\".\n"
      "    layout: The layout of the tiles loaded by this tileloader. Defaults to tiledwebmaps.Layout.XYZ().\n"
      "    min_zoom: The minimum zoom level that the tileloader will load.\n"
      "    max_zoom: The maximum zoom level that the tileloader will load.\n"
      "    wait_after_last_modified: Waits this many seconds after the last modification of a tile before loading it. Defaults to 1.0.\n"
      "\n"
      "Returns:\n"
      "    A new tileloader that loads tiles from disk.\n"
    )
    .def_property_readonly("path", [](const tiledwebmaps::Disk& disk){return disk.get_path().string();})
  ;
  m.def("DiskCached", [](std::shared_ptr<tiledwebmaps::TileLoader> loader, std::string path, float wait_after_last_modified){
      return std::make_shared<tiledwebmaps::CachedTileLoader>(loader, std::make_shared<tiledwebmaps::Disk>(path, loader->get_layout(), loader->get_min_zoom(), loader->get_max_zoom(), wait_after_last_modified));
    },
    py::arg("loader"),
    py::arg("path"),
    py::arg("wait_after_last_modified") = 1.0,
    "Returns a new tileloader that caches tiles from the given tileloader on disk.\n"
    "\n"
    "Parameters:\n"
    "    loader: The tileloader whose tiles will be cached.\n"
    "    path: The path to where the cached tiles will be saved, including placeholders. If it does not include placeholders, appends \"/zoom/x/y.jpg\".\n"
    "    wait_after_last_modified: Waits this many seconds after the last modification of a tile before loading it. Defaults to 1.0.\n"
    "\n"
    "Returns:\n"
    "    A new tileloader that caches tiles from the given tileloader on disk.\n"
  );
  py::class_<tiledwebmaps::LRU, std::shared_ptr<tiledwebmaps::LRU>, tiledwebmaps::Cache>(m, "LRU", py::dynamic_attr())
    .def(py::init<int>(),
      py::arg("size")
    )
  ;
  m.def("LRUCached", [](std::shared_ptr<tiledwebmaps::TileLoader> loader, int size){
      return std::make_shared<tiledwebmaps::CachedTileLoader>(loader, std::make_shared<tiledwebmaps::LRU>(size));
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
    .def(py::init<std::shared_ptr<tiledwebmaps::TileLoader>, xti::vec3i>(),
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
