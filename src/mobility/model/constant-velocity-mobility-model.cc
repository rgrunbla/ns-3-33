/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2006, 2007 INRIA
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
#include "constant-velocity-mobility-model.h"
#include "ns3/simulator.h"
#include "ns3/log.h"

namespace ns3 {

NS_OBJECT_ENSURE_REGISTERED (ConstantVelocityMobilityModel);

TypeId ConstantVelocityMobilityModel::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::ConstantVelocityMobilityModel")
    .SetParent<MobilityModel> ()
    .SetGroupName ("Mobility")
    .AddConstructor<ConstantVelocityMobilityModel> ();
  return tid;
}

ConstantVelocityMobilityModel::ConstantVelocityMobilityModel ()
{
}

ConstantVelocityMobilityModel::~ConstantVelocityMobilityModel ()
{
}

void
ConstantVelocityMobilityModel::SetVelocity (const Vector &speed)
{
  m_helper.Update ();
  m_helper.SetVelocity (speed);
  m_helper.Unpause ();
  NotifyCourseChange ();
}

void
ConstantVelocityMobilityModel::SetAngularVelocity (const Vector &angularVel)
{
  m_helper.Update ();
  m_helper.SetAngularVelocity (angularVel);
  m_helper.Unpause ();
  NotifyCourseChange ();
}


Vector
ConstantVelocityMobilityModel::DoGetPosition (void) const
{
  m_helper.Update ();
  return m_helper.GetCurrentPosition ();
}
void 
ConstantVelocityMobilityModel::DoSetPosition (const Vector &position)
{
  m_helper.SetPosition (position);
  NotifyCourseChange ();
}
Vector
ConstantVelocityMobilityModel::DoGetVelocity (void) const
{
  return m_helper.GetVelocity ();
}

Vector
ConstantVelocityMobilityModel::DoGetAngularVelocity (void) const
{
  return m_helper.GetAngularVelocity ();
}

Quaternion
ConstantVelocityMobilityModel::DoGetOrientation (void) const
{
  m_helper.Update ();
  return m_helper.GetCurrentOrientation ();
}
void
ConstantVelocityMobilityModel::DoSetOrientation (const Quaternion &orientation)
{
  m_helper.SetOrientation (orientation);
  NotifyCourseChange ();
}

} // namespace ns3
