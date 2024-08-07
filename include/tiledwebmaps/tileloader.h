#pragma once

#include <xti/typedefs.h>
#include <xti/util.h>
#include <xtensor/xtensor.hpp>
#include <xtensor/xview.hpp>
#include <opencv2/imgproc.hpp>
#include <exception>
#include <string>
#include <tiledwebmaps/layout.h>
#include <regex>

namespace tiledwebmaps {

class LoadTileException : public std::exception
{
public:
  LoadTileException(std::string message)
    : m_message(message)
  {
  }

  LoadTileException()
  {
  }

  const char* what() const noexcept override
  {
    return m_message.c_str();
  }

private:
  std::string m_message;
};

class TileLoader
{
public:
  TileLoader(const Layout& layout)
    : m_layout(layout)
  {
  }

  virtual ~TileLoader() = default;

  virtual cv::Mat load(xti::vec2i tile, int zoom) = 0;

  const Layout& get_layout() const
  {
    return m_layout;
  }

  virtual void make_forksafe()
  {
  }

  virtual int get_min_zoom() const = 0;

  virtual int get_max_zoom() const = 0;

  int get_zoom(xti::vec2d latlon, float meters_per_pixel) const
  {
    int zoom = get_min_zoom();
    while (zoom < get_max_zoom() && 1.0 / xt::amax(get_layout().pixels_per_meter_at_latlon(latlon, zoom))() >= 0.5 * meters_per_pixel)
    {
      zoom++;
    }
    return zoom;
  }

protected:
  void to_tile(cv::Mat& input, bool bgr_to_rgb = true) const
  {
    xti::vec2i got_tile_shape({(int) input.rows, (int) input.cols});
    if (got_tile_shape != m_layout.get_tile_shape_px())
    {
      throw LoadTileException("Expected tile shape " + XTI_TO_STRING(m_layout.get_tile_shape_px()) + ", got tile shape " + XTI_TO_STRING(got_tile_shape));
    }

    if (input.channels() == 3)
    {
      if (bgr_to_rgb)
      {
        cv::cvtColor(input, input, cv::COLOR_BGR2RGB);
      }
    }
    else if (input.channels() == 4)
    {
      if (bgr_to_rgb)
      {
        cv::cvtColor(input, input, cv::COLOR_BGRA2RGB);
      }
      else
      {
        cv::cvtColor(input, input, cv::COLOR_BGRA2BGR);
      }
    }
    else
    {
      throw LoadTileException("Expected 3 or 4 color channels, got " + std::to_string(input.channels()));
    }
  }

private:
  Layout m_layout;
};

uint64_t get_time()
{
  return std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
}

cv::Mat load(TileLoader& tileloader, xti::vec2i min_tile, xti::vec2i max_tile, int zoom)
{
  xti::vec2i tiles_num = max_tile - min_tile;
  xti::vec2i pixels_num = xt::abs(tileloader.get_layout().tile_to_pixel(tiles_num, zoom));

  xti::vec2i corner1 = tileloader.get_layout().tile_to_pixel(min_tile, zoom);
  xti::vec2i corner2 = tileloader.get_layout().tile_to_pixel(max_tile, zoom);
  xti::vec2i image_min_pixel = xt::minimum(corner1, corner2);
  xti::vec2i image_max_pixel = xt::maximum(corner1, corner2);

  cv::Mat image(pixels_num(0), pixels_num(1), CV_8UC3, cv::Scalar(0, 0, 0));
  for (int t0 = min_tile(0); t0 < max_tile(0); t0++)
  {
    for (int t1 = min_tile(1); t1 < max_tile(1); t1++)
    {
      xti::vec2i tile({t0, t1});
      cv::Mat tile_image = tileloader.load(tile, zoom);

      xti::vec2i corner1 = tileloader.get_layout().tile_to_pixel(tile, zoom);
      xti::vec2i corner2 = tileloader.get_layout().tile_to_pixel(tile + 1, zoom);
      xti::vec2i min_pixel = xt::minimum(corner1, corner2) - image_min_pixel;
      xti::vec2i max_pixel = xt::maximum(corner1, corner2) - image_min_pixel;

      cv::Rect roi(min_pixel(1), min_pixel(0), max_pixel(1) - min_pixel(1), max_pixel(0) - min_pixel(0));
      cv::Mat image_roi = image(roi);
      tile_image.copyTo(image_roi);
    }
  }

  return image;
}

cv::Mat load(TileLoader& tileloader, xti::vec2i tile, int zoom)
{
  return tileloader.load(tile, zoom);
}

cv::Mat load_metric(TileLoader& tileloader, xti::vec2d latlon, float bearing, float meters_per_pixel, xti::vec2i shape, int zoom)
{
  // Load source image
  xti::vec2f dest_pixels = shape;
  xti::vec2f dest_meters = dest_pixels * meters_per_pixel;
  xti::vec2f src_meters = dest_meters;
  xti::vec2f src_pixels_per_meter = tileloader.get_layout().pixels_per_meter_at_latlon(latlon, zoom);
  float src_pixels_per_meter1 = 0.5 * (src_pixels_per_meter(0) + src_pixels_per_meter(1));
  src_pixels_per_meter = xti::vec2f({src_pixels_per_meter1, src_pixels_per_meter1}); // TODO: why is this necessary?
  xti::vec2f src_pixels = src_meters * src_pixels_per_meter;

  float rotation_factor = std::fmod(tiledwebmaps::radians(bearing), xt::numeric_constants<float>::PI / 2);
  if (rotation_factor < 0)
  {
    rotation_factor += xt::numeric_constants<float>::PI / 2;
  }
  rotation_factor = std::sqrt(2.0f) * std::sin(rotation_factor + xt::numeric_constants<float>::PI / 4);
  src_pixels = src_pixels * rotation_factor;

  xti::vec2d global_center_pixel = tileloader.get_layout().epsg4326_to_pixel(latlon, zoom);
  xti::vec2d global_min_pixel = global_center_pixel - src_pixels / 2;
  xti::vec2d global_max_pixel = global_center_pixel + src_pixels / 2;

  xti::vec2i global_tile_corner1 = tileloader.get_layout().pixel_to_tile(global_min_pixel, zoom);
  xti::vec2i global_tile_corner2 = tileloader.get_layout().pixel_to_tile(global_max_pixel, zoom);
  xti::vec2i global_min_tile = xt::minimum(global_tile_corner1, global_tile_corner2);
  xti::vec2i global_max_tile = xt::maximum(global_tile_corner1, global_tile_corner2) + 1;

  cv::Mat src_image = load(tileloader, global_min_tile, global_max_tile, zoom);

  if (xt::amin(src_pixels_per_meter)() > 1.0 / meters_per_pixel)
  {
    double sigma = (xt::amin(src_pixels_per_meter)() * meters_per_pixel - 1) / 2;
    size_t kernel_size = static_cast<size_t>(std::ceil(sigma) * 4) + 1;
    cv::GaussianBlur(src_image, src_image, cv::Size(kernel_size, kernel_size), sigma, sigma);
  }

  // Sample dest image
  xti::vec2d global_srcimagemin_pixel = xt::minimum(tileloader.get_layout().tile_to_pixel(global_min_tile, zoom), tileloader.get_layout().tile_to_pixel(global_max_tile, zoom));
  xti::vec2d destim_center_pixel = xt::cast<float>(shape) / 2;
  xti::vec2d srcim_center_pixel = global_center_pixel - global_srcimagemin_pixel;
  float angle_dest_to_src = -tiledwebmaps::radians(bearing) + tileloader.get_layout().get_meridian_convergence(latlon);
  tiledwebmaps::ScaledRigid<float, 2, false> dest_to_center;
  dest_to_center.get_translation() = -destim_center_pixel;
  tiledwebmaps::ScaledRigid<float, 2, false> dest_pixels_to_meters;
  dest_pixels_to_meters.get_scale() = xti::vec2f({meters_per_pixel, meters_per_pixel});
  tiledwebmaps::ScaledRigid<float, 2, false> rotate_dest_to_src;
  rotate_dest_to_src.get_rotation() = tiledwebmaps::angle_to_rotation_matrix(angle_dest_to_src); // TODO: epsg4326_to_epsg....transform_angle()?
  tiledwebmaps::ScaledRigid<float, 2, false> src_meters_to_pixels;
  src_meters_to_pixels.get_scale() = src_pixels_per_meter;
  tiledwebmaps::ScaledRigid<float, 2, false> src_from_center;
  src_from_center.get_translation() = srcim_center_pixel;

  tiledwebmaps::ScaledRigid<float, 2, false> transform = src_from_center * src_meters_to_pixels * rotate_dest_to_src * dest_pixels_to_meters * dest_to_center;
  xti::mat2f sR = transform.get_rotation();
  for (int r = 0; r < 2; r++)
  {
    for (int c = 0; c < 2; c++)
    {
      sR(r, c) *= transform.get_scale()(r);
    }
  }
  xti::vec2f t = transform.get_translation();

  cv::Size newsize((size_t) shape(1), (size_t) shape(0));
  cv::Mat map_x(newsize, CV_32FC1);
  cv::Mat map_y(newsize, CV_32FC1);
  for (int r = 0; r < shape(0); r++)
  {
    float point0 = r;
    float sR00t0 = sR(0, 0) * point0 + t(0);
    float sR10t0 = sR(1, 0) * point0 + t(1);
    for (int c = 0; c < shape(1); c++)
    {
      float point1 = c;
      map_y.at<float>(r, c) = sR00t0 + sR(0, 1) * point1;
      map_x.at<float>(r, c) = sR10t0 + sR(1, 1) * point1;
    }
  }
  cv::Mat dest_image;
  cv::remap(src_image, dest_image, map_x, map_y, cv::INTER_LINEAR, cv::BORDER_CONSTANT, cv::Scalar(0, 0, 0)); // BORDER_REPLICATE

  return dest_image;
}

cv::Mat load_metric(TileLoader& tileloader, xti::vec2d latlon, float bearing, float meters_per_pixel, xti::vec2i shape)
{
  return load_metric(tileloader, latlon, bearing, meters_per_pixel, shape, tileloader.get_zoom(latlon, meters_per_pixel));
}

std::string replace_placeholders(std::string url, const Layout& layout, xti::vec2i tile, int zoom)
{
  xti::vec2d crs_lower = layout.tile_to_crs(tile, zoom);
  xti::vec2d crs_upper = layout.tile_to_crs(tile + 1, zoom);
  if (crs_lower(0) > crs_upper(0))
  {
    std::swap(crs_lower(0), crs_upper(0));
  }
  if (crs_lower(1) > crs_upper(1))
  {
    std::swap(crs_lower(1), crs_upper(1));
  }
  xti::vec2d crs_center = layout.tile_to_crs(tile + 0.5, zoom);
  xti::vec2d crs_size = crs_upper - crs_lower;

  xti::vec2d px_lower = layout.tile_to_pixel(tile, zoom);
  xti::vec2d px_upper = layout.tile_to_pixel(tile + 1, zoom);
  if (px_lower(0) > px_upper(0))
  {
    std::swap(px_lower(0), px_upper(0));
  }
  if (px_lower(1) > px_upper(1))
  {
    std::swap(px_lower(1), px_upper(1));
  }
  xti::vec2d px_center = layout.tile_to_pixel(tile + 0.5, zoom);
  xti::vec2i px_size = layout.get_tile_shape_px();

  xti::vec2i tile_lower = tile;
  xti::vec2d tile_center = tile + 0.5;
  xti::vec2i tile_upper = tile + 1;

  xti::vec2d latlon_lower = layout.tile_to_epsg4326(tile, zoom);
  xti::vec2d latlon_upper = layout.tile_to_epsg4326(tile + 1, zoom);
  if (latlon_lower(0) > latlon_upper(0))
  {
    std::swap(latlon_lower(0), latlon_upper(0));
  }
  if (latlon_lower(1) > latlon_upper(1))
  {
    std::swap(latlon_lower(1), latlon_upper(1));
  }
  xti::vec2d latlon_center = layout.tile_to_epsg4326(tile + 0.5, zoom);
  xti::vec2d latlon_size = latlon_upper - latlon_lower;

  std::string quad = "";
  for (int32_t bit = zoom; bit > 0; bit--)
  {
    char digit = '0';
    auto mask = 1 << (bit - 1);
    if ((tile(0) & mask) != 0)
    {
      digit += 1;
    }
    if ((tile(1) & mask) != 0)
    {
      digit += 2;
    }
    quad += digit;
  }

  std::string crs = layout.get_crs()->get_description();
  std::string bbox = std::to_string(crs_lower(0)) + "," + std::to_string(crs_lower(1)) + "," + std::to_string(crs_upper(0)) + "," + std::to_string(crs_upper(1));

  url = std::regex_replace(url, std::regex("\\{crs_lower_x\\}"), std::to_string(crs_lower(0)));
  url = std::regex_replace(url, std::regex("\\{crs_lower_y\\}"), std::to_string(crs_lower(1)));
  url = std::regex_replace(url, std::regex("\\{crs_upper_x\\}"), std::to_string(crs_upper(0)));
  url = std::regex_replace(url, std::regex("\\{crs_upper_y\\}"), std::to_string(crs_upper(1)));
  url = std::regex_replace(url, std::regex("\\{crs_center_x\\}"), std::to_string(crs_center(0)));
  url = std::regex_replace(url, std::regex("\\{crs_center_y\\}"), std::to_string(crs_center(1)));
  url = std::regex_replace(url, std::regex("\\{crs_size_x\\}"), std::to_string(crs_size(0)));
  url = std::regex_replace(url, std::regex("\\{crs_size_y\\}"), std::to_string(crs_size(1)));

  url = std::regex_replace(url, std::regex("\\{px_lower_x\\}"), std::to_string(px_lower(0)));
  url = std::regex_replace(url, std::regex("\\{px_lower_y\\}"), std::to_string(px_lower(1)));
  url = std::regex_replace(url, std::regex("\\{px_upper_x\\}"), std::to_string(px_upper(0)));
  url = std::regex_replace(url, std::regex("\\{px_upper_y\\}"), std::to_string(px_upper(1)));
  url = std::regex_replace(url, std::regex("\\{px_center_x\\}"), std::to_string(px_center(0)));
  url = std::regex_replace(url, std::regex("\\{px_center_y\\}"), std::to_string(px_center(1)));
  url = std::regex_replace(url, std::regex("\\{px_size_x\\}"), std::to_string(px_size(0)));
  url = std::regex_replace(url, std::regex("\\{px_size_y\\}"), std::to_string(px_size(1)));

  url = std::regex_replace(url, std::regex("\\{tile_lower_x\\}"), std::to_string(tile_lower(0)));
  url = std::regex_replace(url, std::regex("\\{tile_lower_y\\}"), std::to_string(tile_lower(1)));
  url = std::regex_replace(url, std::regex("\\{tile_upper_x\\}"), std::to_string(tile_upper(0)));
  url = std::regex_replace(url, std::regex("\\{tile_upper_y\\}"), std::to_string(tile_upper(1)));
  url = std::regex_replace(url, std::regex("\\{tile_center_x\\}"), std::to_string(tile_center(0)));
  url = std::regex_replace(url, std::regex("\\{tile_center_y\\}"), std::to_string(tile_center(1)));

  url = std::regex_replace(url, std::regex("\\{lat_lower\\}"), std::to_string(latlon_lower(0)));
  url = std::regex_replace(url, std::regex("\\{lon_lower\\}"), std::to_string(latlon_lower(1)));
  url = std::regex_replace(url, std::regex("\\{lat_upper\\}"), std::to_string(latlon_upper(0)));
  url = std::regex_replace(url, std::regex("\\{lon_upper\\}"), std::to_string(latlon_upper(1)));
  url = std::regex_replace(url, std::regex("\\{lat_center\\}"), std::to_string(latlon_center(0)));
  url = std::regex_replace(url, std::regex("\\{lon_center\\}"), std::to_string(latlon_center(1)));
  url = std::regex_replace(url, std::regex("\\{lat_size\\}"), std::to_string(latlon_size(0)));
  url = std::regex_replace(url, std::regex("\\{lon_size\\}"), std::to_string(latlon_size(1)));

  url = std::regex_replace(url, std::regex("\\{zoom\\}"), std::to_string(zoom));
  url = std::regex_replace(url, std::regex("\\{quad\\}"), quad);

  url = std::regex_replace(url, std::regex("\\{x\\}"), std::to_string(tile_lower(0)));
  url = std::regex_replace(url, std::regex("\\{y\\}"), std::to_string(tile_lower(1)));
  url = std::regex_replace(url, std::regex("\\{z\\}"), std::to_string(zoom));
  url = std::regex_replace(url, std::regex("\\{width\\}"), std::to_string(px_size(0)));
  url = std::regex_replace(url, std::regex("\\{height\\}"), std::to_string(px_size(1)));
  url = std::regex_replace(url, std::regex("\\{bbox\\}"), bbox);
  url = std::regex_replace(url, std::regex("\\{proj\\}"), crs);
  url = std::regex_replace(url, std::regex("\\{crs\\}"), crs);

  return url;
}

} // end of ns tiledwebmaps
