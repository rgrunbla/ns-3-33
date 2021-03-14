/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2006,2007 INRIA
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
 * Author: Mathieu Lacage <mathieu.lacage@sophia.inria.fr>
 */
#include "constant-velocity-helper.h"
#include "ns3/box.h"
#include "ns3/log.h"
#include "ns3/rectangle.h"
#include "ns3/simulator.h"

namespace ns3 {

NS_LOG_COMPONENT_DEFINE("ConstantVelocityHelper");

ConstantVelocityHelper::ConstantVelocityHelper() : m_paused(true) {
  NS_LOG_FUNCTION(this);
}
ConstantVelocityHelper::ConstantVelocityHelper(const Vector &position)
    : m_position(position), m_paused(true) {
  NS_LOG_FUNCTION(this << position);
}
ConstantVelocityHelper::ConstantVelocityHelper(const Vector &position,
                                               const Vector &vel)
    : m_position(position), m_velocity(vel), m_paused(true) {
  NS_LOG_FUNCTION(this << position << vel);
}
ConstantVelocityHelper::ConstantVelocityHelper(const Vector &position,
                                               const Vector &vel,
                                               const Vector &angularVel)
    : m_position(position), m_velocity(vel), m_angularVelocity(angularVel), m_paused(true) {
  NS_LOG_FUNCTION(this << position << vel << angularVel);
}

void ConstantVelocityHelper::SetPosition(const Vector &position) {
  NS_LOG_FUNCTION(this << position);
  m_position = position;
  m_velocity = Vector(0.0, 0.0, 0.0);
  m_angularVelocity = Vector(0.0, 0.0, 0.0);
  m_lastUpdate = Simulator::Now();
}

Vector ConstantVelocityHelper::GetCurrentPosition(void) const {
  NS_LOG_FUNCTION(this);
  return m_position;
}

Vector ConstantVelocityHelper::GetVelocity(void) const {
  NS_LOG_FUNCTION(this);
  return m_paused ? Vector(0.0, 0.0, 0.0) : m_velocity;
}
void ConstantVelocityHelper::SetVelocity(const Vector &vel) {
  NS_LOG_FUNCTION(this << vel);
  m_velocity = vel;
  m_lastUpdate = Simulator::Now();
}

void ConstantVelocityHelper::SetOrientation(const Quaternion &orientation) {
  NS_LOG_FUNCTION(this << orientation);
  m_orientation = orientation;
  m_angularVelocity = Vector(0.0, 0.0, 0.0);
  m_lastUpdate = Simulator::Now();
}

Quaternion ConstantVelocityHelper::GetCurrentOrientation(void) const {
  NS_LOG_FUNCTION(this);
  return m_orientation;
}

Vector ConstantVelocityHelper::GetAngularVelocity(void) const {
  NS_LOG_FUNCTION(this);
  return m_paused ? Vector(0.0, 0.0, 0.0) : m_angularVelocity;
}
void ConstantVelocityHelper::SetAngularVelocity(const Vector &angularVel) {
  NS_LOG_FUNCTION(this << angularVel);
  m_angularVelocity = angularVel;
  /* double cr = std::cos(angularVel.x * 0.5);
  double sr = std::sin(angularVel.x * 0.5);
  double cp = std::cos(angularVel.y * 0.5);
  double sp = std::sin(angularVel.y * 0.5);
  double cy = std::cos(angularVel.z * 0.5);
  double sy = std::sin(angularVel.z * 0.5);

  m_angularVelocity.x = sr * cp * cy - cr * sp * sy;
  m_angularVelocity.y = cr * sp * cy + sr * cp * sy;
  m_angularVelocity.z = cr * cp * sy - sr * sp * cy;
  m_angularVelocity.w = cr * cp * cy + sr * sp * sy; */

  m_lastUpdate = Simulator::Now();
}

void ConstantVelocityHelper::Update(void) const {
  NS_LOG_FUNCTION(this << m_orientation);
  Time now = Simulator::Now();
  NS_ASSERT(m_lastUpdate <= now);
  Time deltaTime = now - m_lastUpdate;
  m_lastUpdate = now;
  if (m_paused) {
    m_orientation.normalize();
    return;
  }
  double deltaS = deltaTime.GetSeconds();
  m_position.x += m_velocity.x * deltaS;
  m_position.y += m_velocity.y * deltaS;
  m_position.z += m_velocity.z * deltaS;
  // https://math.stackexchange.com/questions/39553/how-do-i-apply-an-angular-velocity-vector3-to-a-unit-quaternion-orientation
  Quaternion deltaRotation = Quaternion(m_angularVelocity.x * deltaS * 0.5, m_angularVelocity.y * deltaS * 0.5, m_angularVelocity.z * deltaS * 0.5, 1.0);
  m_orientation = m_orientation * deltaRotation;
  m_orientation.normalize();
  return;
}

void ConstantVelocityHelper::UpdateWithBounds(const Rectangle &bounds) const {
  NS_LOG_FUNCTION(this << bounds);
  Update();
  m_position.x = std::min(bounds.xMax, m_position.x);
  m_position.x = std::max(bounds.xMin, m_position.x);
  m_position.y = std::min(bounds.yMax, m_position.y);
  m_position.y = std::max(bounds.yMin, m_position.y);
}

void ConstantVelocityHelper::UpdateWithBounds(const Box &bounds) const {
  NS_LOG_FUNCTION(this << bounds);
  Update();
  m_position.x = std::min(bounds.xMax, m_position.x);
  m_position.x = std::max(bounds.xMin, m_position.x);
  m_position.y = std::min(bounds.yMax, m_position.y);
  m_position.y = std::max(bounds.yMin, m_position.y);
  m_position.z = std::min(bounds.zMax, m_position.z);
  m_position.z = std::max(bounds.zMin, m_position.z);
}

void ConstantVelocityHelper::Pause(void) {
  NS_LOG_FUNCTION(this);
  m_paused = true;
}

void ConstantVelocityHelper::Unpause(void) {
  NS_LOG_FUNCTION(this);
  m_paused = false;
}

} // namespace ns3
