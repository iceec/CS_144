#include "tcp_receiver.hh"

// Dummy implementation of a TCP receiver

// For Lab 2, please replace with a real implementation that passes the
// automated checks run by `make check_lab2`.

template <typename... Targs>
void DUMMY_CODE(Targs &&.../* unused */) {}

using namespace std;

void TCPReceiver::segment_received(const TCPSegment &seg) {
    if (seg.header().syn && !_recv_syn)
    {
        _recv_syn = true;
        _isn = seg.header().seqno;
    }
        

    if (_recv_syn == false)
        return;

    

    uint64_t ab_index = unwrap(seg.header().seqno,_isn,_checkpoint);

    ab_index = (seg.header().syn) ? ab_index : ab_index -1;

    _reassembler.push_substring(seg.payload().copy(),ab_index,seg.header().fin);

    _checkpoint = _reassembler.get_next_assembler_index() + 1;
    
   

}

optional<WrappingInt32> TCPReceiver::ackno() const 
{
    if(_recv_syn == false)
        return {};

    uint64_t ack_ab_index = _checkpoint;

    if(_reassembler.eof()&&(_reassembler.get_next_assembler_index() == _reassembler.eof_index()))
        ack_ab_index +=1;
    

    return WrappingInt32(wrap(ack_ab_index,_isn));


}

size_t TCPReceiver::window_size() const { 
    return _capacity - _reassembler.stream_out().buffer_size();
}
