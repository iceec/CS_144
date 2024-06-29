#include "byte_stream.hh"
#include<algorithm>
// Dummy implementation of a flow-controlled in-memory byte stream.

// For Lab 0, please replace with a real implementation that passes the
// automated checks run by `make check_lab0`.

// You will need to add private members to the class declaration in `byte_stream.hh`

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

ByteStream::ByteStream(const size_t capacity):_stream(capacity+1),_capacity(capacity) { DUMMY_CODE(capacity); }

size_t ByteStream::write(const string &data) {
    size_t count,n =data.size();
 
    for( count=0 ;count<n&&_size<_capacity;++count,++_size)
    {
        _stream[_tail_index] = data[count];

        _tail_index = (_tail_index +1) % _capacity;
    }

    _bytes_written +=count;

    return count;
}

//! \param[in] len bytes will be copied from the output side of the buffer
string ByteStream::peek_output(const size_t len) const {
    size_t string_size = std::min(len,_size);

    string out(string_size,0);
    size_t index = _head_index;
    


    for(size_t i =0;i<string_size;++i)
    {
        out[i] = _stream[index];

        index = (index+1)%_capacity;
    }

    return out;

}

//! \param[in] len bytes will be removed from the output side of the buffer
void ByteStream::pop_output(const size_t len) { 

    size_t pop_size = std::min(len,_size);

    _size -= pop_size;
    _bytes_read +=pop_size;

    _head_index = (_head_index +pop_size)%_capacity;
 }

//! Read (i.e., copy and then pop) the next "len" bytes of the stream
//! \param[in] len bytes will be popped and returned
//! \returns a string
std::string ByteStream::read(const size_t len) {
    string out = peek_output(len);
    pop_output(len);

    
    return out;
}

void ByteStream::end_input() {_eof = true;}

bool ByteStream::input_ended() const { return _eof; }

size_t ByteStream::buffer_size() const { return _size; }

bool ByteStream::buffer_empty() const { return _size ==0; }

bool ByteStream::eof() const { return _eof && _size==0; }

size_t ByteStream::bytes_written() const { return _bytes_written; }

size_t ByteStream::bytes_read() const { return _bytes_read; }

size_t ByteStream::remaining_capacity() const { return _capacity-_size; }
