#include "tcp_sender.hh"

#include "tcp_config.hh"

#include <algorithm>
#include <assert.h>
#include <iostream>
#include <random>

// Dummy implementation of a TCP sender

// For Lab 3, please replace with a real implementation that passes the
// automated checks run by `make check_lab3`.

template <typename... Targs>
void DUMMY_CODE(Targs &&.../* unused */) {}

using namespace std;

//! \param[in] capacity the capacity of the outgoing byte stream
//! \param[in] retx_timeout the initial amount of time to wait before retransmitting the oldest outstanding segment
//! \param[in] fixed_isn the Initial Sequence Number to use, if set (otherwise uses a random ISN)
TCPSender::TCPSender(const size_t capacity, const uint16_t retx_timeout, const std::optional<WrappingInt32> fixed_isn)
    : _isn(fixed_isn.value_or(WrappingInt32{random_device()()}))
    , _initial_retransmission_timeout{retx_timeout}
    , _stream(capacity)
    , _timer(_initial_retransmission_timeout) {}

uint64_t TCPSender::bytes_in_flight() const { return _next_seqno - _ack_ab_seqno; }

void TCPSender::fill_window() {
    // send syn
    if (_next_seqno == 0) {
        TCPSegment package;
        package.header().syn = true;
        package.header().seqno = _isn;
        _segments_out.push(package);
        _retransmission_queue.push(package);
        _next_seqno += package.length_in_sequence_space();
        return;
    }

    uint16_t window_size = _window_size == 0 ? 1 : _window_size;

    uint64_t _max_send_seqno = _ack_ab_seqno + window_size;

    while (_next_seqno < _max_send_seqno && (!_stream.buffer_empty() || _stream.eof()) && _send_fin == false) {
        uint64_t seq_legth = _max_send_seqno - _next_seqno;

        uint64_t send_legth = std::min(seq_legth, TCPConfig::MAX_PAYLOAD_SIZE);

        string payload = _stream.read(send_legth);

        TCPSegment package;

        package.header().seqno = wrap(_next_seqno, _isn);

        package.payload() = Buffer(std::move(payload));

        _next_seqno += package.length_in_sequence_space();

        if (_stream.eof() && _next_seqno < _max_send_seqno) {
            _next_seqno += 1;
            package.header().fin = true;
            _send_fin = true;
        }

        // std::cout<<package.payload().str()<<endl;

        _segments_out.push(package);
        _retransmission_queue.push(package);
    }

    if (_retransmission_queue.size())
        _timer._on = true;
}

//! \param ackno The remote receiver's ackno (acknowledgment number)
//! \param window_size The remote receiver's advertised window size
void TCPSender::ack_received(const WrappingInt32 ackno, const uint16_t window_size) {
    uint64_t receiver_ack_ab_seqno = unwrap(ackno, _isn, _ack_ab_seqno);

    if (receiver_ack_ab_seqno > _next_seqno || receiver_ack_ab_seqno < _ack_ab_seqno)
        return;

    _ack_ab_seqno = receiver_ack_ab_seqno;

    _window_size = window_size;

    size_t before_retrans_size = _retransmission_queue.size();

    uint64_t front_ab_seqno;
    
    while (_retransmission_queue.size()) {

        front_ab_seqno = unwrap(_retransmission_queue.front().header().seqno,_isn,_ack_ab_seqno);

        if(front_ab_seqno + _retransmission_queue.front().length_in_sequence_space() > _ack_ab_seqno)
            break;
        
        _retransmission_queue.pop();
    }

    if (before_retrans_size > _retransmission_queue.size()) {
        _timer._retx_times = 0;
        _timer._now_time = 0;
        _timer._retx_timeout = _initial_retransmission_timeout;
        _timer._on = false;
    }

    fill_window();
}

//! \param[in] ms_since_last_tick the number of milliseconds since the last call to this method
void TCPSender::tick(const size_t ms_since_last_tick) {
    if (_timer._on == false)
        return;

    assert(_retransmission_queue.size());

    _timer._now_time += ms_since_last_tick;

    if (_timer._now_time >= _timer._retx_timeout) {
        TCPSegment tmp =_retransmission_queue.front();
        _segments_out.push(tmp);
        if (_window_size) {
            size_t double_timeout = _timer._retx_timeout * 2;

            if (_timer._retx_timeout < double_timeout)
                _timer._retx_timeout = double_timeout;

            _timer._retx_times += 1;
        }

        _timer._now_time = 0;
    }
}

unsigned int TCPSender::consecutive_retransmissions() const { return _timer._retx_times; }

// 不用重传
void TCPSender::send_empty_segment() {
    TCPSegment package;
    package.header().seqno = wrap(_next_seqno, _isn);
    _segments_out.push(package);
    return;
}
