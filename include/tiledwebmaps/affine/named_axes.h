#pragma once

#include <memory>
#include <xti/typedefs.h>
#include <xti/util.h>
#include <tiledwebmaps/affine.h>

namespace tiledwebmaps {

template <typename TScalar, size_t TRank>
class NamedAxesTransformation;

template <size_t TRank>
class NamedAxes
{
public:
  NamedAxes()
  {
  }

  NamedAxes(std::initializer_list<std::initializer_list<std::string>> axes_names)
    : m_axes_names(TRank)
  {
    if (axes_names.size() != TRank)
    {
      throw std::invalid_argument(XTI_TO_STRING("Expected " << TRank << " axes, got " << axes_names.size() << " axes"));
    }
    for (size_t i = 0; i < TRank; i++)
    {
      const std::initializer_list<std::string>& axis_initializer_list = axes_names.begin()[i];
      if (axis_initializer_list.size() != 2)
      {
        throw std::invalid_argument(XTI_TO_STRING("Expected 2 names per axis, got " << axis_initializer_list.size() << " names"));
      }
      m_axes_names[i].first = axis_initializer_list.begin()[0];
      m_axes_names[i].second = axis_initializer_list.begin()[1];
    }
  }

  const std::pair<std::string, std::string>& operator[](size_t i) const
  {
    return m_axes_names[i];
  }

  bool operator==(const NamedAxes& other) const
  {
    return this->m_axes_names == other.m_axes_names;
  }

  bool operator!=(const NamedAxes& other) const
  {
    return !(*this == other);
  }

  xti::vecXi<TRank> get_vector(std::string direction) const
  {
    xti::vecXi<TRank> vector;
    vector.fill(0);
    for (size_t i = 0; i < TRank; i++)
    {
      if (m_axes_names[i].first == direction)
      {
        vector[i] = 1;
        break;
      }
      else if (m_axes_names[i].second == direction)
      {
        vector[i] = -1;
        break;
      }
    }
    if (xt::all(xt::equal(vector, 0)))
    {
      throw std::invalid_argument("Invalid axis direction");
    }
    return vector;
  }

  template <typename TScalar2, size_t TRank2>
  friend class NamedAxesTransformation;

public:
  std::vector<std::pair<std::string, std::string>> m_axes_names;
};

template <size_t TRank>
std::ostream& operator<<(std::ostream& stream, const NamedAxes<TRank>& axes)
{
  stream << "NamedAxes[";
  for (size_t i = 0; i < TRank; i++)
  {
    if (i > 0)
    {
      stream << ", ";
    }
    stream << axes[i].first << "-" << axes[i].second;
  }
  stream << "]";
  return stream;
}

template <typename TScalar, size_t TRank>
class NamedAxesTransformation : public Rotation<TScalar, TRank>
{
public:
  NamedAxesTransformation(const NamedAxes<TRank>& axes1, const NamedAxes<TRank>& axes2)
    : Rotation<TScalar, TRank>()
    , m_axes1(axes1)
    , m_axes2(axes2)
  {
    this->get_rotation().fill(0);
    for (size_t i1 = 0; i1 < TRank; i1++)
    {
      const auto& axis1 = axes1[i1];
      for (size_t i2 = 0; i2 < TRank; i2++)
      {
        const auto& axis2 = axes2[i2];
        if (axis1.first == axis2.first)
        {
          if (axis1.second != axis2.second)
          {
            throw std::invalid_argument("Named axes do not correspond");
          }
          this->get_rotation()(i2, i1) = 1;
        }
        else if (axis1.first == axis2.second)
        {
          if (axis1.second != axis2.first)
          {
            throw std::invalid_argument("Named axes do not correspond");
          }
          this->get_rotation()(i2, i1) = -1;
        }
      }
    }
    for (size_t i = 0; i < TRank; i++)
    {
      if (xt::all(xt::equal(xt::row(this->get_rotation(), i), 0)) || xt::all(xt::equal(xt::col(this->get_rotation(), i), 0)))
      {
        throw std::invalid_argument("Named axes do not correspond");
      }
    }
  }

private:
  NamedAxes<TRank> m_axes1;
  NamedAxes<TRank> m_axes2;
};

} // end of ns tiledwebmaps
