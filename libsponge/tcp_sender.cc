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
    , consec_retr(0)
    , rto(_initial_retransmission_timeout)
    , flight(0)
    , send_base(0)
    , _fin(false)
    , timer_running(false) {}

uint64_t TCPSender::bytes_in_flight() const { return flight; }

void TCPSender::fill_window() {
    if (nextseqnum == 0) {
        // state is CLOSED,where no syn sent
        TCPSegment seg;
        seg.header().syn = true;
        send_segment(seg);
        return;
    } else if (nextseqnum > 0 && nextseqnum == flight) {
        // state is SYN SENT, don't send SYN
        return;
    }
    // normal segment
    TCPSegment seg;
    size_t win = _window_size > 0 ? _window_size : 1;
    while (win != (nextseqnum - send_base) && !_fin) {  //当窗口中还有未发送的字符时
        //结束发送
        seg.payload() = _stream.read(min(win - (nextseqnum - send_base), TCPConfig::MAX_PAYLOAD_SIZE));
        if (seg.length_in_sequence_space() < win && _stream.eof()) {
            seg.header().fin = true;
            _fin = true;
        }
        if (seg.length_in_sequence_space() == 0)
            return;
        send_segment(seg);
    }
}

//! \param ackno The remote receiver's ackno (acknowledgment number)
//! \param window_size The remote receiver's advertised window size
void TCPSender::ack_received(const WrappingInt32 ackno, const uint16_t window_size) {
    y = unwrap(ackno, _isn, send_base);
    if (y > nextseqnum) {
        return;
    }
    _window_size = window_size;//?
    if (y > send_base) {
        send_base = y;
        while (!_segments_cache.empty()) {
            TCPSegment seg = _segments_cache.front();
            if (static_cast<int32_t>(seg.length_in_sequence_space()) <= ackno - seg.header().seqno) {
                flight -= seg.length_in_sequence_space();
                _segments_cache.pop();
            } else {
                break;
            }
        }
        fill_window();
        rto = _initial_retransmission_timeout;
        consec_retr = 0;
        //if there are currently not-yet-acknowledged segments
        if (!_segments_cache.empty()) {
            //start timer
            time = 0;
            timer_running = true;
        }
    }
}

//! \param[in] ms_since_last_tick the number of milliseconds since the last call to this method
void TCPSender::tick(const size_t ms_since_last_tick) {
    time += ms_since_last_tick;
    //When all outstanding data has been acknowledged, stop the retransmission timer.
    if (_segments_cache.empty()) {
        timer_running = false;
        return;
    }

    if (time >= rto) {
        //retransmit not-yet-acknowledged segment with smallest sequence number
        TCPSegment seg = _segments_cache.front();
        _segments_out.push(seg);
        if (_window_size != 0) {
            consec_retr++;
            rto = rto * 2;
        }
        // start timer
        timer_running = true;
        time = 0;
    }
}

unsigned int TCPSender::consecutive_retransmissions() const { return consec_retr; }

void TCPSender::send_empty_segment() {
    TCPSegment seg;
    _segments_out.push(seg);
}

//发送数据包(除了组装payload,设置syn/fin标志位,其它都做了)
void TCPSender::send_segment(TCPSegment seg) {
    // create tcp segment with sequence number nextseqnum
    seg.header().seqno = wrap(nextseqnum, _isn);

    // if timer not running,then run it
    if (!timer_running) {
        timer_running = true;
        time = 0;
    }

    _segments_out.push(seg);  // pass segment to ip
    _segments_cache.push(seg);

    nextseqnum += seg.length_in_sequence_space();  // nextseqnum=nextseqnum+length(data)
    flight += seg.length_in_sequence_space();
}
