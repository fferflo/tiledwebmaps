#pragma once

#include <xti/typedefs.h>
#include <xti/util.h>
#include <xtensor/xadapt.hpp>
#include <xtensor/xview.hpp>
#include <xtensor/xtensor.hpp>
#include <xtensor/xio.hpp>
#include <xtensor-blas/xlinalg.hpp>

namespace tiledwebmaps {

template <typename TScalar, typename = std::enable_if_t<!xti::is_xtensor_v<TScalar>>>
TScalar radians(TScalar degrees)
{
  return degrees / 180 * xt::numeric_constants<TScalar>::PI;
}

template <typename TTensor, typename = std::enable_if_t<xti::is_xtensor_v<TTensor>>>
auto radians(TTensor&& tensor)
{
  return std::forward<TTensor>(tensor) / 180 * xt::numeric_constants<xti::elementtype_t<TTensor>>::PI;
}

template <typename TScalar, typename = std::enable_if_t<!xti::is_xtensor_v<TScalar>>>
TScalar degrees(TScalar degrees)
{
  return degrees * 180 / xt::numeric_constants<TScalar>::PI;
}

template <typename TTensor, typename = std::enable_if_t<xti::is_xtensor_v<TTensor>>>
auto degrees(TTensor&& tensor)
{
  return std::forward<TTensor>(tensor) * 180 / xt::numeric_constants<xti::elementtype_t<TTensor>>::PI;
}

template <typename TScalar>
xti::mat2T<std::decay_t<TScalar>> angle_to_rotation_matrix(TScalar angle)
{
  return {{std::cos(angle), -std::sin(angle)}, {std::sin(angle), std::cos(angle)}};
}

template <typename TRotationMatrix>
auto rotation_matrix_to_angle(TRotationMatrix&& rotation_matrix)
{
  return std::atan2(rotation_matrix(1, 0), rotation_matrix(0, 0));
}

template <typename TScalar>
xti::vecXT<TScalar, 4> rotation_matrix_to_quaternion(xti::mat3T<TScalar> rotation_matrix) // wxyz
{
  TScalar q0 = std::sqrt(std::max(TScalar(0.25) * (1 + rotation_matrix(0, 0) + rotation_matrix(1, 1) + rotation_matrix(2, 2)), TScalar(0)));
  TScalar q1 = std::sqrt(std::max(TScalar(0.25) * (1 + rotation_matrix(0, 0) - rotation_matrix(1, 1) - rotation_matrix(2, 2)), TScalar(0)));
  TScalar q2 = std::sqrt(std::max(TScalar(0.25) * (1 - rotation_matrix(0, 0) + rotation_matrix(1, 1) - rotation_matrix(2, 2)), TScalar(0)));
  TScalar q3 = std::sqrt(std::max(TScalar(0.25) * (1 - rotation_matrix(0, 0) - rotation_matrix(1, 1) + rotation_matrix(2, 2)), TScalar(0)));

  #define PSIGN(i, j) ((rotation_matrix(i, j) + rotation_matrix(j, i) >= 0) ? 1.0f : -1.0f)
  #define NSIGN(i, j) ((rotation_matrix(i, j) - rotation_matrix(j, i) >= 0) ? 1.0f : -1.0f)

	if (q0 >= q1 && q0 >= q2 && q0 >= q3)
  {
		// q0 *= 1.0f;
		q1 *= NSIGN(2, 1);
		q2 *= NSIGN(0, 2);
		q3 *= NSIGN(1, 0);
	}
	else if (q1 >= q0 && q1 >= q2 && q1 >= q3)
  {
		q0 *= NSIGN(2, 1);
		// q1 *= 1.0f;
		q2 *= PSIGN(1, 0);
		q3 *= PSIGN(0, 2);
	}
	else if (q2 >= q0 && q2 >= q1 && q2 >= q3)
  {
		q0 *= NSIGN(0, 2);
		q1 *= PSIGN(1, 0);
		// q2 *= 1.0f;
		q3 *= PSIGN(2, 1);
	}
	else // if (q3 >= q0 && q3 >= q1 && q3 >= q2)
  {
		q0 *= NSIGN(1, 0);
		q1 *= PSIGN(2, 0);
		q2 *= PSIGN(2, 1);
		// q3 *= 1.0f;
	}

  #undef PSIGN
  #undef NSIGN

  xti::vecXT<TScalar, 4> quaternion({q0, q1, q2, q3});
  quaternion = quaternion / xt::linalg::norm(quaternion);
  return quaternion;
}

template <typename TScalar>
xti::mat3T<TScalar> quaternion_to_rotation_matrix(xti::vecXT<TScalar, 4> quaternion) // wxyz
{
  xti::mat3T<TScalar> rotation_matrix;
  rotation_matrix(0, 0) = 1 - 2 * quaternion(2) * quaternion(2) - 2 * quaternion(3) * quaternion(3);
  rotation_matrix(1, 1) = 1 - 2 * quaternion(1) * quaternion(1) - 2 * quaternion(3) * quaternion(3);
  rotation_matrix(2, 2) = 1 - 2 * quaternion(1) * quaternion(1) - 2 * quaternion(2) * quaternion(2);
  rotation_matrix(0, 1) = 2 * quaternion(1) * quaternion(2) - 2 * quaternion(3) * quaternion(0);
  rotation_matrix(1, 0) = 2 * quaternion(1) * quaternion(2) + 2 * quaternion(3) * quaternion(0);
  rotation_matrix(2, 0) = 2 * quaternion(1) * quaternion(3) - 2 * quaternion(2) * quaternion(0);
  rotation_matrix(0, 2) = 2 * quaternion(1) * quaternion(3) + 2 * quaternion(2) * quaternion(0);
  rotation_matrix(1, 2) = 2 * quaternion(2) * quaternion(3) - 2 * quaternion(1) * quaternion(0);
  rotation_matrix(2, 1) = 2 * quaternion(2) * quaternion(3) + 2 * quaternion(1) * quaternion(0);
  return rotation_matrix;
}

template <typename TScalar>
xti::vecXT<TScalar, 4> slerp(xti::vecXT<TScalar, 4> quaternion1, xti::vecXT<TScalar, 4> quaternion2, TScalar alpha)
{
  TScalar dot = xt::linalg::dot(quaternion1, quaternion2)();

  if (dot < 0)
  {
    dot = -dot;
    quaternion2 = -quaternion2;
  }

  xti::vecXT<TScalar, 4> result;
  if (dot > 0.9999)
  {
    result = quaternion1 + alpha * (quaternion2 - quaternion1);
  }
  else
  {
    TScalar theta_0 = std::acos(dot);
    TScalar sin_theta_0 = std::sin(theta_0);

    TScalar theta = theta_0 * alpha;
    TScalar sin_theta = std::sin(theta);

    TScalar s1 = std::cos(theta) - dot * sin_theta / sin_theta_0;
    TScalar s2 = sin_theta / sin_theta_0;

    result = s1 * quaternion1 + s2 * quaternion2;
  }
  result = result / xt::linalg::norm(result);

  return result;
}

template <typename TScalar>
xti::matXT<TScalar, 3> slerp(xti::matXT<TScalar, 3> rotation_matrix1, xti::matXT<TScalar, 3> rotation_matrix2, TScalar alpha)
{
  return quaternion_to_rotation_matrix(slerp(rotation_matrix_to_quaternion(rotation_matrix1), rotation_matrix_to_quaternion(rotation_matrix2), alpha));
}

template <typename TScalar>
xti::vecXT<TScalar, 4> axisangle_to_quaternion(xti::vecXT<TScalar, 3> axis, TScalar angle)
{
  axis = axis / (TScalar) xt::linalg::norm(axis);
  TScalar theta = angle / 2;
  TScalar sin_theta = std::sin(theta);

  xti::vecXT<TScalar, 4> result;
  result(0) = std::cos(theta);
  result(1) = axis(0) * sin_theta;
  result(2) = axis(1) * sin_theta;
  result(3) = axis(2) * sin_theta;

  return result;
}

template <typename TScalar>
xti::matXT<TScalar, 3> axisangle_to_rotation_matrix(xti::vecXT<TScalar, 3> axis, TScalar angle)
{
  return quaternion_to_rotation_matrix(axisangle_to_quaternion(axis, angle));
}

template <typename TScalar>
auto normalize_angle(TScalar angle, TScalar lower = -xt::numeric_constants<TScalar>::PI, TScalar upper = xt::numeric_constants<TScalar>::PI)
{
  static const TScalar pi = xt::numeric_constants<TScalar>::PI;
  while (angle >= upper)
  {
    angle -= 2 * pi;
  }
  while (angle < lower)
  {
    angle += 2 * pi;
  }
  return angle;
}

template <typename TVec1, typename TVec2>
auto angle_between_vectors(TVec1&& vec1, TVec2&& vec2, bool clockwise = false)
{
  auto angle = std::atan2(vec2(1), vec2(0)) - std::atan2(vec1(1), vec1(0));

  return clockwise ? -angle : angle;
}

template <typename TScalar>
xti::matXT<TScalar, 3> rpy_to_rotation_matrix(TScalar r, TScalar p, TScalar y)
{
  // https://en.wikipedia.org/wiki/Rotation_matrix#General_3D_rotations
  TScalar a = y;
  TScalar b = p;
  TScalar c = r;
  xti::matXT<TScalar, 3> rotation_matrix;
  rotation_matrix(0, 0) = std::cos(a) * std::cos(b);
  rotation_matrix(0, 1) = std::cos(a) * std::sin(b) * std::sin(c) - std::sin(a) * std::cos(c);
  rotation_matrix(0, 2) = std::cos(a) * std::sin(b) * std::cos(c) + std::sin(a) * std::sin(c);
  rotation_matrix(1, 0) = std::sin(a) * std::cos(b);
  rotation_matrix(1, 1) = std::sin(a) * std::sin(b) * std::sin(c) + std::cos(a) * std::cos(c);
  rotation_matrix(1, 2) = std::sin(a) * std::sin(b) * std::cos(c) - std::cos(a) * std::sin(c);
  rotation_matrix(2, 0) = -std::sin(b);
  rotation_matrix(2, 1) = std::cos(b) * std::sin(c);
  rotation_matrix(2, 2) = std::cos(b) * std::cos(c);
  return rotation_matrix;
}

template <typename TScalar>
xti::matXT<TScalar, 3> rpy_to_rotation_matrix(xti::vecXT<TScalar, 3> rpy)
{
  return rpy_to_rotation_matrix(rpy(0), rpy(1), rpy(2));
}

template <typename TScalar, size_t TRank>
class Rotation
{
private:
  xti::matXT<TScalar, TRank> m_rotation;

public:
  Rotation()
    : m_rotation(xt::eye<TScalar>(TRank))
  {
  }

  template <bool TDummy = true, typename = std::enable_if_t<TDummy && TRank == 2, void>>
  Rotation(TScalar angle)
    : m_rotation(angle_to_rotation_matrix(angle))
  {
  }

  Rotation(xti::matXT<TScalar, TRank + 1> transformation_matrix)
    : m_rotation(xt::view(transformation_matrix, xt::range(0, TRank), xt::range(0, TRank)))
  {
    // TODO: check that all other elements of matrix are 0, with epsilon
    // if (xt::view(transformation_matrix, TRank, xt::range(0, TRank)) != 0 || transformation_matrix(TRank, TRank) != 1)
  }

  Rotation(xti::matXT<TScalar, TRank> rotation)
    : m_rotation(rotation)
  {
  }

  template <typename TScalar2>
  Rotation(const Rotation<TScalar2, TRank>& other)
    : m_rotation(other.m_rotation)
  {
  }

  template <typename TScalar2>
  Rotation<TScalar, TRank>& operator=(const Rotation<TScalar2, TRank>& other)
  {
    this->m_rotation = other.m_rotation;
    return *this;
  }

  auto transform(xti::vecXT<TScalar, TRank> point) const
  {
    return xt::linalg::dot(m_rotation, point);
  }

  template <typename TTensor>
  auto transform_all(TTensor&& points) const
  {
    if (points.shape()[1] != TRank)
    {
      throw std::invalid_argument(XTI_TO_STRING("Points tensor must have shape (n, " << TRank << "), got shape " << xt::adapt(points.shape())));
    }
    return xt::transpose(xt::eval(xt::linalg::dot(m_rotation, xt::transpose(xt::eval(std::forward<TTensor>(points)), {1, 0}))), {1, 0});
  }

  auto transform_inverse(xti::vecXT<TScalar, TRank> point) const
  {
    return xt::linalg::dot(xt::transpose(m_rotation, {1, 0}), point);
  }

  template <typename TTensor>
  auto transform_all_inverse(TTensor&& points) const
  {
    if (points.shape()[1] != TRank)
    {
      throw std::invalid_argument(XTI_TO_STRING("Points tensor must have shape (n, " << TRank << "), got shape " << xt::adapt(points.shape())));
    }
    return xt::transpose(xt::eval(xt::linalg::dot(xt::transpose(m_rotation, {1, 0}), xt::transpose(xt::eval(std::forward<TTensor>(points)), {1, 0})), {1, 0}));
  }

  Rotation<TScalar, TRank> inverse() const
  {
    Rotation<TScalar, TRank> result;
    result.get_rotation() = xt::transpose(m_rotation, {1, 0});
    return result;
  }

  Rotation<TScalar, TRank>& operator*=(const Rotation<TScalar, TRank>& right)
  {
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

  bool flips() const
  {
    return xt::linalg::det(m_rotation) < 0;
  }

  template <typename TScalar2, size_t TRank2>
  friend class Rotation;
};

template <typename TScalar, size_t TRank>
Rotation<TScalar, TRank> operator*(const Rotation<TScalar, TRank>& left, const Rotation<TScalar, TRank>& right)
{
  return Rotation<TScalar, TRank>(xt::linalg::dot(left.get_rotation(), right.get_rotation()));
}

template <typename TScalar, size_t TRank>
Rotation<TScalar, TRank> operator/(const Rotation<TScalar, TRank>& left, const Rotation<TScalar, TRank>& right)
{
  return left * right.inverse();
}

template <typename TScalar, size_t TRank>
std::ostream& operator<<(std::ostream& stream, const Rotation<TScalar, TRank>& transform)
{
  return stream << "Rotation(" << " R=" << transform.get_rotation() << ")";
}

} // tiledwebmaps
