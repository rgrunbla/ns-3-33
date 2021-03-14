/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2020 INRIA
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation;
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * Author: Rémy Grünblatt <remy@grunblatt.org>
 */
#include "quaternion.h"
#include "fatal-error.h"
#include "log.h"
#include "test.h"
#include <cmath>
#include <sstream>
#include <tuple>
#include <limits>
#include <algorithm>

/**
 * \file
 * \ingroup attribute_Quaternion
 * ns3::Quaternion attribute value implementations.
 */

namespace ns3 {

NS_LOG_COMPONENT_DEFINE ("Quaternion");

ATTRIBUTE_HELPER_CPP (Quaternion);

Quaternion::Quaternion (double _x, double _y, double _z, double _w)
  : x (_x),
    y (_y),
    z (_z),
    w (_w)
{
  NS_LOG_FUNCTION (this << _x << _y << _z << _w);
}

Quaternion::Quaternion ()
  : x (0.0),
    y (0.0),
    z (0.0),
    w (1.0)
{
  NS_LOG_FUNCTION (this);
}

Quaternion::Quaternion (const double &angle, const Vector &v)
{
  NS_LOG_FUNCTION (this << angle << v);
		const double s = std::sin(angle * 0.5);
    x = v.x * s;
    y = v.y * s;
    z = v.z * s;
    w = std::cos(angle * 0.5);
}

double
Quaternion::GetLength () const
{
  NS_LOG_FUNCTION (this);
  return std::sqrt (x * x + y * y + z * z + w * w);
}

double
Quaternion::angle () const
{
  if (std::abs(w) > std::cos(1/2))
		{
			return 2 * std::asin(sqrt(x * x + y * y + z * z));
		}

		return 2 * std::acos(w);
}

Vector
Quaternion::eulerAngles () const
{
  NS_LOG_FUNCTION (this);
  return Vector(roll(), pitch(), yaw());
}

double
Quaternion::roll () const
{
	return std::atan2(2 * (w * x + y * z), 1 - 2 * ( x * x + y * y));
}

double
Quaternion::pitch () const
{
	  const double sinp = 2 * (w * y - z * x);
    if (std::abs(sinp) >= 1)
        return std::copysign(M_PI / 2, sinp); // use 90 degrees if out of range
    else
        return std::asin(sinp);
}

double
Quaternion::yaw () const
{
    const double siny_cosp = 2 * (w * z + x * y);
    const double cosy_cosp = 1 - 2 * (y * y + z * z);
    return std::atan2(siny_cosp, cosy_cosp);}

void
Quaternion::normalize ()
{
  double length = GetLength();
  x /=  length;
  y /=  length;
  z /=  length;
  w /=  length;
}

void
Quaternion::conjugate ()
{
  x = -x;
  y = -y;
  z = -z;
  w = w;
}

void
Quaternion::inverse ()
{
  double dot = x*x + y*y + z*z + w*w;
  conjugate();
  x /= dot;
  y /= dot;
  z /= dot;
  w /= dot;
}


Vector Quaternion::rotate(Vector v) const
{
  Vector u = Vector(x, y, z);
  return 2.0 * Dot(u, v) * u + (w*w - Dot(u, u)) * v + 2.0 * w * Cross(u, v);
}

std::ostream &operator << (std::ostream &os, const Quaternion &Quaternion)
{
  os << Quaternion.x << ":" << Quaternion.y << ":" << Quaternion.z << ":" << Quaternion.w;
  return os;
}
std::istream &operator >> (std::istream &is, Quaternion &Quaternion)
{
  char c1, c2, c3;
  is >> Quaternion.x >> c1 >> Quaternion.y >> c2 >> Quaternion.z >> c3 >> Quaternion.w;
  if (c1 != ':'
      || c2 != ':'
      || c3 != ':')
    {
      is.setstate (std::ios_base::failbit);
    }
  return is;
}

bool operator == (const Quaternion &a, const Quaternion &b)
{
  return std::tie (a.x, a.y, a.z, a.w) ==
         std::tie (b.x, b.y, b.z, b.w);
}
bool operator != (const Quaternion &a, const Quaternion &b)
{
  return !(a == b);
}

Quaternion
operator * (const Quaternion &a, const Quaternion &b)
{
  return Quaternion (a.w * b.x + a.x * b.w + a.y * b.z - a.z * b.y,
                     a.w * b.y + a.y * b.w + a.z * b.x - a.x * b.z,
                     a.w * b.z + a.z * b.w + a.x * b.y - a.y * b.x,
                     a.w * b.w - a.x * b.x - a.y * b.y - a.z * b.z);
}

Quaternion
operator + (const Quaternion &a, const Quaternion &b)
{
  return Quaternion (a.x + b.x, a.y + b.y, a.z + b.z, a.w + b.w);
}

Quaternion
operator - (const Quaternion &a, const Quaternion &b)
{
  return Quaternion (a.x - b.x, a.y - b.y, a.z - b.z, a.w - b.w);
}

} // namespace ns3
