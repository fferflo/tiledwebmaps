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

class LoadFileException : public std::exception
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

class Disk : public Cache
{
public:
  Disk(std::filesystem::path path, const Layout& layout, float wait_after_last_modified = 1.0)
    : Cache(layout)
    , m_wait_after_last_modified(wait_after_last_modified)
  {
    if (path.string().find("{") == std::string::npos)
    {
      path = path / "{zoom}" / "{x}" / "{y}.jpg";
    }
    m_path = path;
  }

  std::filesystem::path get_path(xti::vec2s tile, size_t zoom) const
  {
    return replace_placeholders(m_path, this->get_layout(), tile, zoom);
  }

  bool contains(xti::vec2s tile, size_t zoom) const
  {
    return std::filesystem::exists(get_path(tile, zoom));
  }

  Tile load(xti::vec2s tile, size_t zoom)
  {
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
    auto image_bgr = xti::from_opencv<uint8_t>(std::move(image_cv));
    auto image_rgb = xt::view(std::move(image_bgr), xt::all(), xt::all(), xt::range(xt::placeholders::_, xt::placeholders::_, -1));
    try
    {
      return this->to_tile(std::move(image_rgb));
    }
    catch (LoadTileException ex)
    {
      throw LoadFileException(path, std::string("Loaded invalid tile. ") + ex.what());
    }
  }

  void save(const Tile& image, xti::vec2s tile, size_t zoom)
  {
    std::filesystem::path path = get_path(tile, zoom);

    std::filesystem::path parent_path = path.parent_path();
    if (!std::filesystem::exists(parent_path))
    {
      std::filesystem::create_directories(parent_path);
    }

    auto image_bgr = xt::view(image, xt::all(), xt::all(), xt::range(xt::placeholders::_, xt::placeholders::_, -1));
    if (!cv::imwrite(path.string(), xti::to_opencv(std::move(image_bgr))))
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
  float m_wait_after_last_modified;
};

} // end of ns tiledwebmaps
