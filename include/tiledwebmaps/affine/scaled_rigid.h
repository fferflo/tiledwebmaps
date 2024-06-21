#pragma once

#include <xti/typedefs.h>
#include <xti/util.h>
#include <xtensor/xadapt.hpp>
#include <xtensor/xview.hpp>
#include <xtensor/xtensor.hpp>
#include <xtensor/xarray.hpp>
#include <xtensor/xio.hpp>
#include <xtensor-blas/xlinalg.hpp>
#include <tiledwebmaps/affine/rotation.h>
#include <tiledwebmaps/affine/rigid.h>

namespace tiledwebmaps {

template <typename TScalar, size_t TRank, bool TSingleScale = true>
class ScaledRigid
{
private:
  using Scale = std::conditional_t<TSingleScale, TScalar, xti::vecXT<TScalar, TRank>>;

  static Scale scale_one()
  {
    if constexpr (TSingleScale)
    {
      return 1;
    }
    else
    {
      return xt::ones<TScalar>({TRank});
    }
  }

  xti::matXT<TScalar, TRank> m_rotation;
  xti::vecXT<TScalar, TRank> m_translation;
  Scale m_scale;

public:
  ScaledRigid()
    : m_rotation(xt::eye<TScalar>(TRank))
    , m_scale(scale_one())
  {
    m_translation.fill(0);
  }

  template <typename TScalar2>
  ScaledRigid(const Rigid<TScalar2, TRank>& other)
    : m_rotation(other.get_rotation())
    , m_translation(other.get_translation())
    , m_scale(scale_one())
  {
  }

  template <typename TScalar2>
  ScaledRigid(const Rotation<TScalar2, TRank>& other)
    : m_rotation(other.get_rotation())
    , m_scale(scale_one())
  {
    m_translation.fill(0);
  }

  ScaledRigid(xti::matXT<TScalar, TRank> rotation, xti::vecXT<TScalar, TRank> translation, Scale scale = scale_one())
    : m_rotation(rotation)
    , m_translation(translation)
    , m_scale(scale)
  {
  }

  template <bool TDummy = true, typename = std::enable_if_t<TDummy && TRank == 2, void>>
  ScaledRigid(TScalar angle, xti::vecXT<TScalar, TRank> translation, Scale scale)
    : m_rotation(angle_to_rotation_matrix(angle))
    , m_translation(translation)
    , m_scale(scale)
  {
  }

  template <typename TScalar2, bool TSingleScale2>
  ScaledRigid(const ScaledRigid<TScalar2, TRank, TSingleScale2>& other)
    : m_rotation(other.m_rotation)
    , m_translation(other.m_translation)
    , m_scale(other.m_scale)
  {
  }

  template <typename TScalar2, bool TSingleScale2>
  ScaledRigid<TScalar, TRank>& operator=(const ScaledRigid<TScalar2, TRank, TSingleScale2>& other)
  {
    this->m_rotation = other.m_rotation;
    this->m_translation = other.m_translation;
    this->m_scale = other.m_scale;
    return *this;
  }

  auto transform(xti::vecXT<TScalar, TRank> point) const
  {
    return m_scale * xt::linalg::dot(m_rotation, point) + m_translation;
  }

  template <typename TTensor>
  auto transform_all(TTensor&& points) const
  {
    if (points.shape()[1] != TRank)
    {
      throw std::invalid_argument(XTI_TO_STRING("Points tensor must have shape (n, " << TRank << "), got shape " << xt::adapt(points.shape())));
    }
    return xt::transpose(xt::eval(m_scale * xt::linalg::dot(m_rotation, xt::transpose(xt::eval(std::forward<TTensor>(points)), {1, 0})) + xt::view(m_translation, xt::all(), xt::newaxis())), {1, 0});
  }

  auto transform_inverse(xti::vecXT<TScalar, TRank> point) const
  {
    return xt::linalg::dot(xt::transpose(m_rotation, {1, 0}), point - m_translation) / m_scale;
  }

  template <typename TTensor>
  auto transform_all_inverse(TTensor&& points) const
  {
    if (points.shape()[1] != TRank)
    {
      throw std::invalid_argument(XTI_TO_STRING("Points tensor must have shape (n, " << TRank << "), got shape " << xt::adapt(points.shape())));
    }
    return xt::transpose(xt::eval(xt::linalg::dot(xt::transpose(m_rotation, {1, 0}), xt::transpose(xt::eval(std::forward<TTensor>(points)), {1, 0}) - xt::view(m_translation, xt::all(), xt::newaxis())), {1, 0})) / m_scale;
  }

  ScaledRigid<TScalar, TRank> inverse() const
  {
    ScaledRigid<TScalar, TRank> result;
    result.get_rotation() = xt::transpose(m_rotation, {1, 0});
    result.get_translation() = xt::linalg::dot(result.get_rotation(), -m_translation) / m_scale;
    result.get_scale() = 1 / m_scale;
    return result;
  }

  ScaledRigid<TScalar, TRank>& operator*=(const ScaledRigid<TScalar, TRank>& right)
  {
    m_translation = this->transform(right.get_translation());
    m_rotation = xt::linalg::dot(m_rotation, right.get_rotation());
    m_scale = m_scale * right.m_scale;
    return *this;
  }

  xti::matXT<TScalar, TRank>& get_rotation()
  {
    return m_rotation;
  }

  const xti::matXT<TScalar, TRank>& get_rotation() const
  {
    return m_rotation;
  }

  xti::vecXT<TScalar, TRank>& get_translation()
  {
    return m_translation;
  }

  const xti::vecXT<TScalar, TRank>& get_translation() const
  {
    return m_translation;
  }

  Scale& get_scale()
  {
    return m_scale;
  }

  const Scale& get_scale() const
  {
    return m_scale;
  }

  xti::matXT<TScalar, TRank + 1> to_matrix() const
  {
    xti::matXT<TScalar, TRank + 1> result;
    for (int32_t r = 0; r < TRank; r++)
    {
      for (int32_t c = 0; c < TRank; c++)
      {
        if constexpr (TSingleScale)
        {
          result(r, c) = m_rotation(r, c) * m_scale;
        }
        else
        {
          result(r, c) = m_rotation(r, c) * m_scale[c];
        }
      }
      result(r, TRank) = m_translation(r);
      result(TRank, r) = 0;
    }
    result(TRank, TRank) = 1;
    return result;
  }

  template <typename TScalar2, size_t TRank2, bool TSingleScale2>
  friend class ScaledRigid;
};

template <typename TScalar, size_t TRank, bool TSingleScale>
ScaledRigid<TScalar, TRank, TSingleScale> operator*(const ScaledRigid<TScalar, TRank, TSingleScale>& left, const ScaledRigid<TScalar, TRank, TSingleScale>& right)
{
  return ScaledRigid<TScalar, TRank, TSingleScale>(xt::linalg::dot(left.get_rotation(), right.get_rotation()), left.transform(right.get_translation()), left.get_scale() * right.get_scale());
}

template <typename TScalar, size_t TRank, bool TSingleScale>
ScaledRigid<TScalar, TRank, TSingleScale> operator/(const ScaledRigid<TScalar, TRank, TSingleScale>& left, const ScaledRigid<TScalar, TRank, TSingleScale>& right)
{
  return left * right.inverse();
}

template <typename TScalar, size_t TRank, bool TSingleScale>
std::ostream& operator<<(std::ostream& stream, const ScaledRigid<TScalar, TRank, TSingleScale>& transform)
{
  return stream << "ScaledRigid(t=" << transform.get_translation() << " R=" << transform.get_rotation() << " s=" << transform.get_scale() << ")";
}

} // tiledwebmaps
