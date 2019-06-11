#include "ns3/command-line.h"
#include "ns3/config.h"
#include "ns3/uinteger.h"
#include "ns3/boolean.h"
#include "ns3/double.h"
#include "ns3/string.h"
#include "ns3/log.h"
#include "ns3/yans-wifi-helper.h"
#include "ns3/ssid.h"
#include "ns3/mobility-helper.h"
#include "ns3/internet-stack-helper.h"
#include "ns3/ipv4-address-helper.h"
#include "ns3/udp-client-server-helper.h"
#include "ns3/packet-sink-helper.h"
#include "ns3/on-off-helper.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/packet-sink.h"
#include "ns3/yans-wifi-channel.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("he-wifi-network");

int main (int argc, char *argv[])
{
  bool udp = true;
  bool useRts = false;
  double simulationTime = 10; //seconds
  double distance = 0; //meters
  double frequency = 5.0; //whether 2.4 or 5.0 GHz
  double step = 5;
  int mcs = -1; // -1 indicates an unset value
  int nStreams = 1; // number of MIMO streams 
  double minExpectedThroughput = 0;
  double maxExpectedThroughput = 0;
  std::string fileName = "default-output";
  std::ofstream csv;

  CommandLine cmd;
  cmd.AddValue ("frequency", "Whether working in the 2.4 or 5.0 GHz band (other values gets rejected)", frequency);
  cmd.AddValue ("distance", "Distance in meters between the station and the access point", distance);
  cmd.AddValue ("simulationTime", "Simulation time in seconds", simulationTime);
  cmd.AddValue ("udp", "UDP if set to 1, TCP otherwise", udp);
  cmd.AddValue ("useRts", "Enable/disable RTS/CTS", useRts);
  cmd.AddValue ("mcs", "if set, limit testing to a specific MCS (0-7)", mcs);
  cmd.AddValue ("minExpectedThroughput", "if set, simulation fails if the lowest throughput is below this value", minExpectedThroughput);
  cmd.AddValue ("maxExpectedThroughput", "if set, simulation fails if the highest throughput is above this value", maxExpectedThroughput);
  cmd.AddValue ("step", "Granularity of the results to be plotted in meters", step);
  cmd.AddValue ("nStreams", "Number of MIMO streams", nStreams);
  cmd.AddValue ("fileName", "Name of the csv output file", fileName);

  cmd.Parse (argc,argv);

  csv.open ("resultados/"+fileName+".csv");

  if (useRts)
    {
      Config::SetDefault ("ns3::WifiRemoteStationManager::RtsCtsThreshold", StringValue ("0"));
    }

/*   double prevThroughput [12];
  for (uint32_t l = 0; l < 12; l++)
    {
      prevThroughput[l] = 0;
    } */

  std::cout << "MCS value" << "\t\t" << "Channel width" << "\t\t" << "Guard Interval" << "\t\t" << "Throughput" << "\t\t" << "Distance" << '\n';
  csv << "MCS value" << "," << "Channel width" << "," << "Guard Interval" << "," << "Throughput" << "," << "Distance" << '\n';


  int minMcs = 0;
  int maxMcs = 11;
  if (mcs >= 0 && mcs <= 11)
    {
      minMcs = mcs;
      maxMcs = mcs;
    }
  for (distance = 0; distance <= 50;) {  
    for (int mcs = minMcs; mcs <= maxMcs; mcs++)
        {
        uint8_t index = 0;
        // double previous = 0;
        uint8_t maxChannelWidth = frequency == 2.4 ? 40 : 160;
        for (int channelWidth = 20; channelWidth <= maxChannelWidth; ) //MHz
            {
            for (int gi = 3200; gi >= 800; ) //Nanoseconds
                {
                uint32_t payloadSize; //1500 byte IP packet
                if (udp)
                    {
                    payloadSize = 1472; //bytes
                    }
                else
                    {
                    payloadSize = 1448; //bytes
                    Config::SetDefault ("ns3::TcpSocket::SegmentSize", UintegerValue (payloadSize));
                    }

                NodeContainer wifiStaNode;
                wifiStaNode.Create (1);
                NodeContainer wifiApNode;
                wifiApNode.Create (1);

                YansWifiChannelHelper channel = YansWifiChannelHelper::Default ();
                YansWifiPhyHelper phy = YansWifiPhyHelper::Default ();
                phy.SetChannel (channel.Create ());

                // Set guard interval
                phy.Set ("GuardInterval", TimeValue (NanoSeconds (gi)));

                // Set MIMO capabilities
                phy.Set ("Antennas", UintegerValue (nStreams));
                phy.Set ("MaxSupportedTxSpatialStreams", UintegerValue (nStreams));
                phy.Set ("MaxSupportedRxSpatialStreams", UintegerValue (nStreams));

                WifiMacHelper mac;
                WifiHelper wifi;
                if (frequency == 5.0)
                    {
                    wifi.SetStandard (WIFI_PHY_STANDARD_80211ax_5GHZ);
                    }
                else if (frequency == 2.4)
                    {
                    wifi.SetStandard (WIFI_PHY_STANDARD_80211ax_2_4GHZ);
                    Config::SetDefault ("ns3::LogDistancePropagationLossModel::ReferenceLoss", DoubleValue (40.046));
                    }
                else
                    {
                    std::cout << "Wrong frequency value!" << std::endl;
                    return 0;
                    }

                std::ostringstream oss;
                oss << "HeMcs" << mcs;
                wifi.SetRemoteStationManager ("ns3::ConstantRateWifiManager","DataMode", StringValue (oss.str ()),
                                                "ControlMode", StringValue (oss.str ()));

                Ssid ssid = Ssid ("ns3-80211ax");

                mac.SetType ("ns3::StaWifiMac",
                            "Ssid", SsidValue (ssid));

                NetDeviceContainer staDevice;
                staDevice = wifi.Install (phy, mac, wifiStaNode);

                mac.SetType ("ns3::ApWifiMac",
                            "EnableBeaconJitter", BooleanValue (false),
                            "Ssid", SsidValue (ssid));

                NetDeviceContainer apDevice;
                apDevice = wifi.Install (phy, mac, wifiApNode);

                // Set channel width
                Config::Set ("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Phy/ChannelWidth", UintegerValue (channelWidth));

                // mobility.
                MobilityHelper mobility;
                Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator> ();

                positionAlloc->Add (Vector (0.0, 0.0, 0.0));
                positionAlloc->Add (Vector (distance, 0.0, 0.0));
                mobility.SetPositionAllocator (positionAlloc);

                mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");

                mobility.Install (wifiApNode);
                mobility.Install (wifiStaNode);

                /* Internet stack*/
                InternetStackHelper stack;
                stack.Install (wifiApNode);
                stack.Install (wifiStaNode);

                Ipv4AddressHelper address;
                address.SetBase ("192.168.1.0", "255.255.255.0");
                Ipv4InterfaceContainer staNodeInterface;
                Ipv4InterfaceContainer apNodeInterface;

                staNodeInterface = address.Assign (staDevice);
                apNodeInterface = address.Assign (apDevice);

                /* Setting applications */
                ApplicationContainer serverApp;
                if (udp)
                    {
                    //UDP flow
                    uint16_t port = 9;
                    UdpServerHelper server (port);
                    serverApp = server.Install (wifiStaNode.Get (0));
                    serverApp.Start (Seconds (0.0));
                    serverApp.Stop (Seconds (simulationTime + 1));

                    UdpClientHelper client (staNodeInterface.GetAddress (0), port);
                    client.SetAttribute ("MaxPackets", UintegerValue (4294967295u));
                    client.SetAttribute ("Interval", TimeValue (Time ("0.00001"))); //packets/s
                    client.SetAttribute ("PacketSize", UintegerValue (payloadSize));
                    ApplicationContainer clientApp = client.Install (wifiApNode.Get (0));
                    clientApp.Start (Seconds (1.0));
                    clientApp.Stop (Seconds (simulationTime + 1));
                    }
                else
                    {
                    //TCP flow
                    uint16_t port = 50000;
                    Address localAddress (InetSocketAddress (Ipv4Address::GetAny (), port));
                    PacketSinkHelper packetSinkHelper ("ns3::TcpSocketFactory", localAddress);
                    serverApp = packetSinkHelper.Install (wifiStaNode.Get (0));
                    serverApp.Start (Seconds (0.0));
                    serverApp.Stop (Seconds (simulationTime + 1));

                    OnOffHelper onoff ("ns3::TcpSocketFactory", Ipv4Address::GetAny ());
                    onoff.SetAttribute ("OnTime",  StringValue ("ns3::ConstantRandomVariable[Constant=1]"));
                    onoff.SetAttribute ("OffTime", StringValue ("ns3::ConstantRandomVariable[Constant=0]"));
                    onoff.SetAttribute ("PacketSize", UintegerValue (payloadSize));
                    onoff.SetAttribute ("DataRate", DataRateValue (1000000000)); //bit/s
                    AddressValue remoteAddress (InetSocketAddress (staNodeInterface.GetAddress (0), port));
                    onoff.SetAttribute ("Remote", remoteAddress);
                    ApplicationContainer clientApp = onoff.Install (wifiApNode.Get (0));
                    clientApp.Start (Seconds (1.0));
                    clientApp.Stop (Seconds (simulationTime + 1));
                    }

                Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

                Simulator::Stop (Seconds (simulationTime + 1));
                Simulator::Run ();

                uint64_t rxBytes = 0;
                if (udp)
                    {
                    rxBytes = payloadSize * DynamicCast<UdpServer> (serverApp.Get (0))->GetReceived ();
                    }
                else
                    {
                    rxBytes = DynamicCast<PacketSink> (serverApp.Get (0))->GetTotalRx ();
                    }
                double throughput = (rxBytes * 8) / (simulationTime * 1000000.0); //Mbit/s

                Simulator::Destroy ();

                std::cout << mcs << "\t\t\t" << channelWidth << " MHz\t\t\t" << gi << " ns\t\t\t" << throughput << " Mbit/s"  << "\t\t" << distance << " m" << std::endl;
                csv << mcs << "," << channelWidth << "," << gi << "," << throughput << "," << distance << std::endl;


    /*               //test first element
                if (mcs == 0 && channelWidth == 20 && gi == 3200)
                    {
                    if (throughput < minExpectedThroughput)
                        {
                        NS_LOG_ERROR ("Obtained throughput " << throughput << " is not expected!");
                        exit (1);
                        }
                    }
                //test last element
                if (mcs == 11 && channelWidth == 160 && gi == 800)
                    {
                    if (maxExpectedThroughput > 0 && throughput > maxExpectedThroughput)
                        {
                        NS_LOG_ERROR ("Obtained throughput " << throughput << " is not expected!");
                        exit (1);
                        }
                    }
                //test previous throughput is smaller (for the same mcs)
                if (throughput > previous)
                    {
                    previous = throughput;
                    }
                else
                    {
                    NS_LOG_ERROR ("Obtained throughput " << throughput << " is not expected!");
                    exit (1);
                    }
                //test previous throughput is smaller (for the same channel width and GI)
                if (throughput > prevThroughput [index])
                    {
                    prevThroughput [index] = throughput;
                    }
                else
                    {
                    NS_LOG_ERROR ("Obtained throughput " << throughput << " is not expected!");
                    exit (1);
                    } */
                index++;
                gi /= 2;
                }
            channelWidth *= 2;
            }
        }
    distance+=step;
  }
  csv.close();
  return 0;
}
