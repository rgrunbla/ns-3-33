/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2011 CTTC
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
 * Author: Nicola Baldo <nbaldo@cttc.es>
 */

#include <ns3/log.h>
#include <cmath>
#include <ns3/node.h>
#include "custom-antenna-model.h"

namespace ns3 {

NS_LOG_COMPONENT_DEFINE ("CustomAntennaModel");

NS_OBJECT_ENSURE_REGISTERED (CustomAntennaModel);

CustomAntennaModel::CustomAntennaModel ()
{
}

CustomAntennaModel::~CustomAntennaModel ()
{
}

TypeId
CustomAntennaModel::GetTypeId ()
{
  static TypeId tid =
      TypeId ("ns3::CustomAntennaModel").SetParent<Object> ().SetGroupName ("Antenna");
  return tid;
}

void
CustomAntennaModel::Install (Ptr<Node> node)
{
  Ptr<Node> object = node;
  Ptr<CustomAntennaModel> antenna_model = this->GetObject<CustomAntennaModel>();
  node->AggregateObject (antenna_model);
}

void
CustomAntennaModel::Install (NodeContainer nodes)
{
  Ptr<CustomAntennaModel> antenna_model = this->GetObject<CustomAntennaModel>();
  for (NodeContainer::Iterator i = nodes.Begin (); i != nodes.End (); ++i)
    {
      Ptr<Node> node = *i;
      node->AggregateObject (antenna_model);
    }
}

void
CustomAntennaModel::SetModel (std::function<double (double, double)> antenna_model)
{
  m_antenna_model = antenna_model;
}

double
CustomAntennaModel::GetGainDb (double theta, double phi)
{
  return m_antenna_model (theta, phi);
}

} // namespace ns3
