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

  virtual void save(const Tile& image, xti::vec2s tile, size_t zoom) = 0;

  virtual bool contains(xti::vec2s tile, size_t zoom) const = 0;
};

class CachedTileLoader : public TileLoader
{
public:
  CachedTileLoader(std::shared_ptr<TileLoader> loader, std::shared_ptr<Cache> cache)
    : TileLoader(loader->get_layout())
    , m_cache(cache)
    , m_loader(loader)
  {
    if (loader->get_layout() != cache->get_layout())
    {
      throw std::invalid_argument(XTI_TO_STRING("Excepted tile loaders with the same layout"));
    }
  }

  Tile load(xti::vec2s tile, size_t zoom)
  {
    if (m_cache->contains(tile, zoom))
    {
      try
      {
        return m_cache->load(tile, zoom);
      }
      catch (CacheFailure e)
      {
      }
    }
    Tile image = m_loader->load(tile, zoom);
    m_cache->save(image, tile, zoom);
    return image;
  }

private:
  std::shared_ptr<TileLoader> m_loader;
  std::shared_ptr<Cache> m_cache;
};

} // end of ns tiledwebmaps
