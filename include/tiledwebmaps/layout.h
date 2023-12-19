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

class Layout
{
public:
  // https://gist.github.com/tmcw/4954720
  static Layout XYZ(std::shared_ptr<cosy::proj::Context> proj_context)
  {
    return Layout(std::make_shared<cosy::proj::CRS>(proj_context, "epsg:3857"), xti::vec2i({256, 256}), cosy::geo::CompassAxes("east", "south"));
  }

  // https://gist.github.com/tmcw/4954720
  static Layout TMS(std::shared_ptr<cosy::proj::Context> proj_context)
  {
    return Layout(std::make_shared<cosy::proj::CRS>(proj_context, "epsg:3857"), xti::vec2i({256, 256}), cosy::geo::CompassAxes("east", "north"));
  }

  Layout(std::shared_ptr<cosy::proj::CRS> crs, xti::vec2i tile_shape, cosy::geo::CompassAxes tile_axes, std::optional<std::pair<xti::vec2d, xti::vec2d>> bounds_crs = std::optional<std::pair<xti::vec2d, xti::vec2d>>(), double zoom0_scale = 0.0, bool use_only_first_bound_axis = true)
    : Layout(crs, std::make_shared<cosy::proj::Transformer>(std::make_shared<cosy::proj::CRS>(crs->get_context(), "epsg:4326"), crs), tile_shape, tile_axes, bounds_crs, zoom0_scale, use_only_first_bound_axis)
  {
  }

  Layout(std::shared_ptr<cosy::proj::CRS> crs, std::shared_ptr<cosy::proj::Transformer> epsg4326_to_crs, xti::vec2i tile_shape, cosy::geo::CompassAxes tile_axes, std::optional<std::pair<xti::vec2d, xti::vec2d>> bounds_crs = std::optional<std::pair<xti::vec2d, xti::vec2d>>(), double zoom0_scale = 0.0, bool use_only_first_bound_axis = true)
    : m_crs(crs)
    , m_epsg4326_to_crs(std::move(epsg4326_to_crs))
    , m_tile_shape(tile_shape)
    , m_tile_axes(tile_axes)
    , m_crs_to_tile_axes(crs->get_axes(), tile_axes)
    , m_tile_to_pixel_axes(m_tile_axes, pixel_axes)
  {
    if (bounds_crs)
    {
      m_origin_crs = bounds_crs->first;
      m_size_crs = bounds_crs->second - bounds_crs->first;
    }
    else
    {
      auto area_of_use = m_crs->get_area_of_use();
      m_origin_crs = m_epsg4326_to_crs->transform(area_of_use.lower_latlon);
      m_size_crs = m_epsg4326_to_crs->transform(area_of_use.upper_latlon) - m_origin_crs;
    }

    if (use_only_first_bound_axis)
    {
      m_origin_crs(1) = m_origin_crs(0);
      m_size_crs(1) = m_size_crs(0);
    }

    m_zoom0_scale = (zoom0_scale <= 0.0) ? (1.0 / m_size_crs(0)) : zoom0_scale; // size of level0 in CRS units

    cosy::NamedAxesTransformation<double, 2> crs_to_tile_axes(crs->get_axes(), m_tile_axes);
    cosy::NamedAxesTransformation<double, 2> tile_to_pixel_axes(m_tile_axes, pixel_axes);

    m_tile_to_crs = cosy::ScaledRigid<double, 2>(
      crs_to_tile_axes.inverse().get_rotation(),
      m_origin_crs + xt::maximum(-crs_to_tile_axes.transform(m_size_crs), 0.0),
      1.0
    );

    m_tile_to_pixel = cosy::ScaledRigid<double, 2>(
      tile_to_pixel_axes.get_rotation(),
      xti::vec2d({0.0, 0.0}),// xt::maximum(-(scale * tile_to_pixel_axes.transform(xt::abs(crs_to_tile_axes.transform(m_size_crs))) - 1), 0.0),
      tile_shape(0)
    );

    m_bounds_crs = std::make_pair(m_origin_crs, m_origin_crs + m_size_crs);
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

  xti::vec2d crs_to_tile(xti::vec2d coords_crs, double scale) const
  {
    return tile_to_crs(scale).transform_inverse(coords_crs);
  }
 
  xti::vec2d crs_to_tile(xti::vec2d coords_crs, int zoom) const
  {
    double scale = (double) (1 << zoom) * m_zoom0_scale;
    return crs_to_tile(coords_crs, scale);
  }

  cosy::ScaledRigid<double, 2> tile_to_crs(double scale) const
  {
    return cosy::ScaledRigid<double, 2>(
      m_tile_to_crs.get_rotation(),
      m_tile_to_crs.get_translation(),
      1.0 / scale
    );
  }

  xti::vec2d tile_to_crs(xti::vec2d coords_tile, double scale) const
  {
    return tile_to_crs(scale).transform(coords_tile);
  }

  xti::vec2d tile_to_crs(xti::vec2d coords_tile, int zoom) const
  {
    double scale = (double) (1 << zoom) * m_zoom0_scale;
    return tile_to_crs(coords_tile, scale);
  }

  cosy::ScaledRigid<double, 2> tile_to_pixel(double scale) const
  {
    return cosy::ScaledRigid<double, 2>(
      m_tile_to_pixel.get_rotation(),
      m_tile_to_pixel.get_translation(), // xt::maximum(-(scale * m_tile_to_pixel_axes.transform(xt::abs(m_crs_to_tile_axes.transform(m_size_crs))) - 1), 0.0),
      m_tile_shape(0)
    );
  }

  xti::vec2d tile_to_pixel(xti::vec2d coords_tile, double scale) const
  {
    return tile_to_pixel(scale).transform(coords_tile);
  }

  xti::vec2d tile_to_pixel(xti::vec2d coords_tile, int zoom) const
  {
    double scale = (double) (1 << zoom) * m_zoom0_scale;
    return tile_to_pixel(coords_tile, scale);
  }

  xti::vec2d pixel_to_tile(xti::vec2d coords_pixel, double scale) const
  {
    return tile_to_pixel(scale).transform_inverse(coords_pixel);
  }

  xti::vec2d pixel_to_tile(xti::vec2d coords_pixel, int zoom) const
  {
    double scale = (double) (1 << zoom) * m_zoom0_scale;
    return pixel_to_tile(coords_pixel, scale);
  }

  template <typename T>
  xti::vec2d epsg4326_to_tile(xti::vec2d coords_epsg4326, T zoom_or_scale) const
  {
    return crs_to_tile(epsg4326_to_crs(coords_epsg4326), zoom_or_scale);
  }

  template <typename T>
  xti::vec2d tile_to_epsg4326(xti::vec2d coords_tile, T zoom_or_scale) const
  {
    return crs_to_epsg4326(tile_to_crs(coords_tile, zoom_or_scale));
  }

  template <typename T>
  xti::vec2d epsg4326_to_pixel(xti::vec2d coords_epsg4326, T zoom_or_scale) const
  {
    return tile_to_pixel(epsg4326_to_tile(coords_epsg4326, zoom_or_scale), zoom_or_scale);
  }

  template <typename T>
  xti::vec2d pixel_to_epsg4326(xti::vec2d coords_pixel, T zoom_or_scale) const
  {
    return tile_to_epsg4326(pixel_to_tile(coords_pixel, zoom_or_scale), zoom_or_scale);
  }

  template <typename T>
  xti::vec2d pixels_per_meter_at_latlon(xti::vec2d latlon, T zoom_or_scale) const
  {
    static const double f = 0.1;
    xti::vec2d center_tile = epsg4326_to_tile(latlon, zoom_or_scale);
    xti::vec2d f_tile_size_deg = xt::abs(tile_to_epsg4326(center_tile + 0.5 * f, zoom_or_scale) - tile_to_epsg4326(center_tile - 0.5 * f, zoom_or_scale));
    xti::vec2d f_tile_size_meter = f_tile_size_deg * cosy::geo::meters_per_deg_at_latlon(latlon);
    xti::vec2d f_tile_size_px = f * xt::cast<double>(m_tile_shape);
    xti::vec2d pixels_per_meter = xt::abs(m_tile_to_pixel_axes.transform(f_tile_size_px / f_tile_size_meter));
    return pixels_per_meter;
  }

  std::shared_ptr<cosy::proj::CRS> get_crs() const
  {
    return m_crs;
  }

  xti::vec2i get_tile_shape() const
  {
    return m_tile_shape;
  }

  cosy::geo::CompassAxes get_tile_axes() const
  {
    return m_tile_axes;
  }

  std::pair<xti::vec2d, xti::vec2d> get_bounds_crs() const
  {
    return m_bounds_crs;
  }

  double get_zoom0_scale() const
  {
    return m_zoom0_scale;
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
  xti::vec2i m_tile_shape;
  cosy::geo::CompassAxes m_tile_axes;
  cosy::NamedAxesTransformation<double, 2> m_crs_to_tile_axes;
  cosy::NamedAxesTransformation<double, 2> m_tile_to_pixel_axes;
  xti::vec2d m_origin_crs;
  xti::vec2d m_size_crs;
  double m_zoom0_scale;
  std::pair<xti::vec2d, xti::vec2d> m_bounds_crs;

  cosy::ScaledRigid<double, 2> m_tile_to_crs;
  cosy::ScaledRigid<double, 2> m_tile_to_pixel;
};

} // end of ns tiledwebmaps
