/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2013 ResiliNets, ITTC, University of Kansas
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
 * Authors: Justin P. Rohrer, Truc Anh N. Nguyen <annguyen@ittc.ku.edu>, Siddharth Gangadhar <siddharth@ittc.ku.edu>
 *
 * James P.G. Sterbenz <jpgs@ittc.ku.edu>, director
 * ResiliNets Research Group  http://wiki.ittc.ku.edu/resilinets
 * Information and Telecommunication Technology Center (ITTC)
 * and Department of Electrical Engineering and Computer Science
 * The University of Kansas Lawrence, KS USA.
 *
 * Work supported in part by NSF FIND (Future Internet Design) Program
 * under grant CNS-0626918 (Postmodern Internet Architecture),
 * NSF grant CNS-1050226 (Multilayer Network Resilience Analysis and Experimentation on GENI),
 * US Department of Defense (DoD), and ITTC at The University of Kansas.
 *
 * “TCP Westwood(+) Protocol Implementation in ns-3”
 * Siddharth Gangadhar, Trúc Anh Ngọc Nguyễn , Greeshma Umapathi, and James P.G. Sterbenz,
 * ICST SIMUTools Workshop on ns-3 (WNS3), Cannes, France, March 2013
 */

#include <iostream>
#include <fstream>
#include <string>

//#define USE_DCE

#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/error-model.h"
#include "ns3/tcp-header.h"
#include "ns3/udp-header.h"
#include "ns3/enum.h"
#include "ns3/event-id.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/timer.h"
#include "ns3/config-store-module.h"

#ifdef USE_DCE
  #include "ns3/dce-module.h"
#endif

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("Variants");

bool firstCwnd = true;
bool firstSshThr = true;
bool firstData1 = true;
bool firstData2 = true;
bool firstRto = true;
bool firstEnqueue = true;

Ptr<OutputStreamWrapper> cWndStream;
Ptr<OutputStreamWrapper> ssThreshStream;
Ptr<OutputStreamWrapper> dataRxStream;
Ptr<OutputStreamWrapper> rtoStream;

uint32_t cWndValue;
uint32_t ssThreshValue;


static void
CwndTracer (uint32_t oldval, uint32_t newval)
{
  if (firstCwnd)
    {
      *cWndStream->GetStream () << "0.0 " << oldval << std::endl;
      firstCwnd = false;
    }
  *cWndStream->GetStream () << Simulator::Now ().GetSeconds () << " " << newval << std::endl;
  cWndValue = newval;

  if (!firstSshThr)
    {
      *ssThreshStream->GetStream () << Simulator::Now ().GetSeconds () << " " << ssThreshValue << std::endl;
    }
}

static void
SsThreshTracer (uint32_t oldval, uint32_t newval)
{
  if (firstSshThr)
    {
      *ssThreshStream->GetStream () << "0.0 " << oldval << std::endl;
      firstSshThr = false;
    }

  *ssThreshStream->GetStream () << Simulator::Now ().GetSeconds () << " " << newval << std::endl;
  ssThreshValue = newval;

  if (!firstCwnd)
    {
      *cWndStream->GetStream () << Simulator::Now ().GetSeconds () << " " << cWndValue << std::endl;
    }
}

static void
RtoTracer (Time oldval, Time newval)
{
  if (firstRto)
    {
      *rtoStream->GetStream () << "0.0 " << oldval.GetMilliSeconds() << std::endl;
      firstRto = false;
    }

  *rtoStream->GetStream () << Simulator::Now ().GetSeconds () << " " << newval.GetMilliSeconds() << std::endl;
}

static void
QueueTracer (uint32_t oldval, uint32_t newval)
{
  *queueStream->GetStream () << Simulator::Now ().GetSeconds () << " " << newva\
    l << std::endl;
}

static void
dataRxCallback (Ptr<const Packet> p, const Address &addr)
{
  (void) addr;
  static uint32_t bytes1 = 0;
  static uint32_t bytes2 = 0;
  uint32_t toPrint = 0;

  InetSocketAddress a = InetSocketAddress::ConvertFrom(addr);

  std::stringstream ss ;
  a.GetIpv4().Print(ss);

  if (firstData1)
    {
      *dataRxStream->GetStream () << "0.0 0 "  << ss.str() << std::endl;
      firstData1 = false;
    }
  if (ss.str() == "10.0.2.1")
    {
      bytes2 += p->GetSize();
      toPrint = bytes2;
    }
  else if (ss.str() == "10.0.1.1")
    {
      bytes1 += p->GetSize();
      toPrint = bytes1;
    }

  *dataRxStream->GetStream () << Simulator::Now ().GetSeconds () << " " << toPrint << " " << ss.str() << std::endl;
}

static void
TraceCwnd (std::string cwnd_tr_file_name, Ptr<BulkSendApplication> sendApp)
{
  AsciiTraceHelper ascii;
  cWndStream = ascii.CreateFileStream (cwnd_tr_file_name.c_str ());
  Ptr<Socket> socket = sendApp->GetSocket();
  NS_ASSERT (socket != 0);
  socket->TraceConnectWithoutContext ("CongestionWindow", MakeCallback (&CwndTracer));
}

static void
TraceSsThresh (std::string ssthresh_tr_file_name, Ptr<BulkSendApplication> sendApp)
{
  AsciiTraceHelper ascii;
  ssThreshStream = ascii.CreateFileStream (ssthresh_tr_file_name.c_str ());
  Ptr<Socket> socket = sendApp->GetSocket();
  NS_ASSERT (socket != 0);
  socket->TraceConnectWithoutContext ("SlowStartThreshold", MakeCallback (&SsThreshTracer));
}

static void
TraceRto (std::string rto_tr_file_name, Ptr<BulkSendApplication> sendApp)
{
  AsciiTraceHelper ascii;
  rtoStream = ascii.CreateFileStream (rto_tr_file_name.c_str ());
  Ptr<Socket> socket = sendApp->GetSocket();
  NS_ASSERT (socket != 0);
  socket->TraceConnectWithoutContext ("RTO", MakeCallback (&RtoTracer));
}

static void
TraceQueue (std::string q_tr_file_name)
{
  AsciiTraceHelper ascii;
  queueStream = ascii.CreateFileStream (q_tr_file_name.c_str ());
  Config::ConnectWithoutContext ("/NodeList/0/DeviceList/2/$ns3::PointToPointNe\
tDevice/TxQueue/nPackets", MakeCallback (&QueueTracer));
}

int main (int argc, char *argv[])
{
  std::string transport_prot = "TcpWestwood";
  double error_p = 25.0;
  std::string bandwidth = "10Mb/s";
  std::string delay = "45ms";
  bool tracing = true;
  std::string prefix_file_name = "TcpVariantsComparison";
  //  std::string tr_file_name = "test";
  std::string cwnd_tr_file_name = "";
  std::string ssthresh_tr_file_name = "";
  std::string rx_tr_file_name = "";
  std::string rto_tr_file_name = "";
  double data_mbytes = 0;
  uint32_t mtu_bytes = 1506;
  uint16_t num_flows = 1;
  float duration = 360;
  uint32_t run = 0;
  uint32_t queue_size = 3075; //based on formula in shell script

  CommandLine cmd;
  cmd.AddValue ("transport_prot", "Transport protocol to use: TcpTahoe, TcpReno, TcpNewReno, TcpWestwood, TcpWestwoodPlus ", transport_prot);
  cmd.AddValue ("error_p", "Packet error rate", error_p);
  cmd.AddValue ("bandwidth", "Bottleneck bandwidth", bandwidth);
  cmd.AddValue ("delay", "Access link delay", delay);
  cmd.AddValue ("tracing", "Flag to enable/disable tracing", tracing);
  cmd.AddValue ("prefix_name", "Prefix of output trace file", prefix_file_name);
  //cmd.AddValue ("tr_name", "Name of output trace file", tr_file_name);
  cmd.AddValue ("cwnd_tr_name", "Name of output trace file", cwnd_tr_file_name);
  cmd.AddValue ("ssthresh_tr_name", "Name of output trace file", ssthresh_tr_file_name);
  cmd.AddValue ("rto_tr_name", "Name of output trace file", rto_tr_file_name);
  cmd.AddValue ("rxdata_tr_name", "Name of the output rx data trace file", rx_tr_file_name);
  cmd.AddValue ("data", "Number of Megabytes of data to transmit", data_mbytes);
  cmd.AddValue ("mtu", "Size of IP packets to send in bytes", mtu_bytes);
  cmd.AddValue ("num_flows", "Number of flows", num_flows);
  cmd.AddValue ("duration", "Time to allow flows to run in seconds", duration);
  cmd.AddValue ("run", "Run index (for setting repeatable seeds)", run);
  cmd.AddValue ("queue_size", "Size (in bytes) for the queue", queue_size);
  cmd.Parse (argc, argv);

  SeedManager::SetSeed (2);
  SeedManager::SetRun (run);

  // Calculate the ADU size
  Header* temp_header = new Ipv4Header ();
  uint32_t ip_header = temp_header->GetSerializedSize ();
  NS_LOG_UNCOND ("IP Header size is: " << ip_header);
  delete temp_header;
  temp_header = new TcpHeader ();
  uint32_t tcp_header = temp_header->GetSerializedSize () + 20;
  NS_LOG_UNCOND ("TCP Header size is: " << tcp_header);
  delete temp_header;
  uint32_t tcp_adu_size = mtu_bytes - (ip_header + tcp_header);
  NS_LOG_UNCOND ("TCP ADU size is: " << tcp_adu_size);

  // Set the simulation start and stop time
  float start_time = 1.0;
  float stop_time = start_time + duration;

  NodeContainer router;
  router.Create(1);
  NodeContainer sources;
  sources.Create (num_flows);
  NodeContainer sinks;
  sinks.Create (1);

  InternetStackHelper internetStack;

  GlobalValue::Bind ("ChecksumEnabled", BooleanValue (true));

  // Select TCP variant
  if (transport_prot.compare ("TcpTahoe") == 0)
    {
      Config::SetDefault ("ns3::TcpL4Protocol::SocketType", TypeIdValue (TcpTahoe::GetTypeId ()));
    }
  else if (transport_prot.compare ("TcpReno") == 0)
    {
      Config::SetDefault ("ns3::TcpL4Protocol::SocketType", TypeIdValue (TcpReno::GetTypeId ()));
    }
  else if (transport_prot.compare ("TcpNewReno") == 0)
    {
      Config::SetDefault ("ns3::TcpL4Protocol::SocketType", TypeIdValue (TcpNewReno::GetTypeId ()));
    }
  else if (transport_prot.compare ("TcpWestwood") == 0)
    {
      // the default protocol type in ns3::TcpWestwood is WESTWOOD
      Config::SetDefault ("ns3::TcpL4Protocol::SocketType", TypeIdValue (TcpWestwood::GetTypeId ()));
      Config::SetDefault ("ns3::TcpWestwood::FilterType", EnumValue (TcpWestwood::TUSTIN));
    }
  else if (transport_prot.compare ("TcpWestwoodPlus") == 0)
    {
      Config::SetDefault ("ns3::TcpL4Protocol::SocketType", TypeIdValue (TcpWestwood::GetTypeId ()));
      Config::SetDefault ("ns3::TcpWestwood::ProtocolType", EnumValue (TcpWestwood::WESTWOODPLUS));
      Config::SetDefault ("ns3::TcpWestwood::FilterType", EnumValue (TcpWestwood::TUSTIN));
    }
  else if (transport_prot.compare ("TcpCubic") == 0)
    {
      Config::SetDefault ("ns3::TcpL4Protocol::SocketType", TypeIdValue (TcpCubic::GetTypeId ()));
    }
  else if (transport_prot.compare ("TcpHybla") == 0)
    {
      Config::SetDefault ("ns3::TcpL4Protocol::SocketType", TypeIdValue (TcpHybla::GetTypeId ()));
    }
  else if (transport_prot.compare ("TcpHighSpeed") == 0)
    {
      Config::SetDefault ("ns3::TcpL4Protocol::SocketType", TypeIdValue (TcpHighSpeed::GetTypeId ()));
    }
  else if (transport_prot.compare ("TcpBic") == 0)
    {
      Config::SetDefault ("ns3::TcpL4Protocol::SocketType", TypeIdValue (TcpBic::GetTypeId ()));
    }
  else if (transport_prot.compare ("TcpNoordwijk") == 0)
    {
      Config::SetDefault ("ns3::TcpL4Protocol::SocketType", TypeIdValue (TcpNoordwijk::GetTypeId ()));
      Config::SetDefault ("ns3::TcpSocket::SegmentSize", UintegerValue(1000));
      Config::SetDefault ("ns3::TcpSocket::InitialCwnd", UintegerValue(156));
      Config::SetDefault ("ns3::TcpNoordwijk::TxTime", TimeValue (MilliSeconds (125)));
      Config::SetDefault ("ns3::TcpNoordwijk::B", TimeValue (MilliSeconds (350)));
    }
  else
    {
      NS_FATAL_ERROR ("Invalid TCP version");
    }


  //Config::SetDefault ("ns3::TcpSocket::InitialSlowStartThreshold", UintegerValue(10*mtu_bytes));
  Config::SetDefault ("ns3::TcpSocket::InitialSlowStartThreshold", UintegerValue(8621440*2));
  Config::SetDefault ("ns3::TcpSocket::InitialCwnd",               UintegerValue(10));
  Config::SetDefault ("ns3::TcpSocket::SegmentSize", UintegerValue (tcp_adu_size));
  Config::SetDefault ("ns3::TcpSocket::DelAckCount", UintegerValue (1));
  Config::SetDefault ("ns3::TcpSocket::SndBufSize", UintegerValue (8621440)); // *Bytes*
  Config::SetDefault ("ns3::TcpSocket::RcvBufSize", UintegerValue (8621440)); // *Bytes*
  Config::SetDefault ("ns3::DropTailQueue::MaxPackets",    UintegerValue (queue_size));

  internetStack.InstallAll ();

  // Configure the error model
  // Here we use RateErrorModel with packet error rate
  Ptr<UniformRandomVariable> uv = CreateObject<UniformRandomVariable> ();
  uv->SetStream (50);
  RateErrorModel error_model;
  error_model.SetRandomVariable (uv);
  error_model.SetUnit (RateErrorModel::ERROR_UNIT_PACKET);
  error_model.SetRate (error_p);

  PointToPointHelper UnReLink;
  //UnReLink.SetQueue("ns3::CoDelQueue");
  UnReLink.SetDeviceAttribute ("DataRate", StringValue (bandwidth));
  UnReLink.SetChannelAttribute ("Delay", StringValue (delay));
  UnReLink.SetDeviceAttribute ("ReceiveErrorModel", PointerValue (&error_model));
  UnReLink.SetDeviceAttribute ("Mtu", UintegerValue (mtu_bytes));

  PointToPointHelper LocalLink;
  LocalLink.SetDeviceAttribute ("DataRate", StringValue ("100Mbps"));
  LocalLink.SetChannelAttribute ("Delay", StringValue ("1ns"));

  Ipv4AddressHelper address;
  address.SetBase ("10.0.0.0", "255.255.255.0");
  Ipv4InterfaceContainer sink_interfaces;
  {
    NetDeviceContainer devices;
    Ipv4InterfaceContainer interfaces;

    for (int i = 0; i < num_flows; i++)
      {
        devices = LocalLink.Install (sources.Get (i), router.Get (0));
        address.NewNetwork ();
        interfaces = address.Assign (devices);
      }
    devices = UnReLink.Install (router.Get (0), sinks.Get (0));
    address.NewNetwork ();
    interfaces = address.Assign (devices);
    sink_interfaces.Add (interfaces.Get (1));
  }

  NS_LOG_INFO ("Initialize Global Routing.");
  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

  ApplicationContainer sinkApp;
  uint16_t port = 50000;
  Address sinkLocalAddress (InetSocketAddress (Ipv4Address::GetAny (), port));
  PacketSinkHelper sinkHelper ("ns3::TcpSocketFactory", sinkLocalAddress);
  AddressValue remoteAddress (InetSocketAddress (sink_interfaces.GetAddress (0, 0), port));
  Ptr<BulkSendApplication> sendApp;

  for (uint16_t i = 0; i < sources.GetN (); i++)
    {
      BulkSendHelper ftp ("ns3::TcpSocketFactory", Address ());
      ftp.SetAttribute ("Remote", remoteAddress);
      ftp.SetAttribute ("SendSize", UintegerValue (tcp_adu_size));
      ftp.SetAttribute ("MaxBytes", UintegerValue (int(data_mbytes * 1000000)));

      ApplicationContainer sourceApp = ftp.Install (sources.Get (i));
      sourceApp.Start (Seconds (start_time));
      sourceApp.Stop (Seconds (stop_time));

      // This does not work for multiple flow
      sendApp = DynamicCast<BulkSendApplication> (sourceApp.Get(0));
    }

  sinkHelper.SetAttribute ("Protocol", TypeIdValue (TcpSocketFactory::GetTypeId ()));
  sinkApp = sinkHelper.Install (sinks);
  sinkApp.Start (Seconds (start_time));
  sinkApp.Stop (Seconds (stop_time));

  if (tracing)
   {
     Simulator::Schedule (Seconds (0.00001), &TraceCwnd, prefix_file_name + "-cwnd.data");
     Simulator::Schedule (Seconds (0.00001), &TraceSsThresh, prefix_file_name + "-ssth.data");
     Simulator::Schedule (Seconds (0.00001), &TraceRtt, prefix_file_name + "-rtt.data");
     Simulator::Schedule (Seconds (0.00001), &TraceRto, prefix_file_name + "-rto.data");
     Simulator::Schedule (Seconds (0.00001), &TraceQueue, prefix_file_name + "-q.data");
    }

  //UnReLink.EnablePcapAll (tr_file_name, true);

  /*Config::SetDefault ("ns3::ConfigStore::Filename", StringValue ("output-attributes.txt"));
  Config::SetDefault ("ns3::ConfigStore::FileFormat", StringValue ("RawText"));
  Config::SetDefault ("ns3::ConfigStore::Mode", StringValue ("Save"));
  ConfigStore outputConfig2;
  outputConfig2.ConfigureDefaults ();
  outputConfig2.ConfigureAttributes ();*/

  Simulator::Stop (Seconds (stop_time));
  Simulator::Run ();

  Simulator::Destroy ();
  return 0;
}
