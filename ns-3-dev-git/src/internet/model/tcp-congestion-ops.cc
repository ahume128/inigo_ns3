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

static unsigned int dctcp_shift_g = 4; /* g = 1/2^4 */
static unsigned int dctcp_alpha_on_init = 0;
//static unsigned int dctcp_clamp_alpha_on_loss;                                             
static unsigned int suspect_rtt = 15;
static unsigned int markthresh = 174;
static unsigned int slowstart_rtt_observations_needed = 10;
static unsigned int rtt_fairness = 10;

namespace ns3 {

NS_LOG_COMPONENT_DEFINE ("TcpCongestionOps");

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
NS_OBJECT_ENSURE_REGISTERED (TcpInigo);

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

TcpInigo::TcpInigo (const TcpInigo& sock)
  : TcpCongestionOps (sock)
{
 
}

void
TcpInigo::InigoInit (void) {
  NS_LOG_FUNCTION_NOARGS ();                                                                     
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
  this->snd_cwnd_cnt = 0;

  //ignoring section on ECN for now                                                       
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
  NS_LOG_FUNCTION (this << tcb->m_cWnd << tcb->m_ssThresh << segmentsAcked << rtt << expiredRtt);
  //inigo_pkts_acked
  uint32_t rtt_us = rtt.ToInteger(Time::US);

  /* Some calls are for duplicates without timetamps */
  if (rtt_us <= 0)
    return;

  this->rtts_observed++;

  this->rtt_min = std::min(rtt_us, this->rtt_min);
  if (this->rtt_min < suspect_rtt) {
    //eventually this would be turned into a log statement
    //pr_debug_ratelimited("tcp_inigo: rtt_min=%u is suspiciously low, setting to rtt=%u\n",
    //                     ca->rtt_min, (u32) rtt);
    this->rtt_min = rtt_us;
  }

  /* Mimic DCTCP's ECN marking threshhold of approximately 0.17*BDP */
  if (rtt_us > (this->rtt_min + (this->rtt_min * markthresh / INIGO_MAX_MARK)))
    this->rtts_late++;

  NS_LOG_INFO ("In PktsAcked, updated rtts_observed " << this->rtts_observed <<
               " rtt_min " << this->rtt_min << " rtts_late " << this->rtts_late);
}

void
TcpInigo::IncreaseWindow (Ptr<TcpSocketState> tcb, uint32_t segmentsAcked)
{
  NS_LOG_FUNCTION (this << tcb->m_cWnd << tcb->m_ssThresh << segmentsAcked);
  // assuming cwnd limited because this function was called

  NS_LOG_INFO ("inigo increase window initial cwnd and ssthresh " << tcb->m_cWnd << " " << tcb->m_ssThresh);

  if (tcb->GetCwndInSegments() <= tcb->GetSsThreshInSegments()) {
    if (this->rtts_observed >= slowstart_rtt_observations_needed) {
      InigoUpdateRttAlpha();

      if (this->rtt_alpha) {
        InigoEnterCwr(tcb);
        return;
      }
    }

    /* In "safe" area, increase. */
    segmentsAcked = InigoSlowStart(tcb, segmentsAcked);
    if (!segmentsAcked)
      return;
  }
  /* In dangerous area, increase slowly. */
  InigoCongAvoidAi(tcb, tcb->GetCwndInSegments(), segmentsAcked);
  
  NS_LOG_INFO ("In IncreaseWindow, updated cwnd " << tcb->m_cWnd <<
               " ssthresh " << tcb->m_ssThresh);
}


//returns ssthresh in unites of bytes as used by ns3
uint32_t
TcpInigo::GetSsThresh (Ptr<const TcpSocketState> state,
                         uint32_t bytesInFlight)
{
  NS_LOG_FUNCTION (this << state << bytesInFlight);
  uint32_t ret = InigoSsThresh (state) * state->m_segmentSize;
  NS_LOG_INFO ("returning " << ret);
  return ret;
}

Ptr<TcpCongestionOps>
TcpInigo::Fork ()
{
  return CopyObject<TcpInigo> (this);
}

void 
TcpInigo::InigoUpdateRttAlpha() {
  NS_LOG_FUNCTION (this);

  uint32_t alpha = this->rtt_alpha;
  uint32_t marks = this->rtts_late;
  uint32_t total = this->rtts_observed;

  /* alpha = (1 - g) * alpha + g * F */
  if (alpha > (uint32_t) (1 << dctcp_shift_g))
    alpha -= alpha >> dctcp_shift_g;
  else
    alpha = 0; // otherwise, alpha can never reach zero                                                                       

  if (marks) {
    /* If shift_g == 1, a 32bit value would overflow                                                                          
     * after 8 M.                                                                                                             
     */
    marks <<= (10 - dctcp_shift_g);
    marks = marks/std::max(1U, total);

    alpha = std::min(alpha + (uint32_t)marks, DCTCP_MAX_ALPHA);
  }

  this->rtt_alpha = alpha;

  NS_LOG_INFO ("In UpdateRttAlpha, updated alpha " << this->rtt_alpha);
}

void 
TcpInigo::InigoEnterCwr (Ptr<TcpSocketState> tcb) 
{
  NS_LOG_FUNCTION (this << tcb->m_cWnd << tcb->m_ssThresh);

  tcb->m_initialSsThresh = 0;
  if (tcb->m_congState < tcb->CA_CWR) {
    tcb->m_initialCWnd = tcb->m_cWnd;
    tcb->SetSsThreshInSegments( InigoSsThresh(tcb) );
    this->rtts_late = 0;
    this->rtts_observed = 0;
    tcb->m_congState = tcb->CA_LOSS;
  }

  NS_LOG_INFO ("In EnterCwr, updated cwnd " << tcb->m_cWnd <<
               " ssthresh " << tcb->m_ssThresh);
}

void 
TcpInigo::InigoCongAvoidAi (Ptr<TcpSocketState> tcb, uint32_t w, uint32_t segmentsAcked)
{
  NS_LOG_FUNCTION (this << tcb->m_cWnd << tcb->m_ssThresh << w << segmentsAcked);

  uint32_t interval = tcb->GetCwndInSegments();

  if (this->snd_cwnd_cnt >= w) {
    if (tcb->GetCwndInSegments() < CWND_CLAMP) {
      tcb->SetCwndInSegments( tcb->GetCwndInSegments() + 1 );
      if (rtt_fairness)
        tcb->SetCwndInSegments( tcb->GetCwndInSegments() + 1 );
    }

    this->snd_cwnd_cnt = 0;
  }

  if (rtt_fairness)
    interval = std::min(interval, rtt_fairness);

  if (this->snd_cwnd_cnt >= interval) {
    if (this->snd_cwnd_cnt % interval == 0 || this->snd_cwnd_cnt >= w) {
      InigoUpdateRttAlpha();
      
      if (this->rtt_alpha)
        InigoEnterCwr(tcb);
    }
  }
  
  if (this->snd_cwnd_cnt < w) {
    this->snd_cwnd_cnt += segmentsAcked;
  }

  NS_LOG_INFO ("In CongAvoidAI, updated to cwnd " << tcb->m_cWnd <<
               " ssthresh " << tcb->m_ssThresh << " snd_cwnd_cnt " << this->snd_cwnd_cnt);
}

//returns ssthresh in units of segments following inigo units
uint32_t
TcpInigo::InigoSsThresh(Ptr<const TcpSocketState> tcb) 
{  
  NS_LOG_FUNCTION (this << tcb->m_cWnd << tcb->m_ssThresh << tcb->m_segmentSize);
  uint16_t alpha = this->rtt_alpha;
  uint32_t nsubwnd = 1;
  uint32_t cong_adj;

  if (rtt_fairness) {
    nsubwnd = tcb->GetCwndInSegments();
    nsubwnd = nsubwnd/rtt_fairness;
    if (tcb->GetCwndInSegments() % rtt_fairness)
      nsubwnd++;
  }

  cong_adj = ((tcb->GetCwndInSegments() * alpha) >> 11U) / nsubwnd;

  NS_LOG_INFO ("In InigoSsThresh returning " 
               << std::max((uint32_t) (tcb->GetCwndInSegments() - cong_adj), 2U));
  return std::max((uint32_t) (tcb->GetCwndInSegments() - cong_adj), 2U);
}

uint32_t
TcpInigo::InigoSlowStart (Ptr<TcpSocketState> tcb, uint32_t segmentsAcked)
{
  NS_LOG_FUNCTION(this << tcb->m_cWnd << tcb->m_ssThresh << segmentsAcked);
  uint32_t cwnd = tcb->GetCwndInSegments() + segmentsAcked;

  if (cwnd > tcb->GetSsThreshInSegments())
    cwnd = tcb->GetSsThreshInSegments() + 1;
  segmentsAcked -= cwnd - tcb->GetCwndInSegments();
  tcb->SetCwndInSegments( std::min(cwnd, CWND_CLAMP) );

  NS_LOG_INFO ("In SlowStart, updated cwnd " << tcb->m_cWnd <<
               " ssthresh " << tcb->m_ssThresh << " segmentsAcked " << segmentsAcked);
  return segmentsAcked;
}

} // namespace ns3
