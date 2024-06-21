#pragma once

#include <xti/typedefs.h>
#include <xti/exception.h>
#include <xti/util.h>
#include <tiledwebmaps/tileloader.h>
#include <tiledwebmaps/cache.h>
#include <xtensor/xarray.hpp>
#include <xtensor/xtensor.hpp>
#include <memory>
#include <tuple>
#include <mutex>

namespace tiledwebmaps {

class LRU : public Cache
{
public:
  using Key = std::tuple<int, int, int>; // tile-x, tile-y, zoom

  LRU(const Layout& layout, int size)
    : Cache(layout)
    , m_size(size)
  {
  }

  bool contains(xti::vec2i tile, int zoom) const
  {
    return m_key_to_tile.count(Key(tile(0), tile(1), zoom)) > 0;
  }

  cv::Mat load(xti::vec2i tile, int zoom)
  {
    std::lock_guard<std::mutex> guard(m_mutex);

    Key key(tile(0), tile(1), zoom);
    auto key_it = std::find(m_keys.begin(), m_keys.end(), key);
    if (key_it == m_keys.end())
    {
      throw CacheFailure();
    }
    m_keys.erase(key_it);
    m_keys.push_back(key);
    return m_key_to_tile[key];
  }

  void save(const cv::Mat& image, xti::vec2i tile, int zoom)
  {
    std::lock_guard<std::mutex> guard(m_mutex);

    Key key(tile(0), tile(1), zoom);
    auto key_it = std::find(m_keys.begin(), m_keys.end(), key);
    if (key_it != m_keys.end())
    {
      m_keys.erase(key_it);
    }
    m_keys.push_back(key);
    m_key_to_tile[key] = image;
    if (m_keys.size() > m_size)
    {
      m_key_to_tile.erase(m_key_to_tile.find(m_keys.front()));
      m_keys.pop_front();
    }
    if (m_keys.size() > m_size || m_key_to_tile.size() > m_size)
    {
      std::cout << "Assertion failure in tiledwebmaps::LRU" << std::endl;
      exit(-1);
    }
  }

private:
  int m_size;
  std::map<Key, cv::Mat> m_key_to_tile;
  std::list<Key> m_keys;
  std::mutex m_mutex;
};

} // end of ns tiledwebmaps
