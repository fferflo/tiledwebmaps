#pragma once

#include <xti/typedefs.h>
#include <xti/opencv.h>
#include <cosy/proj.h>
#include <cosy/affine.h>
#include <xtensor/xmath.hpp>
#include <xtensor/xindex_view.hpp>
#include <utility>

namespace tiledwebmaps {

static const cosy::geo::CompassAxes pixel_axes("south", "east");

// TODO: all crs should be epsg:3857?
class Layout
{
public:
  // https://gist.github.com/tmcw/4954720
  static Layout XYZ(std::shared_ptr<cosy::proj::Context> proj_context, xti::vec2s tile_shape = xti::vec2s({256, 256}))
  {
    return Layout(std::make_shared<cosy::proj::CRS>(proj_context, "epsg:3857"), tile_shape, cosy::geo::CompassAxes("east", "south"), true);
  }

  // https://gist.github.com/tmcw/4954720
  static Layout TMS(std::shared_ptr<cosy::proj::Context> proj_context, xti::vec2s tile_shape = xti::vec2s({256, 256}))
  {
    return Layout(std::make_shared<cosy::proj::CRS>(proj_context, "epsg:3857"), tile_shape, cosy::geo::CompassAxes("east", "north"), true);
  }

  Layout(std::shared_ptr<cosy::proj::CRS> crs, xti::vec2s tile_shape, cosy::geo::CompassAxes tile_axes, bool use_only_first_bound_axis = true)
    : Layout(crs, std::make_shared<cosy::proj::Transformer>(std::make_shared<cosy::proj::CRS>(crs->get_context(), "epsg:4326"), crs), tile_shape, tile_axes, use_only_first_bound_axis)
  {
  }

  Layout(std::shared_ptr<cosy::proj::CRS> crs, std::shared_ptr<cosy::proj::Transformer> epsg4326_to_crs, xti::vec2s tile_shape, cosy::geo::CompassAxes tile_axes, bool use_only_first_bound_axis = true)
    : m_crs(crs)
    , m_epsg4326_to_crs(std::move(epsg4326_to_crs))
    , m_tile_shape(tile_shape)
    , m_tile_axes(tile_axes)
    , m_crs_to_tile_axes(crs->get_axes(), tile_axes)
    , m_tile_to_pixel_axes(m_tile_axes, pixel_axes)
  {
    auto area_of_use = m_crs->get_area_of_use();
    xti::vec2d lower_bound = m_epsg4326_to_crs->transform(area_of_use.lower_latlon);
    xti::vec2d upper_bound = m_epsg4326_to_crs->transform(area_of_use.upper_latlon);
    if (use_only_first_bound_axis)
    {
      lower_bound(1) = lower_bound(0);
      upper_bound(1) = upper_bound(0);
    }

    m_lower_bound = lower_bound;
    m_diff_bound = upper_bound - lower_bound;
  }

  const std::shared_ptr<cosy::proj::Transformer>& get_epsg4326_to_crs() const
  {
    return m_epsg4326_to_crs;
  }

  xti::vec2d epsg4326_to_crs(xti::vec2d coords_epsg4326) const
  {
    return m_epsg4326_to_crs->transform(coords_epsg4326);
  }

  xti::vec2d crs_to_epsg4326(xti::vec2d coords_crs) const
  {
    return m_epsg4326_to_crs->transform_inverse(coords_crs);
  }

  xti::vec2d crs_to_tile(xti::vec2d coords_crs, size_t zoom) const
  {
    xti::vec2d coords_tile = coords_crs;
    coords_tile = (coords_crs - m_lower_bound) / m_diff_bound;
    coords_tile = m_crs_to_tile_axes.transform(coords_tile);
    xt::filter(coords_tile, coords_tile < 0) += 1;
    coords_tile = coords_tile * (1 << zoom);
    return coords_tile;
  }

  xti::vec2d tile_to_crs(xti::vec2d coords_tile, size_t zoom) const
  {
    xti::vec2d coords_crs = coords_tile;
    coords_crs = coords_crs / (1 << zoom);
    coords_crs = m_crs_to_tile_axes.transform_inverse(coords_crs);
    xt::filter(coords_crs, coords_crs < 0) += 1;
    coords_crs = coords_crs * m_diff_bound + m_lower_bound;
    return coords_crs;
  }

  xti::vec2d tile_to_pixel(xti::vec2d coords_tile, size_t zoom) const
  {
    xti::vec2d coords_pixel = coords_tile;
    coords_pixel = coords_pixel * m_tile_shape;
    coords_pixel = m_tile_to_pixel_axes.transform(coords_pixel);
    xt::filter(coords_pixel, coords_pixel < 0) += (1 << zoom) - 1;
    return coords_pixel;
  }

  xti::vec2d pixel_to_tile(xti::vec2d coords_pixel, size_t zoom) const
  {
    xti::vec2d coords_tile = coords_pixel;
    coords_tile = m_tile_to_pixel_axes.transform_inverse(coords_tile);
    coords_tile = coords_tile / m_tile_shape;
    xt::filter(coords_tile, coords_tile < 0) += (1 << zoom) - 1;
    return coords_tile;
  }

  xti::vec2d epsg4326_to_tile(xti::vec2d coords_epsg4326, size_t zoom) const
  {
    return crs_to_tile(epsg4326_to_crs(coords_epsg4326), zoom);
  }

  xti::vec2d tile_to_epsg4326(xti::vec2d coords_tile, size_t zoom) const
  {
    return crs_to_epsg4326(tile_to_crs(coords_tile, zoom));
  }

  xti::vec2d epsg4326_to_pixel(xti::vec2d coords_epsg4326, size_t zoom) const
  {
    return tile_to_pixel(epsg4326_to_tile(coords_epsg4326, zoom), zoom);
  }

  xti::vec2d pixel_to_epsg4326(xti::vec2d coords_pixel, size_t zoom) const
  {
    return tile_to_epsg4326(pixel_to_tile(coords_pixel, zoom), zoom);
  }

  double pixels_per_meter_at_latlon(xti::vec2d latlon, size_t zoom) const
  {
    xti::vec2d center_tile = epsg4326_to_tile(latlon, zoom);
    xti::vec2d tile_size_deg = xt::abs(tile_to_epsg4326(center_tile + 0.5, zoom) - tile_to_epsg4326(center_tile - 0.5, zoom));
    xti::vec2d tile_size_meter = tile_size_deg * cosy::geo::meters_per_deg_at_latlon(latlon);
    xti::vec2d pixels_per_meter = xt::abs(m_tile_to_pixel_axes.transform(m_tile_shape / tile_size_meter));
    return pixels_per_meter(1);
  }

  std::shared_ptr<cosy::proj::CRS> get_crs() const
  {
    return m_crs;
  }

  xti::vec2s get_tile_shape() const
  {
    return m_tile_shape;
  }

  cosy::geo::CompassAxes get_tile_axes() const
  {
    return m_tile_axes;
  }

  bool operator==(const Layout& other) const
  {
    return *this->m_crs == *other.m_crs && this->m_tile_shape == other.m_tile_shape && this->m_tile_axes == other.m_tile_axes;
  }

  bool operator!=(const Layout& other) const
  {
    return !(*this == other);
  }

private:
  std::shared_ptr<cosy::proj::CRS> m_crs;
  std::shared_ptr<cosy::proj::Transformer> m_epsg4326_to_crs;
  xti::vec2s m_tile_shape;
  cosy::geo::CompassAxes m_tile_axes;
  cosy::NamedAxesTransformation<double, 2> m_crs_to_tile_axes;
  cosy::NamedAxesTransformation<double, 2> m_tile_to_pixel_axes;
  xti::vec2d m_lower_bound;
  xti::vec2d m_diff_bound;
};

} // end of ns tiledwebmaps
