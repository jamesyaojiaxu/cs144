#include "tcp_sender.hh"

#include "tcp_config.hh"

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
    , _syn(true)
    ,checkpoint(0) {}

uint64_t TCPSender::bytes_in_flight() const { return {}; }

void TCPSender::fill_window() {
    if (_syn) {
        TCPSegment seg;
        seg.header().seqno = _isn;
        seg.header().syn = true;
        _syn = false;
        _segments_out.push(seg);
        return;
    }
    // normal segment
    TCPSegment seg;
    int read_count;
    while (window_size != (_next_seqno-ackno) && !_fin) {
        seg.header().seqno = wrap(_next_seqno,_isn);
        if(_stream.input_ended() && _stream.buffer_empty()){
            seg.header().fin = true;
            _fin = true;
        }
        send_segment(min(window_size -(_next_seqno-ackno),TCPConfig::MAX_PAYLOAD_SIZE),seg, _next_seqno);
    
    }
}

//! \param ackno The remote receiver's ackno (acknowledgment number)
//! \param window_size The remote receiver's advertised window size
void TCPSender::ack_received(const WrappingInt32 ackno, const uint16_t window_size) {
    this->ackno = unwrap(ackno,_isn,checkpoint);
    this->window_size = window_size;
    
}

//! \param[in] ms_since_last_tick the number of milliseconds since the last call to this method
void TCPSender::tick(const size_t ms_since_last_tick) { 
    time+=ms_since_last_tick;
    if(time>=rto){
        
    }
}

unsigned int TCPSender::consecutive_retransmissions() const { return {}; }

void TCPSender::send_empty_segment() {}

void TCPSender::send_segment(int read_count, TCPSegment seg, size_t _next_seqno) {
    seg.payload() = _stream.read(read_count);
    _segments_out.push(seg);
    _segments_cache.push(seg);
    _next_seqno += seg.length_in_sequence_space();
}