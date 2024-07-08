#pragma once

#include <xti/typedefs.h>
#include <xti/util.h>
#include <tiledwebmaps/tileloader.h>
#include <xtensor/xarray.hpp>
#include <xtensor/xtensor.hpp>
#include <memory>

namespace tiledwebmaps {

class CacheFailure : public std::exception
{
public:
  virtual const char* what() const throw ()
  {
    return "Cache failure";
  }
};

class Cache
{
public:
  virtual cv::Mat load(xti::vec2i tile, int zoom) = 0;

  virtual void save(const cv::Mat& image, xti::vec2i tile, int zoom) = 0;

  virtual bool contains(xti::vec2i tile, int zoom) const = 0;
};

class CachedTileLoader : public TileLoader
{
public:
  CachedTileLoader(std::shared_ptr<TileLoader> loader, std::shared_ptr<Cache> cache)
    : TileLoader(loader->get_layout())
    , m_cache(cache)
    , m_loader(loader)
  {
  }

  int get_min_zoom() const
  {
    return m_loader->get_min_zoom();
  }

  int get_max_zoom() const
  {
    return m_loader->get_max_zoom();
  }

  cv::Mat load(xti::vec2i tile_coord, int zoom)
  {
    if (m_cache->contains(tile_coord, zoom))
    {
      try
      {
        return m_cache->load(tile_coord, zoom);
      }
      catch (CacheFailure e)
      {
      }
    }

    cv::Mat image = m_loader->load(tile_coord, zoom);
    m_cache->save(image, tile_coord, zoom);
    return image;
  }

  std::shared_ptr<Cache> get_cache() const
  {
    return m_cache;
  }

  virtual void make_forksafe()
  {
    m_loader->make_forksafe();
  }

private:
  std::shared_ptr<TileLoader> m_loader;
  std::shared_ptr<Cache> m_cache;
};

class WithDefault : public TileLoader
{
public:
  WithDefault(std::shared_ptr<TileLoader> tileloader, xti::vec3i color)
    : TileLoader(tileloader->get_layout())
    , m_tileloader(tileloader)
    , m_color(color)
  {
  }

  int get_min_zoom() const
  {
    return m_tileloader->get_min_zoom();
  }

  int get_max_zoom() const
  {
    return m_tileloader->get_max_zoom();
  }

  cv::Mat load(xti::vec2i tile_coord, int zoom)
  {
    if (zoom > get_max_zoom())
    {
      throw LoadTileException("Zoom level " + XTI_TO_STRING(zoom) + " is higher than the maximum zoom level " + XTI_TO_STRING(get_max_zoom()) + ".");
    }
    if (zoom < get_min_zoom())
    {
      throw LoadTileException("Zoom level " + XTI_TO_STRING(zoom) + " is lower than the minimum zoom level " + XTI_TO_STRING(get_min_zoom()) + ".");
    }
    try
    {
      return m_tileloader->load(tile_coord, zoom);
    }
    catch (LoadTileException e)
    {
    }
    catch (CacheFailure e)
    {
    }

    return cv::Mat(m_tileloader->get_layout().get_tile_shape_px()(1), m_tileloader->get_layout().get_tile_shape_px()(0), CV_8UC3, cv::Scalar(m_color(0), m_color(1), m_color(2)));
  }

  virtual void make_forksafe()
  {
    m_tileloader->make_forksafe();
  }

private:
  std::shared_ptr<TileLoader> m_tileloader;
  xti::vec3i m_color;
};

} // end of ns tiledwebmaps
