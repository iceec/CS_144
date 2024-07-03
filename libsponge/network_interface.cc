#include "network_interface.hh"

#include "arp_message.hh"
#include "ethernet_frame.hh"

#include <iostream>

// Dummy implementation of a network interface
// Translates from {IP datagram, next hop address} to link-layer frame, and from link-layer frame to IP datagram

// For Lab 5, please replace with a real implementation that passes the
// automated checks run by `make check_lab5`.

// You will need to add private members to the class declaration in `network_interface.hh`

template <typename... Targs>
void DUMMY_CODE(Targs &&.../* unused */) {}

using namespace std;

//! \param[in] ethernet_address Ethernet (what ARP calls "hardware") address of the interface
//! \param[in] ip_address IP (what ARP calls "protocol") address of the interface
NetworkInterface::NetworkInterface(const EthernetAddress &ethernet_address, const Address &ip_address)
    : _ethernet_address(ethernet_address), _ip_address(ip_address) {
    cerr << "DEBUG: Network interface has Ethernet address " << to_string(_ethernet_address) << " and IP address "
         << ip_address.ip() << "\n";
}

//! \param[in] dgram the IPv4 datagram to be sent
//! \param[in] next_hop the IP address of the interface to send it to (typically a router or default gateway, but may also be another host if directly connected to the same network as the destination)
//! (Note: the Address type can be converted to a uint32_t (raw 32-bit IP address) with the Address::ipv4_numeric() method.)
void NetworkInterface::send_datagram(const InternetDatagram &dgram, const Address &next_hop) {
    // convert IP address of next hop to raw 32-bit representation (used in ARP header)
    const uint32_t next_hop_ip = next_hop.ipv4_numeric();

    auto match_next_hop = _Next_Hop_map.find(next_hop_ip);
    auto already_send_arp = _Already_ARP_Ip_set.find(next_hop_ip);

    EthernetFrame frame;
    if (match_next_hop != _Next_Hop_map.end()) {
        frame.header().src = _ethernet_address;
        frame.header().dst = match_next_hop->second.first;
        frame.header().type = EthernetHeader::TYPE_IPv4;
        frame.payload() = dgram.serialize();
        _frames_out.push(frame);

    } else {
        if (already_send_arp == _Already_ARP_Ip_set.end()) {
            ARPMessage arpmessage;
            arpmessage.opcode = ARPMessage::OPCODE_REQUEST;
            arpmessage.sender_ethernet_address = _ethernet_address;
            arpmessage.sender_ip_address = _ip_address.ipv4_numeric();
            arpmessage.target_ethernet_address = {};
            arpmessage.target_ip_address = next_hop_ip;

            frame.header().src = _ethernet_address;
            frame.header().dst = ETHERNET_BROADCAST;
            frame.header().type = EthernetHeader::TYPE_ARP;
            frame.payload() = arpmessage.serialize();

            // 发送ARP报文
            _frames_out.push(frame);

            // 通知已经发送了 ARP报文
            _Already_ARP_Ip_set.insert({next_hop_ip, _DEFALULT_ARP_WAIT});
        }
        // 添加到等待队列当中
        _Wait_to_send_out.push_back({next_hop_ip, dgram});
    }
}

//! \param[in] frame the incoming Ethernet frame
optional<InternetDatagram> NetworkInterface::recv_frame(const EthernetFrame &frame) {
    if (frame.header().dst != ETHERNET_BROADCAST &&(frame.header().dst != _ethernet_address))
        return {};

    if (frame.header().type == EthernetHeader::TYPE_IPv4) {
        InternetDatagram datagram;

        if (datagram.parse(frame.payload()) != ParseResult::NoError)
            return {};

        return datagram;
    } else if (frame.header().type == EthernetHeader ::TYPE_ARP) {
        ARPMessage arpmessage;

        if (arpmessage.parse(frame.payload()) != ParseResult::NoError)
            return {};

        bool is_request = false, is_reply = false;

        is_request = arpmessage.opcode == ARPMessage::OPCODE_REQUEST &&
                     arpmessage.target_ip_address == _ip_address.ipv4_numeric();

        is_reply = arpmessage.opcode == ARPMessage::OPCODE_REPLY &&
                   arpmessage.target_ethernet_address == _ethernet_address ;

        if (is_request) {
            ARPMessage request_arpmessage;

            request_arpmessage.opcode = ARPMessage::OPCODE_REPLY;
            request_arpmessage.sender_ethernet_address = _ethernet_address;
            request_arpmessage.sender_ip_address = _ip_address.ipv4_numeric();
            request_arpmessage.target_ethernet_address = arpmessage.sender_ethernet_address;
            request_arpmessage.target_ip_address = arpmessage.sender_ip_address;

            EthernetFrame reply_frame;
            reply_frame.header().src = _ethernet_address;
            reply_frame.header().dst = arpmessage.sender_ethernet_address;
            reply_frame.header().type = EthernetHeader::TYPE_ARP;
            reply_frame.payload() = request_arpmessage.serialize();
            _frames_out.push(reply_frame);
        }

        if (is_reply || is_request) {
            uint32_t target_ip = arpmessage.sender_ip_address;
            EthernetAddress target_etheraddress = arpmessage.sender_ethernet_address;

            _Next_Hop_map[target_ip] = {target_etheraddress, _DEFALULT_HOP_MAP};
            _Already_ARP_Ip_set.erase(target_ip);

            // wait 队列清除
            for (auto wait_package = _Wait_to_send_out.begin(); wait_package != _Wait_to_send_out.end();) {
                // 发送给下一个节点
                if (wait_package->first == target_ip) {
                    EthernetFrame ip_frame;
                    ip_frame.header().src = _ethernet_address;
                    ip_frame.header().dst = _Next_Hop_map[target_ip].first;
                    ip_frame.header().type = EthernetHeader::TYPE_IPv4;
                    ip_frame.payload() = wait_package->second.serialize();
                    _frames_out.push(ip_frame);
                    wait_package = _Wait_to_send_out.erase(wait_package);
                } else
                    ++wait_package;
            }
        }
    }

    return {};
}

//! \param[in] ms_since_last_tick the number of milliseconds since the last call to this method
void NetworkInterface::tick(const size_t ms_since_last_tick) {
    for (auto i = _Next_Hop_map.begin(); i != _Next_Hop_map.end(); ) {
        if (i->second.second > ms_since_last_tick) {
            i->second.second -= ms_since_last_tick;
            ++i;
        } else {
            i = _Next_Hop_map.erase(i);
        }
    }

    for (auto i = _Already_ARP_Ip_set.begin(); i != _Already_ARP_Ip_set.end();) {
        if (i->second > ms_since_last_tick) {
            i->second -= ms_since_last_tick;
            ++i;
        } else {
            uint32_t target_ip = i->first;

            i->second = _DEFALULT_ARP_WAIT;

            ARPMessage arpmessage;
            arpmessage.opcode = ARPMessage::OPCODE_REQUEST;

            arpmessage.sender_ethernet_address = _ethernet_address;
            arpmessage.sender_ip_address = _ip_address.ipv4_numeric();

            arpmessage.target_ethernet_address = {};
            arpmessage.target_ip_address = target_ip;

            EthernetFrame frame;
            frame.header().src = _ethernet_address;
            frame.header().dst = ETHERNET_BROADCAST;
            frame.header().type = EthernetHeader::TYPE_ARP;
            frame.payload() = arpmessage.serialize();

            // 发送ARP报文
            _frames_out.push(frame);
        }
    }
}
