/* -*-  Mode: C++; c-file-style: "gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2015 Natale Patriciello <natale.patriciello@gmail.com>
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
 */
#ifndef TCPSLOWSTARTTEST_H
#define TCPSLOWSTARTTEST_H

#include "ns3/test.h"
#include "tcp-general-test.h"

namespace ns3 {

/**
 * \brief Test the normal behavior for slow start
 *
 * As method for checking the slow start, a callback is attached to the
 * congestion window. With the knowledge of the number of segments, we can calculate
 * if the value of the cWnd is right. Also, with a fixed delay for each packet,
 * we can know if the timing is correct.
 *
 * Check what is done inside CWndTrace.
 *
 * \see CWndTrace
 */
class
  TcpSlowStartNormalTest : public TcpGeneralTest
{
public:
  TcpSlowStartNormalTest (uint32_t segmentSize, uint32_t packetSize,
                          uint32_t initSsTh, uint32_t packets, TypeId& congControl,
                          const std::string &desc);

protected:
  virtual void CWndTrace (uint32_t oldValue, uint32_t newValue);

  Time     m_lastChange;
  uint32_t m_leftSegments;
  uint32_t m_ackedSegments;

  bool   m_initial;

};

/**
 * \brief A slow start test using a socket which sends smaller ACKs
 *
 * The same test are performed over a connection where, on one side, there is
 * a malicious socket which sends smaller ACKs than the segment received.
 *
 * Slow start behavior should not change.
 */
class
  TcpSlowStartAttackerTest : public TcpSlowStartNormalTest
{
public:
  TcpSlowStartAttackerTest (uint32_t segmentSize, uint32_t packetSize,
                            uint32_t initSsTh, uint32_t packets, TypeId& congControl,
                            const std::string &desc);

protected:
  virtual Ptr<TcpSocketMsgBase> CreateReceiverSocket (Ptr<Node> node);
};

} //namespace ns3
#endif // TCPSLOWSTARTTEST_H

