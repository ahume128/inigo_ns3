/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
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
#include "tcp-congestion-ops.h"
#include "tcp-socket-base.h"
#include "ns3/log.h"

namespace ns3 {

NS_LOG_COMPONENT_DEFINE ("TcpNewReno");

NS_OBJECT_ENSURE_REGISTERED (TcpCongestionOps);

TypeId
TcpCongestionOps::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::TcpCongestionOps")
    .SetParent<Object> ()
    .SetGroupName ("Internet")
  ;
  return tid;
}

TcpCongestionOps::TcpCongestionOps () : Object ()
{
}

TcpCongestionOps::TcpCongestionOps (const TcpCongestionOps &other) : Object (other)
{
}

TcpCongestionOps::~TcpCongestionOps ()
{
}


// RENO

NS_OBJECT_ENSURE_REGISTERED (TcpNewReno);

TypeId
TcpNewReno::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::TcpNewReno")
    .SetParent<TcpCongestionOps> ()
    .SetGroupName ("Internet")
    .AddConstructor<TcpNewReno> ()
  ;
  return tid;
}

TcpNewReno::TcpNewReno (void) : TcpCongestionOps ()
{
  NS_LOG_FUNCTION (this);
}

TcpNewReno::TcpNewReno (const TcpNewReno& sock)
  : TcpCongestionOps (sock)
{
  NS_LOG_FUNCTION (this);
}

TcpNewReno::~TcpNewReno (void)
{
}

/**
 * \brief Tcp NewReno slow start algorithm
 *
 * Defined in RFC 5681 as
 *
 * > During slow start, a TCP increments cwnd by at most SMSS bytes for
 * > each ACK received that cumulatively acknowledges new data.  Slow
 * > start ends when cwnd exceeds ssthresh (or, optionally, when it
 * > reaches it, as noted above) or when congestion is observed.  While
 * > traditionally TCP implementations have increased cwnd by precisely
 * > SMSS bytes upon receipt of an ACK covering new data, we RECOMMEND
 * > that TCP implementations increase cwnd, per:
 * >
 * >    cwnd += min (N, SMSS)                      (2)
 * >
 * > where N is the number of previously unacknowledged bytes acknowledged
 * > in the incoming ACK.
 *
 * The ns-3 implementation respect the RFC definition. Linux does something
 * different:
 * \verbatim
u32 tcp_slow_start(struct tcp_sock *tp, u32 acked)
  {
    u32 cwnd = tp->snd_cwnd + acked;

    if (cwnd > tp->snd_ssthresh)
      cwnd = tp->snd_ssthresh + 1;
    acked -= cwnd - tp->snd_cwnd;
    tp->snd_cwnd = min(cwnd, tp->snd_cwnd_clamp);

    return acked;
  }
  \endverbatim
 *
 * As stated, we want to avoid the case when a cumulative ACK increases cWnd more
 * than a segment size, but we keep count of how many segments we have ignored,
 * and return them.
 *
 * \param tcb internal congestion state
 * \param segmentsAcked count of segments acked
 * \return the number of segments not considered for increasing the cWnd
 */
uint32_t
TcpNewReno::SlowStart (Ptr<TcpSocketState> tcb, uint32_t segmentsAcked)
{
  NS_LOG_FUNCTION (this << tcb << segmentsAcked);

  if (segmentsAcked >= 1)
    {
      tcb->m_cWnd += tcb->m_segmentSize;
      NS_LOG_INFO ("In SlowStart, updated to cwnd " << tcb->m_cWnd << " ssthresh " << tcb->m_ssThresh);
      return segmentsAcked - 1;
    }

  return 0;
}

/**
 * \brief NewReno congestion avoidance
 *
 * During congestion avoidance, cwnd is incremented by roughly 1 full-sized
 * segment per round-trip time (RTT).
 *
 * \param tcb internal congestion state
 * \param segmentsAcked count of segments acked
 */
void
TcpNewReno::CongestionAvoidance (Ptr<TcpSocketState> tcb, uint32_t segmentsAcked)
{
  NS_LOG_FUNCTION (this << tcb << segmentsAcked);

  if (segmentsAcked > 0)
    {
      double adder = static_cast<double> (tcb->m_segmentSize * tcb->m_segmentSize) / tcb->m_cWnd.Get ();
      adder = std::max (1.0, adder);
      tcb->m_cWnd += static_cast<uint32_t> (adder);
      NS_LOG_INFO ("In CongAvoid, updated to cwnd " << tcb->m_cWnd <<
                   " ssthresh " << tcb->m_ssThresh);
    }
}

/**
 * \brief Try to increase the cWnd following the NewReno specification
 *
 * \see SlowStart
 * \see CongestionAvoidance
 *
 * \param tcb internal congestion state
 * \param segmentsAcked count of segments acked
 */
void
TcpNewReno::IncreaseWindow (Ptr<TcpSocketState> tcb, uint32_t segmentsAcked)
{
  NS_LOG_FUNCTION (this << tcb << segmentsAcked);

  if (tcb->m_cWnd < tcb->m_ssThresh)
    {
      segmentsAcked = SlowStart (tcb, segmentsAcked);
    }

  if (tcb->m_cWnd >= tcb->m_ssThresh)
    {
      CongestionAvoidance (tcb, segmentsAcked);
    }

  /* At this point, we could have segmentsAcked != 0. This because RFC says
   * that in slow start, we should increase cWnd by min (N, SMSS); if in
   * slow start we receive a cumulative ACK, it counts only for 1 SMSS of
   * increase, wasting the others.
   *
   * // Uncorrect assert, I am sorry
   * NS_ASSERT (segmentsAcked == 0);
   */
}

std::string
TcpNewReno::GetName () const
{
  return "TcpNewReno";
}

uint32_t
TcpNewReno::GetSsThresh (Ptr<const TcpSocketState> state,
                         uint32_t bytesInFlight)
{
  NS_LOG_FUNCTION (this << state << bytesInFlight);

  return std::max (2 * state->m_segmentSize, bytesInFlight / 2);
}

Ptr<TcpCongestionOps>
TcpNewReno::Fork ()
{
  return CopyObject<TcpNewReno> (this);
}

// Inigo                                                                                                                                                                                                     
//NS_OBJECT_ENSURE_REGISTERED (TcpInigo);

TypeId
TcpInigo::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::TcpInigo")
    .SetParent<TcpCongestionOps> ()
    .SetGroupName ("Internet")
    .AddConstructor<TcpInigo> ()
    ;
  return tid;
}

TcpInigo::TcpInigo (void) : TcpCongestionOps ()
{
  this->InigoInit();
}

TcpInigo::TcpInigo (const TcpNewReno& sock)
  : TcpCongestionOps (sock)
{
  this->InigoInit();
}

void
TcpInigo::InigoInit (void) {
  if (rtt_fairness != 0) {
    if (rtt_fairness < INIGO_MIN_FAIRNESS) {
      rtt_fairness = INIGO_MIN_FAIRNESS;
    }
    else if (rtt_fairness > INIGO_MAX_FAIRNESS){
      rtt_fairness = INIGO_MAX_FAIRNESS;
    }
  }

  this->rtt_min = USEC_PER_SEC;
  this->rtt_alpha = std::min(dctcp_alpha_on_init, DCTCP_MAX_ALPHA);
  this->rtts_late = 0;
  this->rtts_observed = 0;

  //ignoring section on ECN for now                                                                                            
  //NS_LOG_FUNCTION (this);                                                                                                      
  this->dctcp_alpha = 0;
}

TcpInigo::~TcpInigo (void)
{
}

std::string
TcpInigo::GetName () const
{
  return "TcpInigo";
}


void 
TcpInigo::PktsAcked (Ptr<TcpSocketState> tcb, uint32_t segmentsAcked,
                     const Time& rtt, bool expiredRtt) 
{  
  //actually don't think I need to do anything here
  //u32 rtt_min = ;
  //u32 rtts_late = ;
  //u32 rtts_observed = ;
 
  //inigo_pkts_acked
  uint32_t rtt_ms = rtt.ToInteger(Time::US);

  /* Some calls are for duplicates without timetamps */
  if (rtt_ms <= 0)
    return;

  this->rtts_observed++;

  this->rtt_min = std::min(rtt_ms, this->rtt_min); //ASSUMED US FOR NOW
  if (this->rtt_min < suspect_rtt) {
    //eventually this would be turned into a log statement
    //pr_debug_ratelimited("tcp_inigo: rtt_min=%u is suspiciously low, setting to rtt=%u\n",
    //                     ca->rtt_min, (u32) rtt);
    this->rtt_min = rtt_ms;
  }

  /* Mimic DCTCP's ECN marking threshhold of approximately 0.17*BDP */
  if (rtt_ms > (this->rtt_min + (this->rtt_min * markthresh / INIGO_MAX_MARK)))
    this->rtts_late++;

  //inigo_update_dctcp_alpha
  uint32_t acked_bytes = segmentsAcked * tcb->m_segmentSize;

  /* If ack did not advance snd_una, count dupack as MSS size.                                                                
   * If ack did update window, do not count it at all.                                                                        
   */
  if (acked_bytes == 0 && (tcb->m_congState != 3) && (tcb->m_congState != 4) )
    acked_bytes = tcb->m_segmentSize;
  if (acked_bytes) {
    this->acked_bytes_total += acked_bytes;

    //removed prior_snd_una update and two ECN lines
  }

  /* Expired RTT */
  if (expiredRtt) {
    /* For avoiding denominator == 1. */
    if (this->acked_bytes_total == 0)
      this->acked_bytes_total = 1;

    /* alpha = (1 - g) * alpha + g * F */
    this->dctcp_alpha = this->dctcp_alpha -
      (this->dctcp_alpha >> dctcp_shift_g) +
      (this->acked_bytes_ecn << (10U - dctcp_shift_g)) /
      this->acked_bytes_total;

    if (this->dctcp_alpha > DCTCP_MAX_ALPHA)
      /* Clamp dctcp_alpha to max. */
      this->dctcp_alpha = DCTCP_MAX_ALPHA;

    InigoDctcpReset();
  }

}

//FOR NOW THIS DOES NOTHING
void
TcpInigo::IncreaseWindow (Ptr<TcpSocketState> tcb, uint32_t segmentsAcked)
{
}

//FOR NOW THIS IS SAME AS NEWRENO 
uint32_t
TcpInigo::GetSsThresh (Ptr<const TcpSocketState> state,
                         uint32_t bytesInFlight)
{
  //NS_LOG_FUNCTION (this << state << bytesInFlight);

  return std::max (2 * state->m_segmentSize, bytesInFlight / 2);
}

Ptr<TcpCongestionOps>
TcpInigo::Fork ()
{
  return CopyObject<TcpInigo> (this);
}

void
TcpInigo::InigoDctcpReset ()
{
}

} // namespace ns3

