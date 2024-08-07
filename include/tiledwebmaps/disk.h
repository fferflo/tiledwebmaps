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
#include <filesystem>
#include <memory>
#include <chrono>
#include <thread>
#include <fstream>
#include <shared_mutex>

namespace tiledwebmaps {

bool ends_with(std::string_view str, std::string_view suffix)
{
  return str.size() >= suffix.size() && 0 == str.compare(str.size() - suffix.size(), suffix.size(), suffix);
}

class ImreadException : public CacheFailure
{
public:
  ImreadException(std::string message)
    : m_message(message)
  {
  }

  virtual const char* what() const throw ()
  {
    return m_message.c_str();
  }

private:
  std::string m_message;
};

cv::Mat safe_imread(std::filesystem::path path)
{
  if (!std::filesystem::exists(path))
  {
    throw ImreadException(std::string("File does not exist: ") + path.string());
  }
  std::ifstream file(path.string(), std::ios::binary | std::ios::ate);
  std::streamsize size = file.tellg();
  if (size == 0)
  {
    throw ImreadException(std::string("File is empty: ") + path.string());
  }
  file.seekg(0, std::ios::beg);

  std::vector<uint8_t> buffer(size);
  if (!file.read((char*) buffer.data(), size))
  {
    throw ImreadException(std::string("Failed to read bytes of file ") + path.string());
  }

  if (ends_with(path.string(), ".jpg") || ends_with(path.string(), ".jpeg"))
  {
    #define HEX(x) std::setw(2) << std::setfill('0') << std::hex << (int) (x)
    if (buffer[0] != 0xFF || buffer[1] != 0xD8)
    {
      throw ImreadException(XTI_TO_STRING("Loaded jpeg with invalid start marker " << HEX(buffer[0]) << " " << HEX(buffer[1]) << " from file " << path.string()));
    }
    else if (buffer[buffer.size() - 2] != 0xFF || buffer[buffer.size() - 1] != 0xD9)
    {
     throw ImreadException(XTI_TO_STRING("Loaded jpeg with invalid end marker " << HEX(buffer[buffer.size() - 2]) << " " << HEX(buffer[buffer.size() - 1]) << " from file " << path.string()));
    }
    #undef HEX
  }

  cv::Mat data_cv(1, buffer.size(), xti::opencv::pixeltype<uint8_t>::get(1), buffer.data());
  if (data_cv.data == NULL)
  {
    throw ImreadException("Failed to convert data array of file " + path.string() + " to cv mat");
  }

  cv::Mat image_cv = cv::imdecode(data_cv, cv::IMREAD_COLOR);
  if (image_cv.data == NULL)
  {
    throw ImreadException("Failed to decode image from file " + path.string());
  }

  return image_cv;
}

class WriteFileException : public std::exception
{
public:
  WriteFileException(std::filesystem::path path, std::string message)
    : m_message(std::string("Failed to write file ") + path.string() + ". Reason: " + message)
  {
  }

  WriteFileException(std::filesystem::path path)
    : m_message(std::string("Failed to write file ") + path.string())
  {
  }

  virtual const char* what() const throw ()
  {
    return m_message.c_str();
  }

private:
  std::string m_message;
};

class LoadFileException : public LoadTileException
{
public:
  LoadFileException(std::filesystem::path path, std::string message)
    : m_message(std::string("Failed to load file ") + path.string() + ". Reason: " + message)
  {
  }

  virtual const char* what() const throw ()
  {
    return m_message.c_str();
  }

private:
  std::string m_message;
};

class FileNotFoundException : public LoadFileException
{
public:
  FileNotFoundException(std::filesystem::path path)
    : LoadFileException(path, "File not found")
  {
  }
};

class Disk : public TileLoader, public Cache
{
public:
  struct Mutex
  {
    std::shared_mutex mutex;

    Mutex()
    {
    }

    Mutex(const Mutex& other)
    {
    }

    Mutex& operator=(const Mutex& other)
    {
      return *this;
    }
  };

  Disk(std::filesystem::path path, const Layout& layout, int min_zoom, int max_zoom, float wait_after_last_modified = 1.0)
    : TileLoader(layout)
    , Cache()
    , m_min_zoom(min_zoom)
    , m_max_zoom(max_zoom)
    , m_wait_after_last_modified(wait_after_last_modified)
  {
    if (path.string().find("{") == std::string::npos)
    {
      path = path / "{zoom}" / "{x}" / "{y}.jpg";
    }
    m_path = path;
  }

  int get_min_zoom() const
  {
    return m_min_zoom;
  }

  int get_max_zoom() const
  {
    return m_max_zoom;
  }

  std::filesystem::path get_path(xti::vec2i tile, int zoom) const
  {
    if (zoom > m_max_zoom)
    {
      throw LoadTileException("Zoom level " + XTI_TO_STRING(zoom) + " is higher than the maximum zoom level " + XTI_TO_STRING(m_max_zoom) + ".");
    }
    return replace_placeholders(m_path, this->get_layout(), tile, zoom);
  }

  bool contains(xti::vec2i tile, int zoom) const
  {
    return zoom <= m_max_zoom && std::filesystem::exists(get_path(tile, zoom));
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
    std::shared_lock<std::shared_mutex> lock(m_mutex.mutex);

    std::filesystem::path path = get_path(tile, zoom);
    if (!std::filesystem::exists(path))
    {
      throw FileNotFoundException(path);
    }

    auto last_modified_time = std::filesystem::last_write_time(path);
    auto now_time = std::filesystem::__file_clock::now();
    auto sleep_duration = std::chrono::duration<float>(m_wait_after_last_modified) - (now_time - last_modified_time);
    if (sleep_duration > std::chrono::seconds(0))
    {
      std::this_thread::sleep_for(sleep_duration);
    }

    cv::Mat image_cv = safe_imread(path);

    try
    {
      this->to_tile(image_cv, true);
    }
    catch (LoadTileException ex)
    {
      throw LoadFileException(path, std::string("Loaded invalid tile. ") + ex.what());
    }
    return image_cv;
  }

  void save(const cv::Mat& image, xti::vec2i tile, int zoom)
  {
    if (zoom > m_max_zoom)
    {
      throw LoadTileException("Zoom level " + XTI_TO_STRING(zoom) + " is higher than the maximum zoom level " + XTI_TO_STRING(m_max_zoom) + ".");
    }
    if (zoom < m_min_zoom)
    {
      throw LoadTileException("Zoom level " + XTI_TO_STRING(zoom) + " is lower than the minimum zoom level " + XTI_TO_STRING(m_min_zoom) + ".");
    }
    std::lock_guard<std::shared_mutex> lock(m_mutex.mutex);

    std::filesystem::path path = get_path(tile, zoom);

    std::filesystem::path parent_path = path.parent_path();
    if (!std::filesystem::exists(parent_path))
    {
      std::filesystem::create_directories(parent_path);
    }

    cv::Mat image_bgr;
    cv::cvtColor(image, image_bgr, cv::COLOR_RGB2BGR);

    if (!cv::imwrite(path.string(), image_bgr))
    {
      throw WriteFileException(path);
    }
  }

  std::filesystem::path get_path() const
  {
    return m_path;
  }

private:
  std::filesystem::path m_path;
  int m_min_zoom;
  int m_max_zoom;
  float m_wait_after_last_modified;
  Mutex m_mutex;
};

} // end of ns tiledwebmaps
