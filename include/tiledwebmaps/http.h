#pragma once

#include <xti/typedefs.h>
#include <xti/exception.h>
#include <xti/opencv.h>
#include <opencv2/imgcodecs.hpp>
#include <tiledwebmaps/tileloader.h>
#include <curl_easy.h>
#include <curl_header.h>
#include <thread>
#include <chrono>
#include <mutex>

namespace tiledwebmaps {

class Http : public TileLoader
{
public:
  struct Mutex
  {
    std::mutex mutex;

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

  Http(std::string url, const Layout& layout, size_t retries = 10, float wait_after_error = 1.5, bool verify_ssl = true, std::optional<std::filesystem::path> capath = std::optional<std::filesystem::path>(), std::optional<std::filesystem::path> cafile = std::optional<std::filesystem::path>(), std::map<std::string, std::string> header = std::map<std::string, std::string>(), bool allow_multithreading = false)
    : TileLoader(layout)
    , m_url(url)
    , m_retries(retries)
    , m_wait_after_error(wait_after_error)
    , m_verify_ssl(verify_ssl)
    , m_capath(capath)
    , m_cafile(cafile)
    , m_header(header)
    , m_allow_multithreading(allow_multithreading)
    , m_mutex()
  {
  }

  Tile load(xti::vec2s tile, size_t zoom)
  {
    auto lock = m_allow_multithreading ? std::unique_lock<std::mutex>() : std::unique_lock<std::mutex>(m_mutex.mutex);

    std::string url = this->get_url(tile, zoom);

    LoadTileException last_ex;
    for (size_t tries = 0; tries < m_retries; ++tries)
    {
      if (tries > 0)
      {
        std::this_thread::sleep_for(std::chrono::duration<float>(m_wait_after_error));
      }
      try
      {
        // Retrieve data from url
        curl::curl_easy request;
        request.add<CURLOPT_URL>(url.c_str());
        request.add<CURLOPT_FOLLOWLOCATION>(1L);

        curl::curl_header curl_header;
        for (const auto& pair : m_header)
        {
          curl_header.add(pair.first + ": " + pair.second);
        }
        request.add<CURLOPT_HTTPHEADER>(curl_header.get());

        std::ostringstream header_stream, body_stream;
        curl::curl_ios<std::ostringstream> curl_header_stream(header_stream);
        curl::curl_ios<std::ostringstream> curl_body_stream(body_stream);
        request.add<CURLOPT_HEADERFUNCTION>(curl_header_stream.get_function());
        request.add<CURLOPT_HEADERDATA>(curl_header_stream.get_stream());
        request.add<CURLOPT_WRITEFUNCTION>(curl_body_stream.get_function());
        request.add<CURLOPT_WRITEDATA>(curl_body_stream.get_stream());
        if (!m_verify_ssl)
        {
          request.add<CURLOPT_SSL_VERIFYHOST>(0);
          request.add<CURLOPT_SSL_VERIFYPEER>(0);
        }
        if (m_capath)
        {
          request.add<CURLOPT_CAPATH>(m_capath->string().c_str());
        }
        else if (m_cafile)
        {
          request.add<CURLOPT_CAINFO>(m_cafile->string().c_str());
        }

        request.perform();

        // Convert data to image
        std::string data = body_stream.str();
        if (data.length() == 0)
        {
          last_ex = LoadTileException("Failed to download image from url " + url + ". Received no data.");
          continue;
        }
        cv::Mat data_cv(1, data.length(), xti::opencv::pixeltype<uint8_t>::get(1), data.data());
        if (data_cv.data == NULL)
        {
          last_ex = LoadTileException("Failed to download image from url " + url);
          continue;
        }
        cv::Mat image_cv = cv::imdecode(data_cv, cv::IMREAD_COLOR);
        if (image_cv.data == NULL)
        {
          last_ex = LoadTileException("Failed to decode downloaded image from url " + url + ". Received " + XTI_TO_STRING(data.length()) + " bytes: " + data);
          continue;
        }
        auto image_bgr = xti::from_opencv<uint8_t>(std::move(image_cv));
        auto image_rgb = xt::view(std::move(image_bgr), xt::all(), xt::all(), xt::range(xt::placeholders::_, xt::placeholders::_, -1));
        try
        {
          return to_tile(std::move(image_rgb));
        }
        catch (LoadTileException ex)
        {
          last_ex = LoadTileException(std::string("Downloaded invalid tile. ") + ex.what());
        }
      }
      catch (curl::curl_easy_exception ex)
      {
        last_ex = LoadTileException(std::string("Failed to download image. Reason: ") + ex.what());
      }
    }
    throw last_ex;
  }

  std::string get_url(xti::vec2s tile, size_t zoom) const
  {
    return replace_placeholders(m_url, this->get_layout(), tile, zoom);
  }

private:
  std::string m_url;
  size_t m_retries;
  float m_wait_after_error;
  bool m_verify_ssl;
  std::optional<std::filesystem::path> m_capath;
  std::optional<std::filesystem::path> m_cafile;
  std::map<std::string, std::string> m_header;
  bool m_allow_multithreading;
  Mutex m_mutex;
};

} // end of ns tiledwebmaps
