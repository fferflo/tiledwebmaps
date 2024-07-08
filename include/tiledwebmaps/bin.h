#pragma once

#include <xti/typedefs.h>
#include <xti/exception.h>
#include <xti/opencv.h>
#include <opencv2/imgcodecs.hpp>
#include <xti/util.h>
#include <tiledwebmaps/tileloader.h>
#include <tiledwebmaps/cache.h>
#include <xtensor/xarray.hpp>
#include <xtensor/xtensor.hpp>
#include <xtensor-io/xnpz.hpp>
#include <filesystem>
#include <memory>
#include <cstdio>
#include <cstdlib>

namespace tiledwebmaps {

class Bin : public TileLoader
{
public:
  Bin(std::filesystem::path path, const Layout& layout)
    : TileLoader(layout)
    , m_path(path)
    , m_file_pointer(NULL)
  {
    if (!std::filesystem::exists(path / "images.dat"))
    {
      throw FileNotFoundException(path / "images.dat");
    }
    auto npz = xt::load_npz(path / "images-meta.npz");

    xt::xtensor<int64_t, 1> zoom = npz["zoom"].template cast<int64_t>();
    xt::xtensor<int64_t, 1> x = npz["x"].template cast<int64_t>();
    xt::xtensor<int64_t, 1> y = npz["y"].template cast<int64_t>();
    xt::xtensor<int64_t, 1> offset = npz["offset"].template cast<int64_t>();

    for (int i = 0; i < zoom.size(); i++)
    {
      m_tiles[std::make_tuple(zoom(i), x(i), y(i))] = std::make_tuple(offset(i), offset(i + 1) - offset(i));
    }

    m_min_zoom = xt::amin(zoom)();
    m_max_zoom = xt::amax(zoom)();
  }

  Bin(const Bin& other)
    : TileLoader(other)
    , m_path(other.m_path)
    , m_file_pointer(NULL)
    , m_tiles(other.m_tiles)
  {
  }

  Bin(Bin&& other)
    : TileLoader(other)
    , m_path(std::move(other.m_path))
    , m_file_pointer(other.m_file_pointer)
    , m_tiles(std::move(other.m_tiles))
  {
    other.m_file_pointer = NULL;
  }

  virtual ~Bin()
  {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_file_pointer != NULL)
    {
      std::fclose(m_file_pointer);
    }
  }

  Bin& operator=(const Bin& other)
  {
    if (this != &other)
    {
      std::lock_guard<std::mutex> lock(m_mutex);
      m_path = other.m_path;
      m_file_pointer = NULL;
      m_tiles = other.m_tiles;
    }
    return *this;
  }

  Bin& operator=(Bin&& other)
  {
    if (this != &other)
    {
      std::lock_guard<std::mutex> lock(m_mutex);
      m_path = std::move(other.m_path);
      m_file_pointer = other.m_file_pointer;
      other.m_file_pointer = NULL;
      m_tiles = std::move(other.m_tiles);
    }
    return *this;
  }

  virtual void make_forksafe()
  {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_file_pointer != NULL)
    {
      std::fclose(m_file_pointer);
      m_file_pointer = NULL;
    }
  }

  int get_min_zoom() const
  {
    return m_min_zoom;
  }

  int get_max_zoom() const
  {
    return m_max_zoom;
  }

  cv::Mat load(xti::vec2i tile, int zoom)
  {
    if (zoom > m_max_zoom)
    {
      throw LoadTileException("Zoom level " + XTI_TO_STRING(zoom) + " is higher than the maximum zoom level " + XTI_TO_STRING(m_max_zoom) + ".");
    }
    if (zoom < m_min_zoom)
    {
      throw LoadTileException("Zoom level " + XTI_TO_STRING(zoom) + " is lower than the minimum zoom level " + XTI_TO_STRING(m_min_zoom) + ".");
    }
    auto it = m_tiles.find(std::make_tuple(zoom, tile[0], tile[1]));
    if (it == m_tiles.end())
    {
      throw LoadTileException("Tile not found in bin file");
    }
    int64_t offset = std::get<0>(it->second);
    int64_t size = std::get<1>(it->second);

    std::vector<uint8_t> buffer(size);
    {
      std::lock_guard<std::mutex> lock(m_mutex);
      if (m_file_pointer == NULL)
      {
        m_file_pointer = std::fopen((m_path / "images.dat").string().c_str(), "rb");
        if (m_file_pointer == NULL)
        {
          throw LoadFileException(m_path / "images.dat", "Failed to open file");
        }
      }
      if (std::fseek(m_file_pointer, offset, SEEK_SET) != 0)
      {
        throw LoadFileException(m_path / "images.dat", "Failed to seek to offset " + std::to_string(offset));
      }
      if (std::fread(buffer.data(), 1, size, m_file_pointer) != size)
      {
        throw LoadFileException(m_path / "images.dat", "Failed to read " + std::to_string(size) + " bytes from offset " + std::to_string(offset));
      }
    }

    cv::Mat data_cv(1, buffer.size(), xti::opencv::pixeltype<uint8_t>::get(1), buffer.data());
    if (data_cv.data == NULL)
    {
        throw ImreadException("Failed to convert data array of file " + m_path.string() + " to cv mat");
    }

    cv::Mat image = cv::imdecode(data_cv, cv::IMREAD_COLOR);
    if (image.data == NULL)
    {
        throw ImreadException("Failed to decode image from file " + m_path.string());
    }

    cv::cvtColor(image, image, cv::COLOR_BGR2RGB);

    try
    {
      this->to_tile(image, true);
    }
    catch (LoadTileException ex)
    {
      throw LoadFileException(m_path, std::string("Loaded invalid tile. ") + ex.what());
    }
    return image;
  }

private:
  std::filesystem::path m_path;
  FILE* m_file_pointer;
  std::map<std::tuple<int64_t, int64_t, int64_t>, std::tuple<int64_t, int64_t>> m_tiles;
  int m_min_zoom;
  int m_max_zoom;
  std::mutex m_mutex;
};

} // end of ns tiledwebmaps
