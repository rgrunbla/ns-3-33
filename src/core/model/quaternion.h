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
#ifndef NS3_QUATERNION_H
#define NS3_QUATERNION_H

#include "attribute.h"
#include "attribute-helper.h"
#include "ns3/vector.h"


/**
 * \file
 * \ingroup geometry
 * ns3::Quaternion declaration.
 */

namespace ns3 {

/**
 * \ingroup core
 * \defgroup geometry Geometry primitives
 * \brief Primitives for geometry, such as quaternions and angles.
 */

/**
 * \ingroup geometry
 * \brief a quaternion
 * \see attribute_Quaternion
 */
class Quaternion
{
public:
  /**
   * \param [in] _x X coordinate of quaternion
   * \param [in] _y Y coordinate of quaternion
   * \param [in] _z Z coordinate of quaternion
   * \param [in] _w W coordinate of quaternion
   *
   * Create quaternion (_x, _y, _z, _w)
   */
  Quaternion (double _x, double _y, double _z, double _w);
  /** Create quaternion (0.0, 0.0, 0.0, 1.0) */
  Quaternion ();
    /**
   * \param [in] angle Angle in radian
   * \param [in] v Normalized axis
   *
   * Create quaternion from an angle and a normalized axis
   */
   Quaternion (const double &angle, const Vector &v);


  double x;  //!< x coordinate of quaternion
  double y;  //!< y coordinate of quaternion
  double z;  //!< z coordinate of quaternion
  double w;  //!< w coordinate of quaternion

  /**
   * Compute the length (magnitude) of the quaternion.
   * \returns the quaternion length.
   */
  double GetLength () const;

  /**
   * Normalize the quaternion.
   */
  void normalize ();


  /**
   * Conjugate the quaternion.
   */
  void conjugate ();

  /**
   * Inverse the quaternion.
   */
  void inverse ();

  /**
   * Compute the angle of the quaternion.
   * 
   * \returns the angle of the quaternion
   */
  double angle () const;

  /**
   * Compute the Euler angles of the quaternion.
   * Euler angles are given in the roll (x), pitch (y), yaw (z) order.
   * 
   * \returns the Euler angles vector.
   */
  Vector eulerAngles() const;
  Vector rotate(Vector) const;
  
  double pitch() const;
  double yaw() const;
  double roll() const;

  /**
   * Output streamer.
   * Quaternions are written as "x:y:z:w".
   *
   * \param [in,out] os The stream.
   * \param [in] quaternion The quaternion to stream
   * \return The stream.
   */
  friend std::ostream &operator << (std::ostream &os, const Quaternion &quaternion);

  /**
   * Input streamer.
   *
   * Quaternions are expected to be in the form "x:y:z:w".
   *
   * \param [in,out] is The stream.
   * \param [in] quaternion The quaternion.
   * \returns The stream.
   */
  friend std::istream &operator >> (std::istream &is, Quaternion &quaternion);

  /**
   * Equality operator.
   * \param [in] a lhs quaternion.
   * \param [in] b rhs quaternion.
   * \returns \c true if \pname{a} is equal to \pname{b}.
   */
  friend bool operator == (const Quaternion &a, const Quaternion &b);

  /**
   * Inequality operator.
   * \param [in] a lhs quaternion.
   * \param [in] b rhs quaternion.
   * \returns \c true if \pname{a} is not equal to \pname{b}.
   */
  friend bool operator != (const Quaternion &a, const Quaternion &b);

  /**
   * Multiplication operator.
   * \param [in] a lhs quaternion.
   * \param [in] b rhs quaternion.
   * \returns The quaternion multiplication of \pname{a} and \pname{b}.
   */
  friend Quaternion operator * (const Quaternion &a, const Quaternion &b);
  
  /**
   * Difference operator.
   * \param [in] a lhs quaternion.
   * \param [in] b rhs quaternion.
   * \returns The quaternion difference of \pname{a} and \pname{b}.
   */
  friend Quaternion operator - (const Quaternion &a, const Quaternion &b);
    /**
   * Sum operator.
   * \param [in] a lhs quaternion.
   * \param [in] b rhs quaternion.
   * \returns The quaternion sum of \pname{a} and \pname{b}.
   */
  friend Quaternion operator + (const Quaternion &a, const Quaternion &b);
};

std::ostream &operator << (std::ostream &os, const Quaternion &quaternion);
std::istream &operator >> (std::istream &is, Quaternion &quaternion);
bool operator < (const Quaternion &a, const Quaternion &b);

ATTRIBUTE_HELPER_HEADER (Quaternion);

// Document these by hand so they go in group attribute_Quaternion
/**
 * \relates Quaternion
 * \fn ns3::Ptr<const ns3::AttributeAccessor> ns3::MakeQuaternionAccessor (T1 a1)
 * \copydoc ns3::MakeAccessorHelper(T1)
 * \see AttributeAccessor
 */

/**
 * \relates Quaternion
 * \fn ns3::Ptr<const ns3::AttributeAccessor> ns3::MakeQuaternionAccessor (T1 a1, T2 a2)
 * \copydoc ns3::MakeAccessorHelper(T1,T2)
 * \see AttributeAccessor
 */


/**
 * \relates Quaternion
 * \returns The AttributeChecker.
 * \see AttributeChecker
 */
Ptr<const AttributeChecker> MakeQuaternionChecker (void);

} // namespace ns3

#endif /* NS3_QUATERNION_H */
