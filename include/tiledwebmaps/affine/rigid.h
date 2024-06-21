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

namespace tiledwebmaps {

template <typename TScalar, size_t TRank>
class Rigid
{
private:
  xti::matXT<TScalar, TRank> m_rotation;
  xti::vecXT<TScalar, TRank> m_translation;

public:
  Rigid()
    : m_rotation(xt::eye<TScalar>(TRank))
  {
    m_translation.fill(0);
  }

  Rigid(xti::matXT<TScalar, TRank + 1> transformation_matrix)
    : m_rotation(xt::view(transformation_matrix, xt::range(0, TRank), xt::range(0, TRank)))
    , m_translation(xt::view(transformation_matrix, xt::range(0, TRank), TRank))
  {
    // TODO: check that all other elements of matrix are 0, with epsilon
    // if (xt::view(transformation_matrix, TRank, xt::range(0, TRank)) != 0 || transformation_matrix(TRank, TRank) != 1)
  }

  Rigid(xti::matXT<TScalar, TRank> rotation, xti::vecXT<TScalar, TRank> translation)
    : m_rotation(rotation)
    , m_translation(translation)
  {
  }

  template <bool TDummy = true, typename = std::enable_if_t<TDummy && TRank == 2, void>>
  Rigid(TScalar angle, xti::vecXT<TScalar, TRank> translation)
    : m_rotation(angle_to_rotation_matrix(angle))
    , m_translation(translation)
  {
  }

  template <typename TScalar2>
  Rigid(const Rigid<TScalar2, TRank>& other)
    : m_rotation(other.m_rotation)
    , m_translation(other.m_translation)
  {
  }

  template <typename TScalar2>
  Rigid<TScalar, TRank>& operator=(const Rigid<TScalar2, TRank>& other)
  {
    this->m_rotation = other.m_rotation;
    this->m_translation = other.m_translation;
    return *this;
  }

  auto transform(xti::vecXT<TScalar, TRank> point) const
  {
    return xt::linalg::dot(m_rotation, point) + m_translation;
  }

  template <typename TTensor>
  auto transform_all(TTensor&& points) const
  {
    if (points.shape()[1] != TRank)
    {
      throw std::invalid_argument(XTI_TO_STRING("Points tensor must have shape (n, " << TRank << "), got shape " << xt::adapt(points.shape())));
    }
    return xt::transpose(xt::eval(xt::linalg::dot(m_rotation, xt::transpose(xt::eval(std::forward<TTensor>(points)), {1, 0})) + xt::view(m_translation, xt::all(), xt::newaxis())), {1, 0});
  }

  auto transform_inverse(xti::vecXT<TScalar, TRank> point) const
  {
    return xt::linalg::dot(xt::transpose(m_rotation, {1, 0}), point - m_translation);
  }

  template <typename TTensor>
  auto transform_all_inverse(TTensor&& points) const
  {
    if (points.shape()[1] != TRank)
    {
      throw std::invalid_argument(XTI_TO_STRING("Points tensor must have shape (n, " << TRank << "), got shape " << xt::adapt(points.shape())));
    }
    return xt::transpose(xt::eval(xt::linalg::dot(xt::transpose(m_rotation, {1, 0}), xt::transpose(xt::eval(std::forward<TTensor>(points)), {1, 0}) - xt::view(m_translation, xt::all(), xt::newaxis())), {1, 0}));
  }

  Rigid<TScalar, TRank> inverse() const
  {
    Rigid<TScalar, TRank> result;
    result.get_rotation() = xt::transpose(m_rotation, {1, 0});
    result.get_translation() = xt::linalg::dot(result.get_rotation(), -m_translation);
    return result;
  }

  Rigid<TScalar, TRank>& operator*=(const Rigid<TScalar, TRank>& right)
  {
    m_translation = this->transform(right.get_translation());
    m_rotation = xt::linalg::dot(m_rotation, right.get_rotation());
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

  xti::matXT<TScalar, TRank + 1> to_matrix() const
  {
    xti::matXT<TScalar, TRank + 1> result;
    for (int32_t r = 0; r < TRank; r++)
    {
      for (int32_t c = 0; c < TRank; c++)
      {
        result(r, c) = m_rotation(r, c);
      }
      result(r, TRank) = m_translation(r);
      result(TRank, r) = 0;
    }
    result(TRank, TRank) = 1;
    return result;
  }

  template <typename TScalar2, size_t TRank2>
  friend class Rigid;
};

template <typename TScalar, size_t TRank>
Rigid<TScalar, TRank> operator*(const Rigid<TScalar, TRank>& left, const Rigid<TScalar, TRank>& right)
{
  return Rigid<TScalar, TRank>(xt::linalg::dot(left.get_rotation(), right.get_rotation()), left.transform(right.get_translation()));
}

template <typename TScalar, size_t TRank>
Rigid<TScalar, TRank> operator/(const Rigid<TScalar, TRank>& left, const Rigid<TScalar, TRank>& right)
{
  return left * right.inverse();
}

template <typename TScalar>
Rigid<TScalar, 3> slerp(const Rigid<TScalar, 3>& first, const Rigid<TScalar, 3>& second, TScalar alpha)
{
  return Rigid<TScalar, 3>(
    slerp(first.get_rotation(), second.get_rotation(), alpha),
    first.get_translation() + alpha * (second.get_translation() - first.get_translation())
  );
}

template <typename TScalar, size_t TRank>
std::ostream& operator<<(std::ostream& stream, const Rigid<TScalar, TRank>& transform)
{
  return stream << "Rigid(t=" << transform.get_translation() << " R=" << transform.get_rotation() << ")";
}

} // tiledwebmaps
