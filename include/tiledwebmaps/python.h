#pragma once

#include <xtensor-python/pytensor.hpp>
#include <pybind11/pybind11.h>

#include <tiledwebmaps/affine.h>

namespace py = pybind11;

namespace pybind11::detail {

template <typename TElementType, size_t TRank>
struct type_caster<tiledwebmaps::Rigid<TElementType, TRank>>
{
public:
  using Type = tiledwebmaps::Rigid<TElementType, TRank>;
  PYBIND11_TYPE_CASTER(Type, const_name("Rigid"));

  bool load(py::handle src, bool)
  {
    if (!src || !py::hasattr(src, "rotation") || !py::hasattr(src, "translation"))
    {
      return false;
    }

    xt::xtensor<TElementType, 2> rotation = src.attr("rotation").cast<xt::xtensor<TElementType, 2>>(); // TODO: assert batchsize == ()
    xt::xtensor<TElementType, 1> translation = src.attr("translation").cast<xt::xtensor<TElementType, 1>>();

    value = tiledwebmaps::Rigid<TElementType, TRank>(rotation, translation);

    return true;
  }

  static py::handle cast(tiledwebmaps::Rigid<TElementType, TRank> src, py::return_value_policy /* policy */, py::handle /* parent */)
  {
    py::module_ tiledwebmaps = py::module_::import("tiledwebmaps");
    py::object dest = tiledwebmaps.attr("Rigid")(src.get_rotation(), src.get_translation());
    return dest.release();
  }
};

template <typename TElementType, size_t TRank>
struct type_caster<tiledwebmaps::ScaledRigid<TElementType, TRank>>
{
public:
  using Type = tiledwebmaps::ScaledRigid<TElementType, TRank>;
  PYBIND11_TYPE_CASTER(Type, const_name("ScaledRigid"));

  bool load(py::handle src, bool)
  {
    if (!src || !py::hasattr(src, "rotation") || !py::hasattr(src, "translation"))
    {
      return false;
    }

    xt::xtensor<TElementType, 2> rotation = src.attr("rotation").cast<xt::xtensor<TElementType, 2>>();
    xt::xtensor<TElementType, 1> translation = src.attr("translation").cast<xt::xtensor<TElementType, 1>>();
    TElementType scale = 1;
    if (py::hasattr(src, "scale"))
    {
      scale = src.attr("scale").cast<TElementType>();
    }

    value = tiledwebmaps::ScaledRigid<TElementType, TRank>(rotation, translation, scale);

    return true;
  }

  static py::handle cast(tiledwebmaps::ScaledRigid<TElementType, TRank> src, py::return_value_policy /* policy */, py::handle /* parent */)
  {
    py::module_ tiledwebmaps = py::module_::import("tiledwebmaps");
    py::object dest = tiledwebmaps.attr("ScaledRigid")(src.get_rotation(), src.get_translation(), src.get_scale());
    return dest.release();
  }
};

} // end of ns pybind11::detail
