#include <iostream>
#include <unistd.h>
#include "stdlib.h"
#include "PcapLiveDeviceList.h"
#include "SystemUtils.h"
#include <stdio.h>
#include <string.h> /* For strncpy */
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <netinet/in.h>
#include <net/if.h>
#include <arpa/inet.h>
#include <string>
#include <fstream>

#include <cpprest/http_listener.h>              // HTTP server
#include <cpprest/json.h>                       // JSON library
#include <cpprest/uri.h>                        // URI library
#include <cpprest/ws_client.h>                  // WebSocket client
#include <cpprest/asyncrt_utils.h>              // Async helpers
#include <cpprest/containerstream.h>            // Async streams backed by STL containers
#include <cpprest/interopstream.h>              // Bridges for integrating Async streams with STL and WinRT streams
#include <cpprest/rawptrstream.h>               // Async streams backed by raw pointer to memory
#include <cpprest/producerconsumerstream.h>     // Async streams for producer consumer scenarios
#include <cpprest/filestream.h>

#include <string>
#include <vector>
#include <algorithm>
#include <sstream>
#include <iostream>
#include <fstream>
#include <random>

#pragma warning ( push )
#pragma warning ( disable : 4457 )
#pragma warning ( pop )
#include <locale>
#include <ctime>
#include <vector>
/*
*Listens on eth0 for a given interval 
*Stores packet data in PacketStats Structure
*/
using namespace std;
using namespace web;
using namespace http;
using namespace utility;
using namespace http::experimental::listener;

#define TRACE(msg)            wcout << msg
#define TRACE_ACTION(a, k, v) wcout << a << L" (" << k << L", " << v << L")\n"
using namespace std;
struct PacketStats
{
    int ethPacketCount;
    int ipv4PacketCount;
    int ipv6PacketCount;
    int tcpPacketCount;
    int udpPacketCount;
    int dnsPacketCount;
    int httpPacketCount;
    int sslPacketCount;
    void clear() { ethPacketCount = 0; ipv4PacketCount = 0; ipv6PacketCount = 0; tcpPacketCount = 0; udpPacketCount = 0; tcpPacketCount = 0; dnsPacketCount = 0; httpPacketCount = 0; sslPacketCount = 0; }
    PacketStats() { clear(); }
    void consumePacket(pcpp::Packet& packet)
    {
        if (packet.isPacketOfType(pcpp::Ethernet))
            ethPacketCount++;
        if (packet.isPacketOfType(pcpp::IPv4))
            ipv4PacketCount++;
        if (packet.isPacketOfType(pcpp::IPv6))
            ipv6PacketCount++;
        if (packet.isPacketOfType(pcpp::TCP))
            tcpPacketCount++;
        if (packet.isPacketOfType(pcpp::UDP))
            udpPacketCount++;
        if (packet.isPacketOfType(pcpp::DNS))
            dnsPacketCount++;
        if (packet.isPacketOfType(pcpp::HTTP))
            httpPacketCount++;
        if (packet.isPacketOfType(pcpp::SSL))
            sslPacketCount++;
    }
};
std::string getListenIp()
{
    int fd;
    struct ifreq ifr;
    fd = socket(AF_INET, SOCK_DGRAM, 0);
    ifr.ifr_addr.sa_family = AF_INET;
    strncpy(ifr.ifr_name, "eth0", IFNAMSIZ-1);
    ioctl(fd, SIOCGIFADDR, &ifr);
    close(fd);
    std::string ip_addr = std::string(inet_ntoa(((struct sockaddr_in *)&ifr.ifr_addr)->sin_addr));
    std::cout<<"interface ip: "<<ip_addr<<std::endl;
    return ip_addr;
}

vector<vector<pair<string, int>>> protStatsList;
void handle_get(http_request request)
{
   TRACE(L"\nhandle GET\n");
   json::value timeSeries;
   for (auto time : protStatsList){
       timeSeries["PacketStats"][time[0].first] = json::value::number(time[0].second);
       timeSeries["PacketStats"][time[1].first] = json::value::number(time[1].second);
       timeSeries["PacketStats"][time[2].first] = json::value::number(time[2].second);
       timeSeries["PacketStats"][time[3].first] = json::value::number(time[3].second);
       timeSeries["PacketStats"][time[4].first] = json::value::number(time[4].second);
       timeSeries["PacketStats"][time[5].first] = json::value::number(time[5].second);
       timeSeries["PacketStats"][time[6].first] = json::value::number(time[6].second);
       timeSeries["PacketStats"][time[7].first] = json::value::number(time[7].second);
       timeSeries["PacketStats"][time[8].first] = json::value::number(time[8].second);
   }
   cout<<timeSeries.serialize()<<endl;
   request.reply(status_codes::OK, timeSeries);
}

int main(int argc, char* argv[])
{
    

    /*front end request listener*/
    http_listener listener("http://localhost:5000");
    listener.support(methods::GET, handle_get);

    try
    {
        listener
            .open()
            .then([&listener](){TRACE(L"\nstarting to listen\n");})
            .wait();

        while (true);
    }
    catch (exception const & e)
    {
        wcout << e.what() << endl;
    }

    /*Pcap device thread*/
    std::string interfaceIPAddr = getListenIp();
	PacketStats stats;

	pcpp::PcapLiveDevice* dev = pcpp::PcapLiveDeviceList::getInstance().getPcapLiveDeviceByIp(interfaceIPAddr);
	if (dev == NULL)
	{
		std::cerr << "Cannot find interface with IPv4 address of '" << interfaceIPAddr << "'" << std::endl;
		return 1;
	}
	std::cout
    << "Interface info:" << std::endl
    << "   Interface name:        " << dev->getName() << std::endl // get interface name
    << "   Interface description: " << dev->getDesc() << std::endl // get interface description
    << "   MAC address:           " << dev->getMacAddress() << std::endl // get interface MAC address
    << "   Default gateway:       " << dev->getDefaultGateway() << std::endl // get default gateway
    << "   Interface MTU:         " << dev->getMtu() << std::endl; // get interface MTU

	if (dev->getDnsServers().size() > 0)
		std::cout << "   DNS server:            " << dev->getDnsServers().at(0) << std::endl;
	if (!dev->open())
	{
		std::cerr << "Cannot open device" << std::endl;
		return 1;
	}

	pcpp::RawPacketVector packetVec;

	// start capturing packets. All packets will be added to the packet vector
    int time = 1000;
	dev->startCapture(packetVec);
    while(1){
        sleep(5);
        for (pcpp::RawPacketVector::ConstVectorIterator iter = packetVec.begin(); iter != packetVec.end(); iter++)
        {
            // parse raw packet
            pcpp::Packet parsedPacket(*iter);

            // feed packet to the stats object
            stats.consumePacket(parsedPacket);
        }
        vector<pair<string, int>> protStats;
        protStats.push_back(make_pair("time", time));
        time+=1000;
        protStats.push_back(make_pair("ETH", stats.ethPacketCount));
        protStats.push_back(make_pair("IPV4", stats.ipv4PacketCount));
        protStats.push_back(make_pair("IPV6", stats.ipv6PacketCount));
        protStats.push_back(make_pair("UDP", stats.udpPacketCount));
        protStats.push_back(make_pair("TCP", stats.tcpPacketCount));
        protStats.push_back(make_pair("DNS", stats. dnsPacketCount));
        protStats.push_back(make_pair("HTTP", stats.httpPacketCount));
        protStats.push_back(make_pair("SSL", stats.sslPacketCount));

        protStatsList.push_back(protStats);
        if(protStatsList.size() > 100)
        {
            protStatsList.erase(protStatsList.begin());
        }
    }
	
    
    

	// stop capturing packets
	dev->stopCapture();
	// stats.printToConsole();
}
