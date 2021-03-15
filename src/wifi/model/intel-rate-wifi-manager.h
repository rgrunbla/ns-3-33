/******************************************************************************
 *
 * Copyright(c) 2005 - 2014 Intel Corporation. All rights reserved.
 * Copyright(c) 2013 - 2015 Intel Mobile Communications GmbH
 * Copyright(c) 2016 - 2017 Intel Deutschland GmbH
 * Copyright(c) 2018 - 2019 Intel Corporation
 * Copyright(c) 2019 - Rémy Grünblatt <remy@grunblatt.org>
 *
 * Contact Information:
 * Rémy Grünblatt <remy@grunblatt.org>
 *
 *****************************************************************************/

#ifndef CONSTANT_RATE_WIFI_MANAGER_H
#define CONSTANT_RATE_WIFI_MANAGER_H

#include "wifi-remote-station-manager.h"

namespace ns3 {

/**
 * \ingroup wifi
 * \brief use constant rates for data and RTS transmissions
 *
 * This class uses always the same transmission rate for every
 * packet sent.
 */
class IntelWifiManager : public WifiRemoteStationManager
{
public:
  /**
   * \brief Get the type ID.
   * \return the object TypeId
   */
  static TypeId GetTypeId (void);
  IntelWifiManager ();
  virtual ~IntelWifiManager ();


private:
  //overridden from base class
  void DoInitialize (void);
  WifiRemoteStation* DoCreateStation (void) const;
  void DoReportRxOk (WifiRemoteStation *station,
                     double rxSnr, WifiMode txMode);
  void DoReportRtsFailed (WifiRemoteStation *station);
  void DoReportDataFailed (WifiRemoteStation *station);
  void DoReportRtsOk (WifiRemoteStation *station,
                      double ctsSnr, WifiMode ctsMode, double rtsSnr);
  void DoReportDataOk (WifiRemoteStation *station, double ackSnr, WifiMode ackMode,
                       double dataSnr, uint16_t dataChannelWidth, uint8_t dataNss);
  void DoReportAmpduTxStatus (WifiRemoteStation *station, uint8_t nSuccessfulMpdus, uint8_t nFailedMpdus,
                              double rxSnr, double dataSnr, uint16_t dataChannelWidth, uint8_t dataNss);
  void DoReportFinalRtsFailed (WifiRemoteStation *station);
  void DoReportFinalDataFailed (WifiRemoteStation *station);
  WifiTxVector DoGetDataTxVector (WifiRemoteStation *station);
  WifiTxVector DoGetRtsTxVector (WifiRemoteStation *station);
  bool IsLowLatency (void) const;

  void CheckInit (WifiRemoteStation *station);

  /**
   * Check the validity of a combination of number of streams, chWidth and mode.
   *
   * \param phy pointer to the wifi phy
   * \param streams the number of streams
   * \param chWidth the channel width (MHz)
   * \param mode the wifi mode
   * \returns true if the combination is valid
   */
  bool IsValidMcs (Ptr<WifiPhy> phy, uint8_t streams, uint16_t chWidth, WifiMode mode);

  /**
   * Returns a list of only the VHT MCS supported by the device.
   * \returns the list of the VHT MCS supported
   */
  WifiModeList GetVhtDeviceMcsList (void) const;

  /**
   * Returns a list of only the HT MCS supported by the device.
   * \returns the list of the HT MCS supported
   */
  WifiModeList GetHtDeviceMcsList (void) const;

  int MAX_HT_GROUP_RATES = 8;     //!< Number of rates (or MCS) per HT group.
  int MAX_VHT_GROUP_RATES = 10;   //!< Number of rates (or MCS) per VHT group.

  WifiMode m_dataMode; //!< Wifi mode for unicast DATA frames
  WifiMode m_ctlMode;  //!< Wifi mode for RTS frames
};

} //namespace ns3

#endif /* CONSTANT_RATE_WIFI_MANAGER_H */
