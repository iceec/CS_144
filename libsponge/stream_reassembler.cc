#include "stream_reassembler.hh"

#include <algorithm>
#include <assert.h>
// Dummy implementation of a stream reassembler.

// For Lab 1, please replace with a real implementation that passes the
// automated checks run by `make check_lab1`.

// You will need to add private members to the class declaration in `stream_reassembler.hh`

template <typename... Targs>
void DUMMY_CODE(Targs &&.../* unused */) {}

using namespace std;

StreamReassembler::StreamReassembler(const size_t capacity)
    : _output(capacity), _capacity(capacity), _reassembler(capacity + 1), _elem_status(capacity + 1, false) {}

//! \details This function accepts a substring (aka a segment) of bytes,
//! possibly out-of-order, from the logical stream, and assembles any newly
//! contiguous substrings and writes them into the output stream in order.
void StreamReassembler::push_substring(const string &data, const size_t index, const bool eof) {
    // 确定 序列号的接受范围
    size_t data_head = index, data_tail = index + data.size();

    size_t accept_min_index = _next_assembler_index;
    size_t accept_max_index = _next_assembler_index + _capacity;

    if (_eof)
        accept_max_index = std::min(accept_max_index, _eof_index);


    // 判断是否丢弃这个包
    if (data.size() == 0 && (data_head < accept_min_index || data_head >= accept_max_index))
        return;
    else if (data.size() > 0 && (data_head >= accept_max_index || data_tail <= accept_min_index))  // data()  || ()data
        return;

    // fin了吗
    if (eof && (!_eof)) {
        _eof = true;
        _eof_index = index + data.size();
    }

    // 确定数据的起始位置
    size_t offset = (index >= _next_assembler_index) ? 0 : _next_assembler_index - index;

    size_t data_index = (index >= _next_assembler_index) ? index : _next_assembler_index;

    size_t n = data.size(), tmp_index;

    for (size_t i = offset; i < n && data_index < accept_max_index; ++i, ++data_index) {

        tmp_index = data_index % _capacity;

        if (_elem_status[tmp_index])
            continue;

        ++_unassembled_bytes;
        _elem_status[tmp_index] = true;
        _reassembler[tmp_index] = data[i];
    }

    // 向内核缓冲区 写入
    write_into_output();
}

size_t StreamReassembler::unassembled_bytes() const { return _unassembled_bytes; }

bool StreamReassembler::empty() const { return _unassembled_bytes == 0; }

void StreamReassembler::write_into_output(void) {
    string to_byte_stream;
    to_byte_stream.reserve(1460);

    size_t index = _next_assembler_index;
    size_t accept_max_index = _next_assembler_index + _capacity;

    if (_eof)
        accept_max_index = std::min(accept_max_index, _eof_index);

    for (; index < accept_max_index; ++index) {
        if (_elem_status[index % _capacity] == false)
            break;

        --_unassembled_bytes;
        to_byte_stream.push_back(_reassembler[index % _capacity]);
        _elem_status[index % _capacity] = false;
    }

    size_t wirte_size = index - _next_assembler_index;
    size_t write_len = _output.write(to_byte_stream);
    size_t recover_index;

    for (size_t i = write_len; i < wirte_size; ++i) {
        recover_index = (_next_assembler_index + i) % _capacity;
        assert(_elem_status[recover_index] == false);
        _elem_status[recover_index] = true;
        ++_unassembled_bytes;
    }

    _next_assembler_index += write_len;

    if (_eof && _eof_index == _next_assembler_index)
        _output.end_input();
}
