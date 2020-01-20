/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */

#include "ns3/vector.h"
#include "ns3/address.h"
#include "ns3/string.h"
#include "ns3/socket.h"
#include "ns3/double.h"
#include "ns3/config.h"
#include "ns3/log.h"
#include "ns3/command-line.h"
#include "ns3/mobility-model.h"
#include "ns3/yans-wifi-helper.h"
#include "ns3/position-allocator.h"
#include "ns3/mobility-helper.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-stack-helper.h"
#include "ns3/ipv4-address-helper.h"
#include "ns3/ipv4-interface-container.h"
#include "ns3/ocb-wifi-mac.h"
#include "ns3/wifi-80211p-helper.h"
#include "ns3/wave-mac-helper.h"
#include <iostream>
#include <list>
#include <fstream>
#include <sstream>
#include <unistd.h>
#include <math.h>
#include <unistd.h>
#include <ns3/core-module.h>
#include <ns3/ns2-mobility-helper.h>
#include <nckernel/nckernel.h>
#include <nckernel/skb.h>
#include <nckernel/timer.h>
#include <nckernel/api.h>
#include <nckernel/config.h>
#include <chrono>

using namespace ns3;
NS_LOG_COMPONENT_DEFINE ("Platooning");



struct memory_packet{
  uint32_t node_id;
  uint32_t sequence_number;
  uint32_t creation_time;
  uint32_t received_time;
  uint32_t payload_length;
  uint8_t payload[1500];
};
struct coded_memory_packet{
uint32_t node_id;
int direction; // 1 equals from low id to high id, -1 from high to low
uint32_t distance;
uint32_t received_time;
};

struct GenTrafficValues{
  uint32_t node_id;
  uint32_t n_vehicles;
  struct nck_encoder *enc;
  struct nck_recoder *rec;
  std::ostream *sendLog;
  bool networkCoding;
};

struct ReceiveValues{
  uint32_t node_id;
  uint32_t n_vehicles;
  struct nck_recoder *rec;
  uint32_t maxTransmissionDelay;
  uint32_t maxSourceLoss;
  std::ostream *recvLog;
  std::ostream *fwdLog;
  bool fwdAll;
  bool networkCoding;
  bool noForward;
  uint32_t packet_interval;

};

struct interflow_sw_coded_packet {
	uint8_t packet_type;
	uint8_t order;
	uint8_t flags;
	uint8_t reserved;
	uint16_t packet_no;
};
struct kodo_header {
	uint32_t seqno;
	uint8_t systematic_flag;
};

const uint32_t maxMemory = 1000;

static void
CourseChange (std::ostream *os, std::string foo, Ptr<const MobilityModel> mobility)
{
  Vector pos = mobility->GetPosition (); // Get position
  Vector vel = mobility->GetVelocity (); // Get velocity

  // Prints position and velocities
  *os << Simulator::Now () << " POS: x=" << pos.x << ", y=" << pos.y
      << ", z=" << pos.z << "; VEL: x=" << vel.x << ", y=" << vel.y
      << ", z=" << vel.z << std::endl;
}

void ForwardUncodedPacket (Ptr<Socket> socket, Ptr<Packet> packet)
{
  socket->Send(packet);
}

void ForwardPacket (Ptr<Socket> socket, ReceiveValues *Values, uint32_t fwd_reason, uint32_t fwd_delay)
{
  auto start = std::chrono::high_resolution_clock::now();
  struct nck_recoder *nck_rec = Values->rec;
  if (nck_has_coded(nck_rec))
  {
    struct sk_buff rpacket;
    uint8_t rbuffer[1500];
    struct sk_buff fpacket;
    uint8_t fbuffer[nck_rec->coded_size + sizeof(uint32_t)];
    skb_new(&rpacket, rbuffer, sizeof(rbuffer));
    if (!nck_get_coded(nck_rec, &rpacket))
    {
      uint32_t creation_time = (uint32_t) Simulator::Now().GetMicroSeconds();
      skb_new(&fpacket, fbuffer, sizeof(fbuffer));
      skb_reserve(&fpacket, sizeof(uint32_t));
      memcpy (fpacket.tail, rpacket.data, rpacket.len);
      skb_put(&fpacket, rpacket.len);
      skb_push_u32(&fpacket, creation_time);
      char stringbuf[1500];
      uint32_t i;
      char* buf2 = stringbuf;
      char* endofbuf = stringbuf + sizeof(stringbuf);
      for (i = 0; i < rpacket.len; i++)
      {

        if (buf2 + 5 < endofbuf)
        {
          if (i > 0)
          {
              buf2 += sprintf(buf2, ":");
          }
          buf2 += sprintf(buf2, "%02X", rpacket.data[i]);
        }
      }
      auto stop = std::chrono::high_resolution_clock::now();
      auto duration = std::chrono::duration_cast<std::chrono::microseconds>(stop - start);
      *Values->fwdLog << Values->node_id <<", " << creation_time << ", "
      << fwd_reason << ", " << fwd_delay << ", " << duration.count()
      << ", "<< stringbuf << std::endl;

      socket->Send (Create<Packet> (fpacket.data, fpacket.len));
    }
  }
}

void DecodePackets (Ptr<Socket> socket, ReceiveValues *Values, uint32_t creation_time, std::deque<memory_packet> *packet_memory, std::deque<coded_memory_packet> *coded_memory)
{
  struct nck_recoder *nck_rec = Values->rec;
  bool forwarding = false;
  uint32_t fwd_reason = 0; // 1 -> Delay, 2 -> Packet loss, 3 -> both
  uint32_t delivery_delay = 0;
  auto start = std::chrono::high_resolution_clock::now();
  while (nck_has_source(nck_rec))
  {

    struct sk_buff spacket;
    uint8_t sbuffer[1500];
    skb_new(&spacket, sbuffer, sizeof(sbuffer));
    nck_get_source(nck_rec, &spacket);
    struct memory_packet memory_packet;
    memory_packet.payload_length = spacket.len;
    memcpy(memory_packet.payload, spacket.data, spacket.len);

    char stringbuf[1500];
    uint32_t i;
    char* buf2 = stringbuf;
    char* endofbuf = stringbuf + sizeof(stringbuf);
    for (i = 0; i < spacket.len; i++)
    {

      if (buf2 + 5 < endofbuf)
      {
        if (i > 0)
        {
            buf2 += sprintf(buf2, ":");
        }
        buf2 += sprintf(buf2, "%02X", spacket.data[i]);
      }
    }

    uint32_t seq = skb_pull_u32(&spacket);
    uint32_t id = skb_pull_u32(&spacket);
    uint32_t packet_creation_time = skb_pull_u32(&spacket);
    uint32_t received_time = (uint32_t) Simulator::Now().GetMicroSeconds();
    uint32_t delay = received_time - creation_time;
    delivery_delay = received_time - packet_creation_time;
    auto decoding_finish = std::chrono::high_resolution_clock::now();
    auto decoding_duration = std::chrono::duration_cast<std::chrono::microseconds>(decoding_finish - start);
    int direction = 0;
    uint32_t distance;
    if(id > Values->node_id)
    {
      direction = -1;
      distance = id - Values->node_id;
    }
    if(id < Values->node_id)
    {
      direction = 1;
      distance = Values->node_id - id;
    }

    memory_packet.node_id = id;
    memory_packet.sequence_number = seq;
    memory_packet.creation_time = packet_creation_time;
    memory_packet.received_time = received_time;

    *Values->recvLog << memory_packet.node_id <<", "
    << memory_packet.sequence_number << ", "
    << memory_packet.creation_time << ", "
    << memory_packet.received_time << ", "
    << delivery_delay << ", "
    << decoding_duration.count() << ", "
    << delivery_delay + decoding_duration.count() << ", "
    << memory_packet.payload_length << ", "
    << stringbuf << std::endl;

    if(packet_memory->size() == maxMemory)
      packet_memory->pop_front();
    packet_memory->push_back(memory_packet);
    uint32_t packets_from_node = 1;
    uint32_t packets_missed = 0;
    uint32_t last_seq = 0;
    bool got_first = false;

    if (packet_memory->size()>0)
    {
      for (uint32_t k = 0; k < packet_memory->size(); k++)
      {
        if (packet_memory->at(k).node_id == id)
          {
            if (!got_first)
            {
              last_seq = packet_memory->at(k).sequence_number;
              got_first = true;
            }
            if (packet_memory->at(k).sequence_number - last_seq > 1)
             {
               packets_missed += packet_memory->at(k).sequence_number - last_seq - 1;
             }
             packets_from_node += packet_memory->at(k).sequence_number - last_seq;
             last_seq = packet_memory->at(k).sequence_number;

          }

      }
    }
    if ((Values->maxTransmissionDelay > 0) && (delay > Values->maxTransmissionDelay))
    {
      if (coded_memory->size()>0)
      {
        for (uint32_t j = 0; j < coded_memory->size(); j++)
        {
          if (coded_memory->at(j).direction == direction)
          {
            if (!((coded_memory->at(j).distance < distance)&&(received_time - coded_memory->at(j).received_time < Values->maxTransmissionDelay)))
              forwarding = true;
          }
        }
      }
      if (forwarding)
      {
        if (fwd_reason == 0)
          fwd_reason = 1;
        else if (fwd_reason == 2)
          fwd_reason = 3;

      }
    }
    if(( double)packets_missed * 100 / (double) packets_from_node > (double) Values->maxSourceLoss)
    {
      forwarding = true;
      if (fwd_reason == 0)
        fwd_reason = 2;
      else if (fwd_reason == 1)
        fwd_reason = 3;
    }

  }
  if (Values->fwdAll)
    forwarding = true;
  if (Values->node_id == 0 || Values->node_id == Values->n_vehicles -1)
    forwarding = false;
  if (delivery_delay > Values->packet_interval)
  {
    forwarding = false;
  }
  if (Values->noForward)
  {
    forwarding = false;
  }
  auto stop = std::chrono::high_resolution_clock::now();
  auto duration = std::chrono::duration_cast<std::chrono::microseconds>(stop - start);
  if (forwarding)
    Simulator::Schedule (Seconds(((double)duration.count())/1000000), &ForwardPacket, socket, Values, fwd_reason, duration.count());
}

void ReceivePacket (ReceiveValues *Values, std::deque<memory_packet> *packet_memory, std::deque<coded_memory_packet> *coded_memory, Ptr<Socket> socket)
{
  Address address;
  while (Ptr<Packet> ns3Packet = socket->RecvFrom (address))
  {
    if (Values->networkCoding)
    {
      auto start = std::chrono::high_resolution_clock::now();

      struct nck_recoder *nck_rec = Values->rec;
      uint8_t addbuffer[1500];
      struct sk_buff rpacket;
      struct sk_buff cpacket;
      struct kodo_header *kodo_header;
      struct interflow_sw_coded_packet *interflow_sw_coded_packet;
      uint8_t rbuffer[1500];
      uint8_t cbuffer[1500];
      skb_new(&rpacket, rbuffer, sizeof(rbuffer));
      skb_new(&cpacket, cbuffer, sizeof(cbuffer));
      uint32_t size = ns3Packet->CopyData(rpacket.tail, skb_tailroom(&rpacket));
      skb_put(&rpacket, size);
      uint32_t creation_time = skb_pull_u32(&rpacket);
      uint32_t received_time =(uint32_t) Simulator::Now().GetMicroSeconds();
      interflow_sw_coded_packet = (struct interflow_sw_coded_packet *)rpacket.data;
      kodo_header = (struct kodo_header *) (interflow_sw_coded_packet + 1);
      address.CopyTo(addbuffer);
      if (kodo_header->systematic_flag == 0x00)
      {
        struct coded_memory_packet coded_packet;
        coded_packet.node_id = addbuffer[3]-1;
        coded_packet.distance = 0;
        coded_packet.direction = 0;
        if (coded_packet.node_id > Values->node_id)
        {
          coded_packet.direction = -1;
          coded_packet.distance = coded_packet.node_id - Values->node_id;
        }
        if (coded_packet.node_id < Values->node_id)
        {
          coded_packet.direction = 1;
          coded_packet.distance = Values->node_id - coded_packet.node_id;
        }
        coded_packet.received_time = received_time;
        if (!(coded_packet.distance == 0))
          coded_memory->push_back(coded_packet);

        if (coded_memory->size()>0)
        {
          if (received_time - coded_memory->at(0).received_time > 2 * Values->maxTransmissionDelay)
            coded_memory->pop_front();
        }
      }
      memcpy(cbuffer, rpacket.data, rpacket.len);
      skb_put(&cpacket, rpacket.len);
      nck_put_coded(nck_rec, &cpacket);
      auto stop = std::chrono::high_resolution_clock::now();
      auto duration = std::chrono::duration_cast<std::chrono::microseconds>(stop - start);
      Simulator::Schedule (Seconds(((double) duration.count())/1000), &DecodePackets, socket, Values, creation_time, packet_memory, coded_memory);
    }
    else
    {
      auto start = std::chrono::high_resolution_clock::now();
      uint8_t addbuffer[1500];
      uint8_t rbuffer[1500];
      struct sk_buff rpacket;
      skb_new(&rpacket, rbuffer, sizeof(rbuffer));
      uint32_t size = ns3Packet->CopyData(rpacket.tail, skb_tailroom(&rpacket));
      skb_put(&rpacket, size);
      uint32_t creation_time = skb_pull_u32(&rpacket);
      skb_push_u32(&rpacket, creation_time);
      address.CopyTo(addbuffer);
      struct coded_memory_packet coded_packet;
      coded_packet.node_id = addbuffer[3]-1;
      coded_packet.distance = 0;
      coded_packet.direction = 0;
      if (coded_packet.node_id > Values->node_id)
      {
        coded_packet.direction = -1;
        coded_packet.distance = coded_packet.node_id - Values->node_id;
      }
      if (coded_packet.node_id < Values->node_id)
      {
        coded_packet.direction = 1;
        coded_packet.distance = Values->node_id - coded_packet.node_id;
      }
      coded_packet.received_time = (uint32_t) Simulator::Now().GetMicroSeconds();
      if (!(coded_packet.distance == 0))
        coded_memory->push_back(coded_packet);

      if (coded_memory->size()>0)
      {
        if (coded_packet.received_time - coded_memory->at(0).received_time > 2 * Values->maxTransmissionDelay)
          coded_memory->pop_front();
      }

      struct memory_packet memory_packet;
      memory_packet.payload_length = rpacket.len;
      memcpy(memory_packet.payload, rpacket.data, rpacket.len);

      char stringbuf[1500];
      uint32_t i;
      char* buf2 = stringbuf;
      char* endofbuf = stringbuf + sizeof(stringbuf);
      for (i = 0; i < rpacket.len; i++)
      {

        if (buf2 + 5 < endofbuf)
        {
          if (i > 0)
          {
              buf2 += sprintf(buf2, ":");
          }
          buf2 += sprintf(buf2, "%02X", rpacket.data[i]);
        }
      }
      creation_time = skb_pull_u32(&rpacket);
      uint32_t seq = skb_pull_u32(&rpacket);
      uint32_t id = skb_pull_u32(&rpacket);
      uint32_t packet_creation_time = skb_pull_u32(&rpacket);
      uint32_t received_time = (uint32_t) Simulator::Now().GetMicroSeconds();
      uint32_t delay = received_time - creation_time;
      uint32_t delivery_delay = received_time - packet_creation_time;
      int direction = 0;
      uint32_t distance;
      if(id > Values->node_id)
      {
        direction = -1;
        distance = id - Values->node_id;
      }
      if(id < Values->node_id)
      {
        direction = 1;
        distance = Values->node_id - id;
      }

      memory_packet.node_id = id;
      memory_packet.sequence_number = seq;
      memory_packet.creation_time = packet_creation_time;
      memory_packet.received_time = received_time;

      *Values->recvLog << memory_packet.node_id <<", "
      << memory_packet.sequence_number << ", "
      << memory_packet.creation_time << ", "
      << memory_packet.received_time << ", "
      << delivery_delay << ", "
      << 0 << ", "
      << delivery_delay << ", "
      << memory_packet.payload_length << ", "
      << stringbuf << std::endl;

      if(packet_memory->size() == maxMemory)
        packet_memory->pop_front();
      packet_memory->push_back(memory_packet);
      uint32_t packets_from_node = 1;
      uint32_t packets_missed = 0;
      uint32_t last_seq = 0;
      bool got_first = false;

      if (packet_memory->size()>0)
      {
        for (uint32_t k = 0; k < packet_memory->size(); k++)
        {
          if (packet_memory->at(k).node_id == id)
            {
              if (!got_first)
              {
                last_seq = packet_memory->at(k).sequence_number;
                got_first = true;
              }
              if (packet_memory->at(k).sequence_number - last_seq > 1)
               {
                 packets_missed += packet_memory->at(k).sequence_number - last_seq - 1;
               }
               packets_from_node += packet_memory->at(k).sequence_number - last_seq;
               last_seq = packet_memory->at(k).sequence_number;

            }

        }
      }
      bool forwarding = false;
      uint32_t fwd_reason = 0;
      if ((Values->maxTransmissionDelay > 0) && (delay > Values->maxTransmissionDelay))
      {
        if (coded_memory->size()>0)
        {
          for (uint32_t j = 0; j < coded_memory->size(); j++)
          {
            if (coded_memory->at(j).direction == direction)
            {
              if (!((coded_memory->at(j).distance < distance)&&(received_time - coded_memory->at(j).received_time < Values->maxTransmissionDelay)))
                forwarding = true;
            }
          }
        }
        if (forwarding)
        {
          if (fwd_reason == 0)
            fwd_reason = 1;
          else if (fwd_reason == 2)
            fwd_reason = 3;

        }
      }
      if(( double)packets_missed * 100 / (double) packets_from_node > (double) Values->maxSourceLoss)
      {
        forwarding = true;
        if (fwd_reason == 0)
          fwd_reason = 2;
        else if (fwd_reason == 1)
          fwd_reason = 3;
      }
      if (Values->fwdAll)
        forwarding = true;
      if (Values->node_id == 0 || Values->node_id == Values->n_vehicles -1)
        forwarding = false;
      if (delivery_delay > Values->packet_interval)
      {
        forwarding = false;
      }
      if (Values->noForward)
      {
        forwarding = false;
      }
      if (forwarding)
      {
        struct sk_buff fpacket;
        uint8_t fbuffer[1500];
        skb_new(&fpacket, fbuffer, sizeof(fbuffer));
        uint32_t forward_time = (uint32_t) Simulator::Now().GetMicroSeconds();
        skb_reserve(&fpacket, 4 * sizeof(uint32_t));
        memcpy (fpacket.tail, rpacket.data, rpacket.len);
        skb_put(&fpacket, rpacket.len);
        skb_push_u32(&fpacket, packet_creation_time);
        skb_push_u32(&fpacket, id);
        skb_push_u32(&fpacket, seq);
        skb_push_u32(&fpacket, forward_time);
        char stringbuf[1500];
        uint32_t i;
        char* buf2 = stringbuf;
        char* endofbuf = stringbuf + sizeof(stringbuf);
        for (i = 0; i < fpacket.len; i++)
          {

            if (buf2 + 5 < endofbuf)
            {
              if (i > 0)
              {
                  buf2 += sprintf(buf2, ":");
              }
              buf2 += sprintf(buf2, "%02X", fpacket.data[i]);
            }
          }
        Ptr<Packet> packet=Create<Packet> (fpacket.data, fpacket.len);
        auto stop = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(stop - start);
        *Values->fwdLog << Values->node_id <<", " << forward_time << ", "
        << fwd_reason << ", " << duration.count() << ", " << 0 << ", "
        << stringbuf << std::endl;

        Simulator::Schedule (Seconds(((double)duration.count())/1000000), &ForwardUncodedPacket, socket, packet);
      }
    }


  }
}

static void GenerateTraffic (Ptr<Socket> socket, uint32_t pktSize,
  uint32_t pktCount, Time pktInterval, uint32_t seq_num, struct GenTrafficValues Values)
{
  if (pktCount > 0)
  {
    if (Values.networkCoding)
    {
      struct nck_encoder *nck_enc = Values.enc;
      struct nck_recoder *nck_rec = Values.rec;
      uint8_t buffer[nck_enc->source_size];
      uint8_t cbuffer[nck_enc->coded_size];
      uint8_t fbuffer[sizeof(cbuffer) + sizeof(uint32_t)];
      /* Create a packet with Node-Id and sequence number for the first 8 bytes and
      the remaining number of packets for the rest */
      memset(buffer, 0, sizeof(buffer));
      memset(cbuffer, 0, sizeof(cbuffer));
      memset(fbuffer, 0, sizeof(fbuffer));
      struct sk_buff packet; //initial uncoded packet
      struct sk_buff cpacket; //coded packet
      struct sk_buff fpacket; //final packet with additional timing information header
      skb_new(&packet, buffer, sizeof(buffer));
      skb_new(&cpacket, cbuffer, sizeof(cbuffer));
      skb_reserve(&packet, 3 * sizeof(uint32_t));
      for (int j = 0; skb_tailroom(&packet) >= sizeof(uint8_t); j++)
      {
        skb_put_u8(&packet, (uint8_t) rand());
      }
      uint32_t creation_time = (uint32_t) Simulator::Now().GetMicroSeconds();
      skb_push_u32(&packet, creation_time);
      skb_push_u32(&packet, Values.node_id);
      skb_push_u32(&packet, (uint32_t) seq_num);

      auto start = std::chrono::high_resolution_clock::now();

      char stringbuf[1500];
      uint32_t i;
      char* buf2 = stringbuf;
      char* endofbuf = stringbuf + sizeof(stringbuf);
      for (i = 0; i < packet.len; i++)
      {
        if (buf2 + 5 < endofbuf)
        {
          if (i > 0)
          {
              buf2 += sprintf(buf2, ":");
          }
          buf2 += sprintf(buf2, "%02X", packet.data[i]);
        }
      }


      nck_put_source(nck_enc,&packet);
      if (nck_has_coded(nck_enc))
      {
        if (nck_get_coded(nck_enc, &cpacket))
        {
          printf("Failed to get a coded packet!\n");
        }
        skb_new(&fpacket, fbuffer, sizeof(fbuffer));
        skb_reserve(&fpacket, sizeof(uint32_t));
        memcpy (fpacket.tail, cpacket.data, cpacket.len);
        skb_put(&fpacket, cpacket.len);
        skb_push_u32(&fpacket, creation_time);
        auto stop = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(stop - start);

        *Values.sendLog << Values.node_id <<", "
        << seq_num << ", "
        << creation_time << ", "
        << duration.count() << ", "
        << packet.len << ", "
        << stringbuf << std::endl;
        socket->Send (Create<Packet> (fpacket.data, fpacket.len));

        nck_put_coded(nck_rec, &cpacket);

        Simulator::Schedule (pktInterval, &GenerateTraffic,
          socket, pktSize, pktCount - 1, pktInterval,  seq_num + 1 , Values);
      }
      else
      {
        Simulator::Schedule (pktInterval, &GenerateTraffic,
          socket, pktSize, pktCount, pktInterval, seq_num , Values);
      }
    }
    else
    {
      uint8_t buffer[pktSize];
      memset(buffer, 0, sizeof(buffer));
      struct sk_buff packet;
      skb_new(&packet, buffer, sizeof(buffer));
      skb_reserve(&packet, 4 * sizeof(uint32_t));
      for (int j = 0; skb_tailroom(&packet) >= sizeof(uint8_t); j++)
      {
        skb_put_u8(&packet, (uint8_t) rand());
      }
      uint32_t creation_time = (uint32_t) Simulator::Now().GetMicroSeconds();
      skb_push_u32(&packet, creation_time);
      skb_push_u32(&packet, Values.node_id);
      skb_push_u32(&packet, (uint32_t) seq_num);
      skb_push_u32(&packet, creation_time);

      auto start = std::chrono::high_resolution_clock::now();

      char stringbuf[1500];
      uint32_t i;
      char* buf2 = stringbuf;
      char* endofbuf = stringbuf + sizeof(stringbuf);
      for (i = 0; i < packet.len; i++)
      {

        if (buf2 + 5 < endofbuf)
        {
          if (i > 0)
          {
              buf2 += sprintf(buf2, ":");
          }
          buf2 += sprintf(buf2, "%02X", packet.data[i]);
        }
      }

      auto stop = std::chrono::high_resolution_clock::now();
      auto duration = std::chrono::duration_cast<std::chrono::microseconds>(stop - start);

      *Values.sendLog << Values.node_id <<", "
      << seq_num << ", "
      << creation_time << ", "
      << duration.count() << ", "
      << packet.len << ", "
      << stringbuf << std::endl;
      socket->Send (Create<Packet> (packet.data, packet.len));
      Simulator::Schedule (pktInterval, &GenerateTraffic,
        socket, pktSize, pktCount - 1, pktInterval,  seq_num + 1 , Values);
    }
  }
}

int main (int argc, char *argv[])
{
  std::ofstream os;
  std::string logFile;
  std::string recvLogName;
  std::string sendLogName;
  std::string fwdLogName;

  srand (time(NULL));
  std::string traceFile;
  LogComponentEnable ("Ns2MobilityHelper",LOG_LEVEL_ERROR);
  std::string phyMode ("OfdmRate27MbpsBW10MHz");
  uint32_t packetSize = 1000; // bytes
  uint32_t numPackets = 1;
  double interval = 1; // seconds
  bool verbose = false;
  uint32_t numVehicles = 5;
  uint32_t maxTransmissionDelay = -1; //us, -1 -> not used
  double maxPacketLoss = 100; // 100 -> not used
  bool forwardAll = false;
  bool networkCoding = true;
  bool noForward = false;

  CommandLine cmd;
  cmd.AddValue ("traceFile", "string: Ns2 movement trace file", traceFile);
  cmd.AddValue ("phyMode", "string: Wifi Phy mode", phyMode);
  cmd.AddValue ("packetSize", "uint: size of application packet sent", packetSize);
  cmd.AddValue ("numPackets", "uint: number of packets generated", numPackets);
  cmd.AddValue ("interval", "doulbe: interval (seconds) between packets", interval);
  cmd.AddValue ("verbose", "bool: turn on all WifiNetDevice log components", verbose);
  cmd.AddValue ("numVehicles", "uint: Number of vehices in the platoon", numVehicles);
  cmd.AddValue ("maxTransmissionDelay", "uint: maximum transmission time before forwarding is used", maxTransmissionDelay);
  cmd.AddValue ("maxPacketLoss", "uint: maximum % loss of packets before forwarding is used", maxPacketLoss);
  cmd.AddValue ("logFile", "string: Mobility Log file", logFile);
  cmd.AddValue ("recvLogFile", "string: Log file template for received and decoded packets", recvLogName);
  cmd.AddValue ("sendLogFile", "string: Log file template for sent packets", sendLogName);
  cmd.AddValue ("fwdLogFile", "string: Log file template for forwarded packets", fwdLogName);
  cmd.AddValue ("forwardAll", "bool: Enables forwarding for all nodes", forwardAll);
  cmd.AddValue ("networkCoding", "bool: Enables interflow network coding on all devices", networkCoding);
  cmd.AddValue ("noForward", "bool: Disables forwarding", noForward);
  cmd.Parse (argc, argv);
  std::ofstream recvLog[numVehicles];
  std::ofstream sendLog[numVehicles];
  std::ofstream fwdLog[numVehicles];

  //GlobalValue::Bind ("SimulatorImplementationType", StringValue ("ns3::RealtimeSimulatorImpl"));
  Time interPacketInterval = Seconds (interval);
  NodeContainer c;
  c.Create (numVehicles);



  Ns2MobilityHelper ns2 = Ns2MobilityHelper (traceFile);

  ns2.Install();
  LogComponentEnable ("Ns2MobilityHelper",LOG_LEVEL_DEBUG);
  os.open (logFile.c_str ());
  Config::Connect ("/NodeList/*/$ns3::MobilityModel/CourseChange",
                   MakeBoundCallback (&CourseChange, &os));

  YansWifiChannelHelper wifiChannel;
  wifiChannel.SetPropagationDelay("ns3::ConstantSpeedPropagationDelayModel");
  /*wifiChannel.AddPropagationLoss("ns3::TwoRayGroundPropagationLossModel",
    "Frequency", DoubleValue(5886362811.70233654),"MinDistance", DoubleValue(300));*/
  wifiChannel.AddPropagationLoss("ns3::ThreeLogDistancePropagationLossModel");
  wifiChannel.AddPropagationLoss ("ns3::NakagamiPropagationLossModel", "m0",
    DoubleValue(1.5), "m1", DoubleValue(0.75), "Distance1", DoubleValue(80),
    "m2", DoubleValue(0), "Distance2", DoubleValue(320));
  Ptr<YansWifiChannel> channel = wifiChannel.Create ();
  YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default ();
  wifiPhy.SetChannel (channel);
  wifiPhy.SetPcapDataLinkType (YansWifiPhyHelper::DLT_IEEE802_11);
  wifiPhy.Set("TxPowerStart", DoubleValue(20));
  wifiPhy.Set("TxPowerEnd", DoubleValue(20));
  wifiPhy.Set("TxPowerLevels", UintegerValue(1));
  wifiPhy.Set("TxGain", DoubleValue(1));
  NqosWaveMacHelper wifi80211pMac = NqosWaveMacHelper::Default ();
  Wifi80211pHelper wifi80211p = Wifi80211pHelper::Default ();
  if (verbose)
  {
    wifi80211p.EnableLogComponents (); // Turn on all Wifi 802.11p logging
  }
  wifi80211p.SetRemoteStationManager ("ns3::ConstantRateWifiManager",
    "DataMode",StringValue (phyMode),
    "ControlMode",StringValue (phyMode),
    "NonUnicastMode", StringValue (phyMode));
  NetDeviceContainer devices = wifi80211p.Install (wifiPhy, wifi80211pMac, c);
  wifiPhy.EnablePcap ("measurements/platooning", devices, true);



  //nckernel test
  struct nck_encoder enc[numVehicles];
  struct nck_recoder rec[numVehicles];
  char ChrpacketSize[10] = "";
  sprintf(ChrpacketSize, "%d", packetSize);
  char Chrnode_id[10] = "";
  char ChrnumVehicles[10] = "";
  char Chrsymbols[10] = "";
  sprintf(ChrnumVehicles, "%d", numVehicles);



  InternetStackHelper internet;
  internet.Install (c);
  Ipv4AddressHelper ipv4;
  NS_LOG_INFO ("Assign IP Addresses.");
  ipv4.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer i = ipv4.Assign (devices);
  Ptr<Socket> socket[numVehicles];
  struct GenTrafficValues svalues[numVehicles];
  struct ReceiveValues rvalues[numVehicles];
  std::deque<memory_packet> packet_memory[numVehicles];
  std::deque<coded_memory_packet> coded_memory[numVehicles];

  uint32_t symbols = 1;
  for (int p = 0; symbols < numVehicles; p++ )
    symbols = pow(2,p);
  sprintf(Chrsymbols, "%d", symbols);

  for (int s = 0; s < (int) numVehicles; s++)
  {
    char nodestring[100];
    sprintf(nodestring,"_node_%d.log", s);
    std::string recv_file;
    recv_file.append(recvLogName);
    std::string send_file;
    send_file.append(sendLogName);
    std::string fwd_file;
    fwd_file.append(fwdLogName);
    recv_file.append(nodestring);
    send_file.append(nodestring);
    fwd_file.append(nodestring);

    recvLog[s].open (recv_file.c_str ());
    sendLog[s].open (send_file.c_str ());
    fwdLog[s].open (fwd_file.c_str ());

    sprintf(Chrnode_id, "%d", s);
    //configuration values for the encoder
    struct nck_option_value options_enc[] =
    {
      { "protocol", "interflow_sw" },
      { "feedback", "0" },
      { "symbol_size", (const char*) ChrpacketSize },
      { "symbols", Chrsymbols},
      { "redundancy", "0" },
      { "systematic", "1"},
      { "coded", "0"},
      { "node_id", (const char*) Chrnode_id},
      { "n_nodes", (const char*) ChrnumVehicles},
      { NULL, NULL }
    };
    struct nck_option_value options_rec[] =
    {
      { "protocol", "interflow_sw" },
      { "feedback", "0" },
      { "symbol_size", (const char*) ChrpacketSize },
      { "symbols", Chrsymbols},
      { "systematic", "0"},
      { "forward_code_window", (const char*) ChrnumVehicles},
      { "redundancy", "0"},
      { NULL, NULL }
    };
    // initialize the encoder according to the configuration
    if (nck_create_encoder(&enc[s], NULL, options_enc, nck_option_from_array))
    {
      fprintf(stderr, "Failed to create encoder");
      return -1;
    }
    // initialize the recoder according to the configuration
    if (nck_create_recoder(&rec[s], NULL, options_rec, nck_option_from_array))
    {
      fprintf(stderr, "Failed to create recoder");
      return -1;
    }
    rvalues[s].node_id = (uint32_t) s;
    rvalues[s].n_vehicles = numVehicles;
    rvalues[s].rec = &rec[s];
    rvalues[s].maxTransmissionDelay = maxTransmissionDelay;
    rvalues[s].maxSourceLoss = maxPacketLoss;
    rvalues[s].recvLog = &recvLog[s];
    rvalues[s].fwdLog = &fwdLog[s];
    rvalues[s].fwdAll = forwardAll;
    rvalues[s].networkCoding = networkCoding;
    rvalues[s].noForward = noForward;
    rvalues[s].packet_interval = (uint32_t) (interval * 1000000);
    *rvalues[s].recvLog << "Node: " << s << " Rate: " << phyMode
    << " Number of vehicles: " << numVehicles << " Packet length: "
    << packetSize << " maximum Delay: " << maxTransmissionDelay
    << " maximum Loss: " << maxPacketLoss << " Packet interval: "
    << interval << std::endl;

    *rvalues[s].fwdLog << "Node: " << s << " Rate: " << phyMode
    << " Number of vehicles: " << numVehicles << " Packet length: "
    << packetSize << " maximum Delay: " << maxTransmissionDelay
    << " maximum Loss: " << maxPacketLoss << " Packet interval: "
    << interval << std::endl;

    *rvalues[s].recvLog << "Source ID, Sequence Number, Creation Time, Received Time, Packet Delivery Delay, Decoding Delay, Total Delay, Packet Length, Packet data" <<std::endl;
    *rvalues[s].fwdLog << "Node ID, Creation Time, Forwarding Reason, Forwarding Delay, Recoding Delay, Packet data" <<std::endl;


    TypeId tid = TypeId::LookupByName ("ns3::UdpSocketFactory");
    socket[s] = Socket::CreateSocket (c.Get (s), tid);
    InetSocketAddress local = InetSocketAddress (Ipv4Address::GetAny (), 80);
    socket[s]->Bind (local);
    socket[s]->SetRecvCallback (MakeBoundCallback (&ReceivePacket, &rvalues[s], &packet_memory[s], &coded_memory[s]));
    InetSocketAddress remote = InetSocketAddress (Ipv4Address ("255.255.255.255"), 80);
    socket[s]->SetAllowBroadcast (true);
    socket[s]->Connect (remote);

    svalues[s].node_id = (uint32_t) socket[s]->GetNode ()->GetId ();
    svalues[s].n_vehicles = numVehicles;
    svalues[s].enc = &enc[s];
    svalues[s].rec = &rec[s];
    svalues[s].sendLog = &sendLog[s];
    svalues[s].networkCoding = networkCoding;
    *svalues[s].sendLog << "Node: " << s << " Rate: " << phyMode
    << " Number of vehicles: " << numVehicles << " Packet length: "
    << packetSize << " maximum Delay: " << maxTransmissionDelay
    << " maximum Loss: " << maxPacketLoss << " Packet interval: "
    << interval << std::endl;

    *svalues[s].sendLog << "Source ID, Sequence Number, Creation Time, Encoding Delay, Packet Length, Packet data" <<std::endl;

    double sched_time = 200.0 + ((rand() % 10000)/1000000.0);
    Simulator::ScheduleWithContext (socket[s]->GetNode ()->GetId (),
    Seconds (sched_time), &GenerateTraffic,
    socket[s], packetSize, numPackets, interPacketInterval, 0, svalues[s]);

  }

  AsciiTraceHelper ascii;
  wifiPhy.EnableAsciiAll (ascii.CreateFileStream("measurements/platooning.tr"));
    MobilityHelper::EnableAsciiAll (ascii.CreateFileStream ("measurements/mobility-trace-example.mob"));
  Simulator::Run ();
  usleep(1000000);
  Simulator::Destroy ();
  os.close();
  for (uint32_t r = 0; r < numVehicles; r++)
  {
      socket[r]->Close();
      sendLog[r].close();
      recvLog[r].close();
      fwdLog[r].close();
      nck_free(&enc[r]);
      nck_free(&rec[r]);
  }
  os.close ();
  return 0;
}
