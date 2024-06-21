#pragma once

#include <memory>
#include <proj.h>
#include <xti/typedefs.h>
#include <xti/util.h>
#include <tiledwebmaps/affine.h>
#include <tiledwebmaps/geo.h>
#include <thread>
#include <mutex>

namespace tiledwebmaps::proj {

class Transformer;
class CRS;
class Context;

class Exception : public std::exception
{
public:
  Exception(std::string message, Context& context);

  Exception(std::string message, std::shared_ptr<Context> context)
    : Exception(message, *context)
  {
  }

  Exception(std::string message)
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

class Context final
{
public:
  Context(bool use_default_context = false, std::optional<std::string> proj_data_path = std::optional<std::string>())
  {
    if (!use_default_context)
    {
      m_handle = proj_context_create();
      if (!m_handle)
      {
        throw Exception("Failed to create context.");
      }

      if (proj_data_path)
      {
        const char* paths[1];
        paths[0] = proj_data_path->c_str();
        proj_context_set_search_paths(m_handle, 1, paths);
      }
    }
    else
    {
      if (proj_data_path)
      {
        throw Exception("proj_data_path cannot be given for default context");
      }
      m_handle = NULL;
    }
  }

  ~Context()
  {
    if (m_handle != NULL)
    {
      proj_context_destroy(m_handle);
      m_handle = NULL;
    }
  }

  Context(const Context&) = delete;
  Context& operator=(const Context&) = delete;

  friend class tiledwebmaps::proj::CRS;
  friend class tiledwebmaps::proj::Transformer;
  friend class Exception;

private:
  PJ_CONTEXT* m_handle;
  std::mutex m_mutex;
};

Exception::Exception(std::string message, Context& context)
{
  std::lock_guard<std::mutex> lock(context.m_mutex);
  int32_t error = proj_context_errno(context.m_handle);
  std::string str;
  if (error == 0)
  {
    str = "Unknown";
  }
  else
  {
    str = proj_context_errno_string(context.m_handle, error);
  }
  m_message = message + "\nReason: " + str;
}

class CRS
{
public:
  struct AreaOfUse
  {
    xti::vec2d lower_latlon;
    xti::vec2d upper_latlon;
  };

  struct AxisInfo
  {
    size_t axis_index;
    std::string out_name;
    std::string out_abbrev;
    std::string out_direction;
    double out_unit_conv_factor;
    std::string out_unit_name;
    std::string out_unit_auth_name;
    std::string out_unit_code;
  };

  CRS(std::string description)
    : CRS(std::make_shared<Context>(), description)
  {
  }

  CRS(std::shared_ptr<Context> context, std::string description)
    : m_context(context)
    , m_description(description)
  {
    std::lock_guard<std::mutex> lock(m_context->m_mutex);

    // Create CRS
    PJ* handle = proj_create(context->m_handle, description.c_str());
    if (!handle)
    {
      throw Exception("Failed to create CRS.", context);
    }
    m_handle = std::shared_ptr<PJ>(handle, [](PJ* handle){proj_destroy(handle);});

    // Create CS
    PJ* handle_cs = proj_crs_get_coordinate_system(m_context->m_handle, m_handle.get());
    if (!handle_cs)
    {
      throw Exception("Failed to acquire cs via proj_crs_get_coordinate_system");
    }
    m_handle_cs = std::shared_ptr<PJ>(handle_cs, [](PJ* handle_cs){proj_destroy(handle_cs);});

    // Get axes info
    int axis_num = proj_cs_get_axis_count(m_context->m_handle, m_handle_cs.get());
    if (axis_num < 0)
    {
      throw Exception("Failed to get axis num via proj_cs_get_axis_count", m_context);
    }
    if (axis_num != 2)
    {
      throw Exception(XTI_TO_STRING("Expected number of axes 2, got " << axis_num));
    }
    std::vector<AxisInfo> axes_info;
    for (size_t axis_index = 0; axis_index < axis_num; ++axis_index)
    {
      const char* out_name;
      const char* out_abbrev;
      const char* out_direction;
      double out_unit_conv_factor;
      const char* out_unit_name;
      const char* out_unit_auth_name;
      const char* out_unit_code;
      if (!proj_cs_get_axis_info(m_context->m_handle, m_handle_cs.get(), axis_index, &out_name, &out_abbrev, &out_direction, &out_unit_conv_factor, &out_unit_name, &out_unit_auth_name, &out_unit_code))
      {
        throw Exception("Failed to get axis info via proj_cs_get_axis_info", m_context);
      }
      axes_info.push_back(AxisInfo{axis_index, out_name, out_abbrev, out_direction, out_unit_conv_factor, out_unit_name, out_unit_auth_name, out_unit_code});
    }
    m_axes = geo::CompassAxes(axes_info[0].out_direction, axes_info[1].out_direction);

    // Get area of use
    if (!proj_get_area_of_use(m_context->m_handle, m_handle.get(), &m_area_of_use.lower_latlon(1), &m_area_of_use.lower_latlon(0), &m_area_of_use.upper_latlon(1), &m_area_of_use.upper_latlon(0), NULL))
    {
      throw Exception("Failed to get area-of-use.", m_context);
    }
  }

  std::string get_description() const
  {
    return m_description;
  }

  AreaOfUse get_area_of_use() const
  {
    return m_area_of_use;
  }

  std::shared_ptr<Context> get_context() const
  {
    return m_context;
  }

  const geo::CompassAxes& get_axes() const
  {
    return m_axes;
  }

  bool operator==(const CRS& other) const
  {
    std::lock_guard<std::mutex> lock(m_context->m_mutex);
    return proj_is_equivalent_to_with_ctx(m_context->m_handle, this->m_handle.get(), other.m_handle.get(), PJ_COMP_EQUIVALENT);
  }

  bool operator!=(const CRS& other) const
  {
    return !(*this == other);
  }

  xti::vec2d get_vector(std::string direction) const
  {
    return m_axes.get_vector(direction);
  }

  // float get_meridian_convergence(xti::vec2d latlon)
  // {
  //   return get_factors(latlon).meridian_convergence; // radians
  // }

  friend class Transformer;

private:
  std::shared_ptr<Context> m_context;
  std::string m_description;
  std::shared_ptr<PJ> m_handle;
  std::shared_ptr<PJ> m_handle_cs;
  AreaOfUse m_area_of_use;
  geo::CompassAxes m_axes;

  // PJ_FACTORS get_factors(xti::vec2d latlon) const
  // {
  //   std::lock_guard<std::mutex> lock(m_context->m_mutex);
  //   PJ_COORD input_proj = proj_coord(tiledwebmaps::radians(latlon(1)), tiledwebmaps::radians(latlon(0)), 0, 0);
  //   PJ_FACTORS factors = proj_factors(m_handle.get(), input_proj);

  //   return factors;
  // }
};

class Transformer
{
public:
  struct ParamCRS
  {
    std::function<std::shared_ptr<CRS>(std::shared_ptr<Context>)> get;

    ParamCRS(std::shared_ptr<CRS> crs)
      : get([crs](std::shared_ptr<Context> context){return crs;})
    {
    }

    ParamCRS(std::string desc)
      : get([desc](std::shared_ptr<Context> context){return std::make_shared<CRS>(context, desc);})
    {
    }

    ParamCRS(const char* desc)
      : ParamCRS(std::string(desc))
    {
    }
  };

  Transformer(ParamCRS from_crs, ParamCRS to_crs)
    : Transformer(from_crs.get(std::make_shared<Context>())->get_context(), from_crs, to_crs)
  {
  }

  Transformer(std::shared_ptr<Context> context, ParamCRS from_crs, ParamCRS to_crs)
    : m_context(context)
    , m_from_crs(from_crs.get(context))
    , m_to_crs(to_crs.get(context))
    , m_axes_transformation(NamedAxesTransformation<double, 2>(m_from_crs->get_axes(), m_to_crs->get_axes()))
  {
    std::lock_guard<std::mutex> lock(m_context->m_mutex);
    PJ* handle = proj_create_crs_to_crs_from_pj(context->m_handle, m_from_crs->m_handle.get(), m_to_crs->m_handle.get(), NULL, NULL);
    if (!handle)
    {
      throw Exception("Failed to create Transformer.", context);
    }
    m_handle = std::shared_ptr<PJ>(handle, [](PJ* handle){proj_destroy(handle);});
  }

  xti::vec2d transform(xti::vec2d input) const
  {
    return transform(input, PJ_FWD);
  }

  double transform_angle(double angle) const
  {
    return rotation_matrix_to_angle(xt::linalg::dot(m_axes_transformation.get_rotation(), angle_to_rotation_matrix(angle)));
  }

  xti::vec2d transform_inverse(xti::vec2d input) const
  {
    return transform(input, PJ_INV);
  }

  double transform_angle_inverse(double angle) const
  {
    return rotation_matrix_to_angle(xt::linalg::dot(xt::transpose(m_axes_transformation.get_rotation(), {1, 0}), angle_to_rotation_matrix(angle)));
  }

  xti::vec2d operator()(xti::vec2d input) const
  {
    return transform(input);
  }

  std::shared_ptr<Context> get_context() const
  {
    return m_context;
  }

  std::shared_ptr<CRS> get_from_crs() const
  {
    return m_from_crs;
  }

  std::shared_ptr<CRS> get_to_crs() const
  {
    return m_to_crs;
  }

  std::shared_ptr<Transformer> inverse() const
  {
    return std::make_shared<Transformer>(m_context, m_to_crs, m_from_crs);
  }

  bool operator==(const Transformer& other) const
  {
    return this->m_from_crs == other.m_from_crs && this->m_to_crs == other.m_to_crs;
  }

  bool operator!=(const Transformer& other) const
  {
    return !(*this == other);
  }

private:
  std::shared_ptr<Context> m_context;
  std::shared_ptr<CRS> m_from_crs;
  std::shared_ptr<CRS> m_to_crs;
  std::shared_ptr<PJ> m_handle;
  NamedAxesTransformation<double, 2> m_axes_transformation;

  xti::vec2d transform(xti::vec2d input, PJ_DIRECTION direction) const
  {
    std::lock_guard<std::mutex> lock(m_context->m_mutex);
    PJ_COORD input_proj = proj_coord(input(0), input(1), 0, 0);
    PJ_COORD output_proj = proj_trans(m_handle.get(), direction, input_proj);
    return xti::vec2d({output_proj.v[0], output_proj.v[1]});
  }
};

tiledwebmaps::ScaledRigid<double, 2> eastnorthmeters_at_latlon_to_epsg3857(xti::vec2d latlon, const Transformer& epsg4326_to_epsg3857)
{
  double mercator_scale = std::cos(latlon[0] / 180.0 * xt::numeric_constants<double>::PI);
  return tiledwebmaps::ScaledRigid<double, 2>(xt::eye<double>(2), xti::vec2d({0, 0}), 1.0 / mercator_scale)
       * tiledwebmaps::ScaledRigid<double, 2>(xt::eye<double>(2), epsg4326_to_epsg3857(latlon) * mercator_scale);
}

tiledwebmaps::ScaledRigid<double, 2> geopose_to_epsg3857(xti::vec2d latlon, double bearing, const Transformer& epsg4326_to_epsg3857)
{
  tiledwebmaps::ScaledRigid<double, 2> transform = tiledwebmaps::proj::eastnorthmeters_at_latlon_to_epsg3857(latlon, epsg4326_to_epsg3857);
  transform.get_rotation() = tiledwebmaps::angle_to_rotation_matrix(epsg4326_to_epsg3857.transform_angle(tiledwebmaps::radians(bearing)));
  return transform;
}

} // end of ns tiledwebmaps::proj
