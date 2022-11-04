#include "tcp_connection.hh"

#include <iostream>

// Dummy implementation of a TCP connection

// For Lab 4, please replace with a real implementation that passes the
// automated checks run by `make check`.

template <typename... Targs>
void DUMMY_CODE(Targs &&.../* unused */) {}

using namespace std;

size_t TCPConnection::remaining_outbound_capacity() const { return _sender._stream.remaining_capacity(); }

size_t TCPConnection::bytes_in_flight() const { return _sender.bytes_in_flight(); }

size_t TCPConnection::unassembled_bytes() const { return _receiver.unassembled_bytes(); }

size_t TCPConnection::time_since_last_segment_received() const { return {}; }

void TCPConnection::segment_received(const TCPSegment &seg) {
    if (seg.header().rst == true) {
        _sender._stream.set_error();
        _receiver.stream_out.set_error();
        activ=false;
        ~TCPConnection();
    }
    _receiver.segment_received(seg);
    if (seg.header().ack == true) {
        _sender.ack_received(seg.header().ackno, seg.header().win);
    }
    if (seg.length_in_sequence_space() > 0) {
        has_ack=true;
        _sender.send_empty_segment();
        TCPSegment reply = _sender.segments_out().front();
        reply.header().ackno = _receiver.ackno();
        reply.header().win = _receiver.window_size();
        segments_out.push(reply);
    }
    if (_receiver.ackno().has_value() and (seg.length_in_sequence_space() == 0) and
        seg.header().seqno == _receiver.ackno().value() - 1) {
        _sender.send_empty_segment();
    }
}

bool TCPConnection::active() const { return activ; }

size_t TCPConnection::write(const string &data) {
    size_t size = _sender.stream_in().write(data);
    _sender.fill_window();
    while (!_sender.segments_out().empty()) {
        TCPSegment seg;
        if(has_ack){
            has_ack=false;
        }
        segments_out.push(seg);
    }
    return size;
}

//! \param[in] ms_since_last_tick number of milliseconds since the last call to this method
void TCPConnection::tick(const size_t ms_since_last_tick) {
    _sender.tick(ms_since_last_tick);
    if (_sender.consecutive_retransmissions() > TCPConfig::MAX_RETX_ATTEMPTS) {
        ~TCPConnection();
    }
    // end the connection cleanly if necessary
}

void TCPConnection::end_input_stream() { _sender._stream.end_input(); }

void TCPConnection::connect() {
    _sender.fill_window();
    while (!_sender.segments_out().empty()) {
        TCPSegment seg;
        segments_out.push(seg);
    }
}

TCPConnection::~TCPConnection() {
    try {
        if (active()) {
            cerr << "Warning: Unclean shutdown of TCPConnection\n";
            _sender.send_empty_segment();
            TCPSegment seg = _sender.segments_out().front();
            seg.header().rst=true;
            segments_out.push(seg);
            // Your code here: need to send a RST segment to the peer
        }
    } catch (const exception &e) {
        std::cerr << "Exception destructing TCP FSM: " << e.what() << std::endl;
    }
}
