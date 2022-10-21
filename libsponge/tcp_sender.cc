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
    ,consec_retr(0)
    ,rto(0)
    ,lastackno(0)
    ,flight(0)
    ,absackno(0)
    ,timer_running(false) {}

uint64_t TCPSender::bytes_in_flight() const { return flight; }

void TCPSender::fill_window() {
    if (_syn) {
        TCPSegment seg;
        seg.header().syn = true;
        _syn = false;
        send_segment(seg);
        return;
    }
    // normal segment
    TCPSegment seg;
    while (window_size != (_next_seqno-absackno) && !_fin) {//当窗口中还有未发送的字符时
        //结束发送
        if(_stream.input_ended() && _stream.buffer_empty()){
            seg.header().fin = true;
            _fin = true;
        }
        seg.payload() = _stream.read(min(window_size -(_next_seqno-absackno),TCPConfig::MAX_PAYLOAD_SIZE));
        send_segment(seg);
    }
}

//! \param ackno The remote receiver's ackno (acknowledgment number)
//! \param window_size The remote receiver's advertised window size
void TCPSender::ack_received(const WrappingInt32 ackno, const uint16_t window_size) {
    this->absackno = unwrap(ackno,_isn,_next_seqno);
    this->window_size = window_size>0?window_size:1;
    while(!_segments_cache.empty()){
        TCPSegment seg=_segments_cache.front();
        if(unwrap(seg.header().seqno,_isn,_next_seqno)+seg.length_in_sequence_space()<=absackno){
            _segments_cache.pop();
            flight-=seg.length_in_sequence_space();
        }else{
            break;
        }
    }
    fill_window();
    rto=_initial_retransmission_timeout;
    consec_retr=0;
    if(!_segments_cache.empty()){
        time=0;
        timer_running = true;
    }
}

//! \param[in] ms_since_last_tick the number of milliseconds since the last call to this method
void TCPSender::tick(const size_t ms_since_last_tick) { 
    time+=ms_since_last_tick;
    if(_segments_cache.empty()){
        timer_running = false;
    }
    if(time >= rto){
        TCPSegment seg=_segments_cache.front();
        _segments_out.push(seg);
        if(window_size != 0){
            consec_retr++;
            rto=rto*2;
        }
        //start timer
        timer_running = true;
        time = 0;
    }
}

unsigned int TCPSender::consecutive_retransmissions() const { return consec_retr; }

void TCPSender::send_empty_segment() {
    TCPSegment seg;
    _segments_out.push(seg);
}

void TCPSender::send_segment(TCPSegment seg) {
    //if timer not running,then run it
    if(!timer_running){
        timer_running=true;
        time=0;
    }
    seg.header().seqno = wrap(_next_seqno,_isn);

    _segments_out.push(seg);
    _segments_cache.push(seg);

    _next_seqno += seg.length_in_sequence_space();
    flight+=seg.length_in_sequence_space();
}