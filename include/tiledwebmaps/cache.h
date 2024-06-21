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

class Cache : public TileLoader
{
public:
  Cache(const Layout& layout)
    : TileLoader(layout)
  {
  }

  virtual void save(const cv::Mat& image, xti::vec2i tile, int zoom) = 0;

  virtual bool contains(xti::vec2i tile, int zoom) const = 0;
};

class CachedTileLoader : public TileLoader
{
public:
  CachedTileLoader(std::shared_ptr<TileLoader> loader, std::shared_ptr<Cache> cache)
    : TileLoader(cache->get_layout())
    , m_cache(cache)
    , m_loader(loader)
  {
    xti::vec2i cache_tile_shape = cache->get_layout().get_tile_shape_px();
    xti::vec2i loader_tile_shape = loader->get_layout().get_tile_shape_px(); // can be larger than cache tile shape
    
    if (!xt::all(xt::equal(loader_tile_shape / cache_tile_shape * cache_tile_shape, loader_tile_shape)))
    {
      throw std::runtime_error("Cache tile shape must be a whole multiple of loader tile shape");
    }

    if (cache_tile_shape(0) != loader_tile_shape(0) || cache_tile_shape(1) != loader_tile_shape(1))
    {
      throw std::runtime_error("Cache tile shape must be equal to loader tile shape");
    }
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
    m_cache->make_forksafe();
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

  cv::Mat load(xti::vec2i tile_coord, int zoom)
  {
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
