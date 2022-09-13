#include "stream_reassembler.hh"

// Dummy implementation of a stream reassembler.

// For Lab 1, please replace with a real implementation that passes the
// automated checks run by `make check_lab1`.

// You will need to add private members to the class declaration in `stream_reassembler.hh`

template <typename... Targs>
void DUMMY_CODE(Targs &&.../* unused */) {}

using namespace std;

StreamReassembler::StreamReassembler(const size_t capacity)
    : _output(capacity)
    , _capacity(capacity)
    , _eof(false)
    , unass(0)
    , buf(capacity, '\0')
    , mask(capacity, false)
    , size(0) {}

//! \details This function accepts a substring (aka a segment) of bytes,
//! possibly out-of-order, from the logical stream, and assembles any newly
//! contiguous substrings and writes them into the output stream in order.
void StreamReassembler::push_substring(const string &data, const size_t index, const bool eof) {
    if (eof) {
        _eof = true;
    }
    size_t len = data.size();
    /*if (len == 0 && _eof && size == 0) {
        _output.end_input();
        return;
    }*/
    if (index > unass) {
        /*index in unacceptable*/
        if (index - unass > _capacity - _output.buffer_size()) {
            return;
        }

        /*end of data can be in unacceptable*/
        size_t remain = _capacity - _output.buffer_size() - index + unass;
        size_t real_len = len < remain ? len : remain;
        if (real_len < len) {
            _eof = false;
        }
        for (size_t i = 0; i < real_len; i++) {
            if (mask[index - unass + i])
                continue;
            buf[index - unass + i] = data[i];
            mask[index - unass + i] = true;
            size++;
        }
    } else if (index + len > unass) {
        /*only deal with unassembled data;end of data can be in unacceptable*/
        size_t real_len = index + len - unass < _capacity - _output.buffer_size() ? index + len - unass
                                                                                  : _capacity - _output.buffer_size();
        if (real_len < index + len - unass) {
            _eof = false;
        }
        for (size_t i = 0; i < real_len; i++) {
            if (mask[i])
                continue;
            buf[i] = data[i + unass - index];
            mask[i] = true;
            size++;
        }
    }
    string temp = "";
    while (mask.front()) {
        temp += buf.front();
        buf.pop_front();
        mask.pop_front();
        buf.push_back('\0');
        mask.push_back(false);
        unass++;
        size--;
    }
    if (temp.length() > 0)
        _output.write(temp);
    if (_eof && size == 0) {
        _output.end_input();
        return;
    }
}
size_t StreamReassembler::unassembled_bytes() const { return size; }

bool StreamReassembler::empty() const { return size == 0; }
