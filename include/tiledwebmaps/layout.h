#pragma once

#include <xti/typedefs.h>
#include <xti/opencv.h>
#include <tiledwebmaps/proj.h>
#include <tiledwebmaps/affine.h>
#include <xtensor/xmath.hpp>
#include <xtensor/xindex_view.hpp>
#include <utility>

namespace tiledwebmaps {

static const tiledwebmaps::geo::CompassAxes pixel_axes("south", "east");

class Layout
{
public:
  // https://gist.github.com/tmcw/4954720
  static Layout XYZ(std::shared_ptr<tiledwebmaps::proj::Context> proj_context)
  {
    std::shared_ptr<tiledwebmaps::proj::CRS> crs = std::make_shared<tiledwebmaps::proj::CRS>(proj_context, "epsg:3857");
    std::shared_ptr<tiledwebmaps::proj::Transformer> epsg4326_to_crs = std::make_shared<tiledwebmaps::proj::Transformer>(std::make_shared<tiledwebmaps::proj::CRS>(proj_context, "epsg:4326"), crs);

    auto area_of_use = crs->get_area_of_use();

    xti::vec2d lower_crs = epsg4326_to_crs->transform(area_of_use.lower_latlon);
    xti::vec2d upper_crs = epsg4326_to_crs->transform(area_of_use.upper_latlon);

    lower_crs(1) = lower_crs(0);
    upper_crs(1) = upper_crs(0);

    xti::vec2d tile_shape_crs = upper_crs - lower_crs;
    xti::vec2d origin_crs = lower_crs;
    xti::vec2d size_crs = upper_crs - lower_crs;

    return Layout(
      std::make_shared<tiledwebmaps::proj::CRS>(proj_context, "epsg:3857"),
      xti::vec2i({256, 256}),
      tile_shape_crs,
      lower_crs,
      upper_crs - lower_crs,
      tiledwebmaps::geo::CompassAxes("east", "south")
    );
  }

  Layout(std::shared_ptr<tiledwebmaps::proj::CRS> crs, xti::vec2i tile_shape_px, xti::vec2d tile_shape_crs, xti::vec2d origin_crs, std::optional<xti::vec2d> size_crs, tiledwebmaps::geo::CompassAxes tile_axes)
    : Layout(crs, std::make_shared<tiledwebmaps::proj::Transformer>(std::make_shared<tiledwebmaps::proj::CRS>(crs->get_context(), "epsg:4326"), crs), tile_shape_px, tile_shape_crs, origin_crs, size_crs, tile_axes)
  {
  }

  Layout(std::shared_ptr<tiledwebmaps::proj::CRS> crs, std::shared_ptr<tiledwebmaps::proj::Transformer> epsg4326_to_crs, xti::vec2i tile_shape_px, xti::vec2d tile_shape_crs, xti::vec2d origin_crs, std::optional<xti::vec2d> size_crs, tiledwebmaps::geo::CompassAxes tile_axes)
    : m_crs(crs)
    , m_epsg4326_to_crs(std::move(epsg4326_to_crs))
    , m_tile_shape_px(tile_shape_px)
    , m_tile_shape_crs(tile_shape_crs)
    , m_origin_crs(origin_crs)
    , m_size_crs(size_crs)
    , m_tile_axes(tile_axes)
    , m_crs_to_tile_axes(crs->get_axes(), tile_axes)
    , m_tile_to_pixel_axes(m_tile_axes, pixel_axes)
  {
    if (m_tile_shape_px(0) != m_tile_shape_px(1))
    {
      throw std::runtime_error("tile_shape_px must be square");
    }
    if (m_tile_shape_crs(0) != m_tile_shape_crs(1))
    {
      throw std::runtime_error("tile_shape_crs must be square");
    }
    tiledwebmaps::NamedAxesTransformation<double, 2> crs_to_tile_axes(crs->get_axes(), m_tile_axes);
    tiledwebmaps::NamedAxesTransformation<double, 2> tile_to_pixel_axes(m_tile_axes, pixel_axes);

    m_tile_to_crs = tiledwebmaps::ScaledRigid<double, 2>(
      crs_to_tile_axes.inverse().get_rotation(),
      m_origin_crs,
      1.0
    );
    if (xt::any(crs_to_tile_axes.get_rotation() < 0))
    {
      m_tile_to_crs.get_translation() += xt::maximum(-crs_to_tile_axes.transform(size_crs.value()), 0.0);
    }

    m_tile_to_pixel = tiledwebmaps::ScaledRigid<double, 2>(
      tile_to_pixel_axes.get_rotation(),
      xti::vec2d({0.0, 0.0}),// xt::maximum(-(scale * tile_to_pixel_axes.transform(xt::abs(crs_to_tile_axes.transform(m_size_crs))) - 1), 0.0),
      tile_shape_px(0)
    );
  }

  const std::shared_ptr<tiledwebmaps::proj::Transformer>& get_epsg4326_to_crs() const
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
    double scale = (double) std::pow(2.0, zoom) / m_tile_shape_crs(0);
    return crs_to_tile(coords_crs, scale);
  }

  tiledwebmaps::ScaledRigid<double, 2> tile_to_crs(double scale) const
  {
    return tiledwebmaps::ScaledRigid<double, 2>(
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
    double scale = (double) std::pow(2.0, zoom) / m_tile_shape_crs(0);
    return tile_to_crs(coords_tile, scale);
  }

  tiledwebmaps::ScaledRigid<double, 2> tile_to_pixel(double scale) const
  {
    return tiledwebmaps::ScaledRigid<double, 2>(
      m_tile_to_pixel.get_rotation(),
      m_tile_to_pixel.get_translation(), // xt::maximum(-(scale * m_tile_to_pixel_axes.transform(xt::abs(m_crs_to_tile_axes.transform(m_size_crs))) - 1), 0.0),
      m_tile_shape_px(0)
    );
  }

  xti::vec2d tile_to_pixel(xti::vec2d coords_tile, double scale) const
  {
    return tile_to_pixel(scale).transform(coords_tile);
  }

  xti::vec2d tile_to_pixel(xti::vec2d coords_tile, int zoom) const
  {
    double scale = (double) std::pow(2.0, zoom) / m_tile_shape_crs(0);
    return tile_to_pixel(coords_tile, scale);
  }

  xti::vec2d pixel_to_tile(xti::vec2d coords_pixel, double scale) const
  {
    return tile_to_pixel(scale).transform_inverse(coords_pixel);
  }

  xti::vec2d pixel_to_tile(xti::vec2d coords_pixel, int zoom) const
  {
    double scale = (double) std::pow(2.0, zoom) / m_tile_shape_crs(0);
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
    xti::vec2d f_tile_size_meter = f_tile_size_deg * tiledwebmaps::geo::meters_per_deg_at_latlon(latlon);
    xti::vec2d f_tile_size_px = f * xt::cast<double>(m_tile_shape_px);
    xti::vec2d pixels_per_meter = xt::abs(m_tile_to_pixel_axes.transform(f_tile_size_px / f_tile_size_meter));
    return pixels_per_meter;
  }

  float get_meridian_convergence(xti::vec2d latlon) const
  {
    xti::vec2d latlon2({latlon(0) + 0.0001, latlon(1)});
    xti::vec2d crs1 = m_epsg4326_to_crs->transform(latlon);
    xti::vec2d crs2 = m_epsg4326_to_crs->transform(latlon2);
    xti::vec2d true_north = crs2 - crs1;
    xti::vec2d north = m_crs->get_vector("north");
    return angle_between_vectors(north, true_north);
  }

  std::shared_ptr<tiledwebmaps::proj::CRS> get_crs() const
  {
    return m_crs;
  }

  xti::vec2i get_tile_shape_px() const
  {
    return m_tile_shape_px;
  }

  xti::vec2d get_tile_shape_crs() const
  {
    return m_tile_shape_crs;
  }

  xti::vec2d get_origin_crs() const
  {
    return m_origin_crs;
  }

  std::optional<xti::vec2d> get_size_crs() const
  {
    return m_size_crs;
  }

  tiledwebmaps::geo::CompassAxes get_tile_axes() const
  {
    return m_tile_axes;
  }

  bool operator==(const Layout& other) const
  {
    return *this->m_crs == *other.m_crs && this->m_tile_shape_px == other.m_tile_shape_px && this->m_tile_shape_crs == other.m_tile_shape_crs && this->m_origin_crs == other.m_origin_crs && this->m_size_crs == other.m_size_crs && this->m_tile_axes == other.m_tile_axes;
  }

  bool operator!=(const Layout& other) const
  {
    return !(*this == other);
  }

private:
  std::shared_ptr<tiledwebmaps::proj::CRS> m_crs;
  std::shared_ptr<tiledwebmaps::proj::Transformer> m_epsg4326_to_crs;
  xti::vec2i m_tile_shape_px;
  xti::vec2d m_tile_shape_crs;
  xti::vec2d m_origin_crs;
  std::optional<xti::vec2d> m_size_crs;
  tiledwebmaps::geo::CompassAxes m_tile_axes;
  tiledwebmaps::NamedAxesTransformation<double, 2> m_crs_to_tile_axes;
  tiledwebmaps::NamedAxesTransformation<double, 2> m_tile_to_pixel_axes;

  tiledwebmaps::ScaledRigid<double, 2> m_tile_to_crs;
  tiledwebmaps::ScaledRigid<double, 2> m_tile_to_pixel;
};

} // end of ns tiledwebmaps
