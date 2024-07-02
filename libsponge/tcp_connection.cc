#include "tcp_connection.hh"

#include <assert.h>
#include <iostream>

// Dummy implementation of a TCP connection

// For Lab 4, please replace with a real implementation that passes the
// automated checks run by `make check`.

template <typename... Targs>
void DUMMY_CODE(Targs &&.../* unused */) {}

using namespace std;

size_t TCPConnection::remaining_outbound_capacity() const { return _sender.stream_in().remaining_capacity(); }

size_t TCPConnection::bytes_in_flight() const { return _sender.bytes_in_flight(); }

size_t TCPConnection::unassembled_bytes() const { return _receiver.unassembled_bytes(); }

size_t TCPConnection::time_since_last_segment_received() const { return _time_since_last_segment_received; }

void TCPConnection::segment_received(const TCPSegment &seg) {
    if (_active == false)
        return;

    // 接收到package 一定要给receiver  更新 ackno 以为确定 windowsize
    _receiver.segment_received(seg);

    _time_since_last_segment_received = 0;

    // if 收到了 rst 直接关闭连接
    if (seg.header().rst)
    {
        handle_RST(false);
        return ;
    }

      // if 收到了ack  交给_sender
    if (seg.header().ack) {
        // 会进行fill window
        _sender.ack_received(seg.header().ackno, seg.header().win);
        // 有ack 非空 但是无效的 这个时候要加上一个空的包  to do
    }


    string recviver_status = TCPState::state_summary(_receiver);
    string sender_status = TCPState::state_summary(_sender);

    // recv syn  close ->  recv syn synsent 因为上一步的if 不会进入
    // 因为if 来到这一步的话 本身就是没有ack的 所以不耽误
    if (recviver_status == TCPReceiverStateSummary::SYN_RECV && sender_status == TCPSenderStateSummary::CLOSED)
    {
        connect();
        return;
    }

        

    // 先收到了 fin  那么我就不需要time_wait 了
    // 这一步 不能先接受 if 先接受的话 那么
    if (recviver_status == TCPReceiverStateSummary::FIN_RECV && sender_status == TCPSenderStateSummary::SYN_ACKED) {
        _linger_after_streams_finish = false;
    }

  

   // sender_status = TCPState::state_summary(_sender);

    // 这个时候是要接受完sender才可以判断的
    if (recviver_status == TCPReceiverStateSummary::FIN_RECV &&
     sender_status == TCPSenderStateSummary::FIN_ACKED &&
        _linger_after_streams_finish == false) {
        _active = false;
    }

    if (seg.length_in_sequence_space() != 0 && _sender.segments_out().size() == 0)
            _sender.send_empty_segment();

    send_TCPsegmet();
}

bool TCPConnection::active() const { return _active; }

size_t TCPConnection::write(const string &data) {
    if (_active == false)
        return 0;
    size_t bytes_written = _sender.stream_in().write(data);
    _sender.fill_window();
    send_TCPsegmet();
    return bytes_written;
}

//! \param[in] ms_since_last_tick number of milliseconds since the last call to this method
void TCPConnection::tick(const size_t ms_since_last_tick) {
    _time_since_last_segment_received += ms_since_last_tick;

    _sender.tick(ms_since_last_tick);

    if (_sender.consecutive_retransmissions() > _cfg.MAX_RETX_ATTEMPTS) {
        handle_RST(true);
        return;
    }

    send_TCPsegmet();

    if (TCPState::state_summary(_receiver) == TCPReceiverStateSummary::FIN_RECV &&
        TCPState::state_summary(_sender) == TCPSenderStateSummary::FIN_ACKED && _linger_after_streams_finish &&
        _time_since_last_segment_received >= 10 * _cfg.rt_timeout) {
        _active = false;
        _linger_after_streams_finish = false;
    }
}

void TCPConnection::end_input_stream() {
    if(_active ==false)
        return ;
    _sender.stream_in().end_input();

    _sender.fill_window();

    send_TCPsegmet();
}

void TCPConnection::connect() {
    if (_active == false)
        return;
    assert(_sender.segments_out().empty());
        _sender.fill_window();
    
    send_TCPsegmet();
}

TCPConnection::~TCPConnection() {
    try {
        if (active()) {
            cerr << "Warning: Unclean shutdown of TCPConnection\n";
            // Your code here: need to send a RST segment to the peer
            _active = false;
            handle_RST(true);
        }
    } catch (const exception &e) {
        std::cerr << "Exception destructing TCP FSM: " << e.what() << std::endl;
    }
}

void TCPConnection::send_TCPsegmet() {
    while (_sender.segments_out().size()) {
        auto ackno = _receiver.ackno();

        TCPSegment & package = _sender.segments_out().front();
         _sender.segments_out().pop();
        if (ackno.has_value()) {
            package.header().ack = true;
            package.header().ackno = ackno.value();
            package.header().win = _receiver.window_size();
        }
        _segments_out.push(package);
       
    }
}

void TCPConnection::handle_RST(bool send_to_peer) {
    if (send_to_peer) {
        if (_sender.segments_out().size() == 0)
            _sender.send_empty_segment();

        TCPSegment & package = _sender.segments_out().front();

        package.header().rst = true;
        _segments_out.push(package);
        _sender.segments_out().pop();
    }
    _active = false;
    _linger_after_streams_finish =false;
    _sender.stream_in().set_error();
    _receiver.stream_out().set_error();
}
