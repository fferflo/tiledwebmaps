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
    : TileLoader(cache->get_layout())
    , m_cache(cache)
    , m_loader(loader)
  {
    xti::vec2s cache_tile_shape = cache->get_layout().get_tile_shape();
    xti::vec2s loader_tile_shape = loader->get_layout().get_tile_shape(); // can be larger than cache tile shape
    
    if (!xt::all(xt::equal(loader_tile_shape / cache_tile_shape * cache_tile_shape, loader_tile_shape)))
    {
      throw std::runtime_error("Cache tile shape must be a whole multiple of loader tile shape");
    }

    xti::vec2i zoom_up = loader_tile_shape / cache_tile_shape;
    if (zoom_up(0) != zoom_up(1))
    {
      throw std::runtime_error("Cache tile shape must be square if zooming up");
    }
    m_zoom_up = int(std::log2(zoom_up(0)) + 0.1);
  }

  Tile load(xti::vec2s tile_coord, size_t zoom)
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

    if (m_zoom_up == 0)
    {
      Tile image = m_loader->load(tile_coord, zoom);
      m_cache->save(image, tile_coord, zoom);
      return image;
    }
    else
    {
      int factor = (1 << m_zoom_up);

      xti::vec2s multitile_coord = tile_coord / factor;
      Tile multitile = m_loader->load(multitile_coord, zoom - m_zoom_up);

      // Divide into tiles
      xti::vec2s min_tile = multitile_coord * factor;
      xti::vec2s max_tile = (multitile_coord + 1) * factor;
      xti::vec2i corner1 = m_cache->get_layout().tile_to_pixel(min_tile, zoom);
      xti::vec2i corner2 = m_cache->get_layout().tile_to_pixel(max_tile, zoom);
      xti::vec2i image_min_pixel = xt::minimum(corner1, corner2);
      xti::vec2i image_max_pixel = xt::maximum(corner1, corner2);

      Tile resulttile;
      for (size_t tile_rel_x = 0; tile_rel_x < factor; tile_rel_x++)
      {
        for (size_t tile_rel_y = 0; tile_rel_y < factor; tile_rel_y++)
        {
          xti::vec2s tile_rel({tile_rel_x, tile_rel_y});
          xti::vec2s subtile_coord = multitile_coord * factor + tile_rel;

          xti::vec2s tile_coord_xy = min_tile + tile_rel;
          xti::vec2i corner1 = m_cache->get_layout().tile_to_pixel(tile_coord_xy, zoom);
          xti::vec2i corner2 = m_cache->get_layout().tile_to_pixel(tile_coord_xy + 1, zoom);
          xti::vec2i min_pixel = xt::minimum(corner1, corner2) - image_min_pixel;
          xti::vec2i max_pixel = xt::maximum(corner1, corner2) - image_min_pixel;

          Tile subtile = xt::view(multitile, xt::range(min_pixel(0), max_pixel(0)), xt::range(min_pixel(1), max_pixel(1)), xt::all());
          m_cache->save(subtile, subtile_coord, zoom);

          if (xt::all(xt::equal(tile_coord_xy, tile_coord)))
          {
            resulttile = subtile;
          }
        }
      }

      return resulttile;
    }
  }

private:
  std::shared_ptr<TileLoader> m_loader;
  std::shared_ptr<Cache> m_cache;
  int m_zoom_up;
};

} // end of ns tiledwebmaps
