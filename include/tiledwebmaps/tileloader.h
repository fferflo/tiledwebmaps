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

using Tile = xt::xtensor<uint8_t, 3>;

class TileLoader
{
public:
  TileLoader(const Layout& layout)
    : m_layout(layout)
  {
  }

  virtual ~TileLoader() = default;

  virtual Tile load(xti::vec2s tile, size_t zoom) = 0;

  const Layout& get_layout() const
  {
    return m_layout;
  }

protected:
  template <typename TTensor>
  Tile to_tile(TTensor&& input)
  {
    xti::vec2s got_tile_shape({input.shape()[0], input.shape()[1]});
    if (got_tile_shape != m_layout.get_tile_shape())
    {
      throw LoadTileException("Expected tile shape " + XTI_TO_STRING(m_layout.get_tile_shape()) + ", got tile shape " + XTI_TO_STRING(got_tile_shape));
    }

    Tile result;
    if (input.shape()[2] == 3)
    {
      result = std::move(input);
    }
    else if (input.shape()[2] == 4)
    {
      result = xt::view(std::move(input), xt::all(), xt::all(), xt::range(0, 3));
    }
    else
    {
      throw LoadTileException("Expected 3 or 4 color channels, got " + std::to_string(input.dimension()));
    }

    return result;
  }

private:
  Layout m_layout;
};

auto load(TileLoader& tile_loader, xti::vec2s min_tile, xti::vec2s max_tile, size_t zoom)
{
  xti::vec2s tiles_num = max_tile - min_tile;
  xti::vec2s pixels_num = tile_loader.get_layout().tile_to_pixel(tiles_num, zoom);
  xt::xtensor<uint8_t, 3> image({pixels_num(0), pixels_num(1), 3});
  for (size_t t0 = min_tile(0); t0 < max_tile(0); t0++)
  {
    for (size_t t1 = min_tile(1); t1 < max_tile(1); t1++)
    {
      xti::vec2s tile({t0, t1});
      auto tile_image = tile_loader.load(tile, zoom);

      xti::vec2s min_pixel = tile_loader.get_layout().tile_to_pixel(tile - min_tile, zoom);
      xti::vec2s max_pixel = tile_loader.get_layout().tile_to_pixel(tile - min_tile + 1, zoom);

      xt::view(image, xt::range(min_pixel(0), max_pixel(0)), xt::range(min_pixel(1), max_pixel(1)), xt::all()).assign(tile_image);
    }
  }

  return image;
}

auto load(TileLoader& tile_loader, xti::vec2s tile, size_t zoom)
{
  return tile_loader.load(tile, zoom);
}

xt::xtensor<uint8_t, 3> load_metric(TileLoader& tile_loader, xti::vec2d latlon, double bearing, double meters_per_pixel, xti::vec2s shape, size_t zoom)
{
  // Load source image
  xti::vec2d dest_pixels = shape;
  xti::vec2d dest_meters = dest_pixels * meters_per_pixel;
  xti::vec2d src_meters = dest_meters;
  double src_pixels_per_meter = tile_loader.get_layout().pixels_per_meter_at_latlon(latlon, zoom);
  xti::vec2d src_pixels = src_meters * src_pixels_per_meter;

  double rotation_factor = std::fmod(cosy::radians(bearing), xt::numeric_constants<double>::PI / 2);
  if (rotation_factor < 0)
  {
    rotation_factor += xt::numeric_constants<double>::PI / 2;
  }
  rotation_factor = std::sqrt(2) * std::sin(rotation_factor + xt::numeric_constants<double>::PI / 4);
  src_pixels = src_pixels * rotation_factor;

  xti::vec2d global_center_pixel = tile_loader.get_layout().epsg4326_to_pixel(latlon, zoom);
  xti::vec2d global_min_pixel = global_center_pixel - src_pixels / 2;
  xti::vec2d global_max_pixel = global_center_pixel + src_pixels / 2;

  xti::vec2s global_tile_corner1 = tile_loader.get_layout().pixel_to_tile(global_min_pixel, zoom);
  xti::vec2s global_tile_corner2 = tile_loader.get_layout().pixel_to_tile(global_max_pixel, zoom);
  xti::vec2s global_min_tile = xt::minimum(global_tile_corner1, global_tile_corner2);
  xti::vec2s global_max_tile = xt::maximum(global_tile_corner1, global_tile_corner2) + 1;

  auto src_image = load(tile_loader, global_min_tile, global_max_tile, zoom);

  // Anti-aliasing when downsampling
  if (src_pixels_per_meter > 1.0 / meters_per_pixel)
  {
    cv::Mat src_image_cv = xti::to_opencv(src_image);
    double sigma = (src_pixels_per_meter * meters_per_pixel - 1) / 2;
    size_t kernel_size = static_cast<size_t>(std::ceil(sigma) * 4) + 1;
    cv::GaussianBlur(src_image_cv, src_image_cv, cv::Size(kernel_size, kernel_size), sigma, sigma);
  }

  // Sample dest image
  xti::vec2d global_srcimagemin_pixel = xt::minimum(tile_loader.get_layout().tile_to_pixel(global_min_tile, zoom), tile_loader.get_layout().tile_to_pixel(global_max_tile, zoom));
  xti::vec2d destim_center_pixel = xt::cast<double>(shape) / 2;
  xti::vec2d srcim_center_pixel = global_center_pixel - global_srcimagemin_pixel;

  cosy::Rotation<double, 2> rotation(-cosy::radians(bearing));

  xt::xtensor<uint8_t, 3> dest_image({shape(0), shape(1), 3});
  for (size_t x = 0; x < shape(0); x++)
  {
    for (size_t y = 0; y < shape(1); y++)
    {
      xti::vec2s destim_pixel({x, y});
      xti::vec2d offset_from_center_in_meters = rotation.transform((destim_pixel - destim_center_pixel) * meters_per_pixel);
      xti::vec2d srcim_pixel = offset_from_center_in_meters * src_pixels_per_meter + srcim_center_pixel;
      srcim_pixel = xt::clip(srcim_pixel, 0, xti::vec2d({static_cast<double>(src_image.shape()[0]), static_cast<double>(src_image.shape()[1])}) - 1);

      // Linear interpolation
      xti::vec2i srcim_lower = xt::floor(srcim_pixel);
      xti::vec2i srcim_upper = srcim_lower + 1;
      if (srcim_lower(0) < 0)
      {
        srcim_lower(0) += 1;
        srcim_upper(0) += 1;
      }
      if (srcim_lower(1) < 0)
      {
        srcim_lower(1) += 1;
        srcim_upper(1) += 1;
      }
      if (srcim_upper(0) >= src_image.shape()[0])
      {
        srcim_lower(0) -= 1;
        srcim_upper(0) -= 1;
      }
      if (srcim_upper(1) >= src_image.shape()[1])
      {
        srcim_lower(1) -= 1;
        srcim_upper(1) -= 1;
      }
      xti::vec2d t = srcim_pixel - srcim_lower;

      auto get = [&](size_t x, size_t y){
        return xti::vec3T<uint8_t>({src_image(x, y, 0), src_image(x, y, 1), src_image(x, y, 2)});
      };

      xti::vec3T<float> value00 = get(srcim_lower(0), srcim_lower(1));
      xti::vec3T<float> value01 = get(srcim_lower(0), srcim_upper(1));
      xti::vec3T<float> value10 = get(srcim_upper(0), srcim_lower(1));
      xti::vec3T<float> value11 = get(srcim_upper(0), srcim_upper(1));

      xti::vec3T<float> value0 = (1 - t(1)) * value00 + t(1) * value01;
      xti::vec3T<float> value1 = (1 - t(1)) * value10 + t(1) * value11;

      xti::vec3T<float> value = (1 - t(0)) * value0 + t(0) * value1;

      // Save pixel
      dest_image(destim_pixel(0), destim_pixel(1), 0) = value(0);
      dest_image(destim_pixel(0), destim_pixel(1), 1) = value(1);
      dest_image(destim_pixel(0), destim_pixel(1), 2) = value(2);
    }
  }

  return dest_image;
}

xt::xtensor<uint8_t, 3> load_metric(TileLoader& tile_loader, xti::vec2d latlon, double bearing, double meters_per_pixel, xti::vec2s shape)
{
  size_t zoom = 0;
  while (1.0 / tile_loader.get_layout().pixels_per_meter_at_latlon(latlon, zoom) < 0.5 * meters_per_pixel)
  {
    zoom++;
  }
  return load_metric(tile_loader, latlon, bearing, meters_per_pixel, shape, zoom);
}

std::string replace_placeholders(std::string url, const Layout& layout, xti::vec2s tile, size_t zoom)
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
  xti::vec2i px_size = layout.get_tile_shape();

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
