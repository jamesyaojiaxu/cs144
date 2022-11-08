#include "tcp_connection.hh"

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

size_t TCPConnection::time_since_last_segment_received() const { return time; }

void TCPConnection::segment_received(const TCPSegment &seg) {
    if (!activ) {
        return;
    }
    time = 0;
    if (seg.header().rst) {
        _sender.stream_in().set_error();
        _receiver.stream_out().set_error();
        activ = false;
    }
    // in listen state
    else if (_sender.next_seqno_absolute() == 0) {  // sender为closed状态可唯一确定tcp为listen状态
        if (seg.header().syn) {
            _receiver.segment_received(seg);
            connect();
        }
    }
    // in SYN_SENT state
    else if (!_receiver.ackno().has_value() && _sender.next_seqno_absolute() == _sender.bytes_in_flight()) {
        if (seg.header().syn && seg.header().ack) {
            _receiver.segment_received(seg);
            _sender.ack_received(seg.header().ackno, seg.header().win);
            _sender.send_empty_segment();
            send_data();
        } else if (seg.header().syn && !seg.header().ack) {
            // simultaneous open
            _receiver.segment_received(seg);
            _sender.send_empty_segment();
            send_data();
        }
    }
    // in SYN_RCVD state
    else if (_receiver.ackno().has_value() && !_receiver.stream_out().input_ended() &&
             _sender.next_seqno_absolute() == _sender.bytes_in_flight()) {
        if (seg.header().ack) {
            _receiver.segment_received(seg);
            _sender.ack_received(seg.header().ackno, seg.header().win);
        }
    }
    // in ESTABLISHED state
    else if (_receiver.ackno().has_value() && !_receiver.stream_out().input_ended() &&
             _sender.next_seqno_absolute() > _sender.bytes_in_flight() && !_sender.stream_in().eof()) {
        _receiver.segment_received(seg);
        _sender.ack_received(seg.header().ackno, seg.header().win);
        if (seg.length_in_sequence_space() > 0) {
            _sender.send_empty_segment();
        }
        _sender.fill_window();
        send_data();
    }
    // close wait
    else if (!_sender.stream_in().eof() && _receiver.stream_out().input_ended()) {
        _receiver.segment_received(seg);
        _sender.ack_received(seg.header().ackno, seg.header().win);
        _sender.fill_window();
        if (seg.header().fin) {
            _sender.send_empty_segment();
        }
        send_data();
    }
    // 当前是Fin-Wait-1状态
    else if (_sender.stream_in().eof() && _sender.next_seqno_absolute() == _sender.stream_in().bytes_written() + 2 &&
             _sender.bytes_in_flight() > 0 && !_receiver.stream_out().input_ended()) {
        if (seg.header().fin) {
            _receiver.segment_received(seg);
            _sender.ack_received(seg.header().ackno, seg.header().win);
            _sender.send_empty_segment();
            send_data();
        } else if (seg.header().ack) {
            _receiver.segment_received(seg);
            _sender.ack_received(seg.header().ackno, seg.header().win);
        }
    }
    // closing
    else if (_sender.stream_in().eof() && _sender.next_seqno_absolute() == _sender.stream_in().bytes_written() + 2 &&
             _sender.bytes_in_flight() > 0 && _receiver.stream_out().input_ended()) {
        _receiver.segment_received(seg);
        _sender.ack_received(seg.header().ackno, seg.header().win);
    }
    // 当前是Fin-Wait-2状态
    else if (_sender.stream_in().eof() && _sender.next_seqno_absolute() == _sender.stream_in().bytes_written() + 2 &&
             _sender.bytes_in_flight() == 0 && !_receiver.stream_out().input_ended()) {
        //if (seg.header().fin) {
            _receiver.segment_received(seg);
            _sender.ack_received(seg.header().ackno, seg.header().win);
            _sender.send_empty_segment();
            send_data();
        //}
    }
    // 当前是Time-Wait状态
    else if (_sender.stream_in().eof() && _sender.next_seqno_absolute() == _sender.stream_in().bytes_written() + 2 &&
             _sender.bytes_in_flight() == 0 && _receiver.stream_out().input_ended()) {
        if (seg.header().fin) {
            _receiver.segment_received(seg);
            _sender.ack_received(seg.header().ackno, seg.header().win);
            _sender.send_empty_segment();
            send_data();
        }
    }
    // 其他状态
    else {
        _sender.ack_received(seg.header().ackno, seg.header().win);
        _receiver.segment_received(seg);
        _sender.fill_window();
        send_data();
    }

    if (_receiver.ackno().has_value() && (seg.length_in_sequence_space() == 0) &&
        seg.header().seqno == _receiver.ackno().value() - 1) {
        _sender.fill_window();
        _sender.send_empty_segment();
    }
    /*
        1.Fin-Wait-1的ack要send_data?这个不太理解
        2.ESTABLISHED下的状态转换？
        3.其他状态是哪些状态？
        4.if(!activ)
    */
    /*
        _receiver.segment_received(seg);
        if (seg.header().ack == true) {
            _sender.ack_received(seg.header().ackno, seg.header().win);
        }
        if (seg.length_in_sequence_space() > 0) {
            has_ack = true;
            _sender.send_empty_segment();
            TCPSegment reply = _sender.segments_out().front();
            reply.header().ackno = _receiver.ackno();
            reply.header().win = _receiver.window_size();
            segments_out.push(reply);
        }*/
}

bool TCPConnection::active() const { return activ; }

size_t TCPConnection::write(const string &data) {
    size_t size = _sender.stream_in().write(data);
    _sender.fill_window();
    send_data();
    return size;
}

//! \param[in] ms_since_last_tick number of milliseconds since the last call to this method
void TCPConnection::tick(const size_t ms_since_last_tick) {
    if (!activ) {
        return;
    }
    time += ms_since_last_tick;
    _sender.tick(ms_since_last_tick);
    if (_sender.consecutive_retransmissions() > TCPConfig::MAX_RETX_ATTEMPTS) {
        _sender.send_empty_segment();
        TCPSegment seg = _sender.segments_out().front();
        seg.header().rst = true;
        _sender.segments_out().pop();
        _segments_out.push(seg);

        // 在出站入站流中标记错误，使active返回false
        _sender.stream_in().set_error();
        _receiver.stream_out().set_error();
        activ = false;
        return;
    }
    send_data();  // end the connection cleanly if necessary
}

void TCPConnection::end_input_stream() {
    _sender.stream_in().end_input();
    _sender.fill_window();
    send_data();
}

void TCPConnection::connect() {
    _sender.fill_window();
    send_data();
}

TCPConnection::~TCPConnection() {
    try {
        if (active()) {
            cerr << "Warning: Unclean shutdown of TCPConnection\n";
            // Your code here: need to send a RST segment to the peer
            _sender.send_empty_segment();
            TCPSegment seg = _sender.segments_out().front();
            seg.header().rst = true;
            _sender.segments_out().pop();
            _segments_out.push(seg);

            // 在出站入站流中标记错误，使active返回false
            _sender.stream_in().set_error();
            _receiver.stream_out().set_error();
            activ = false;
            return;
        }
    } catch (const exception &e) {
        std::cerr << "Exception destructing TCP FSM: " << e.what() << std::endl;
    }
}

//功能：添加ackno后再发送
void TCPConnection::send_data() {
    while (!_sender.segments_out().empty()) {
        TCPSegment seg = _sender.segments_out().front();
        _sender.segments_out().pop();
        if (_receiver.ackno().has_value()) {
            seg.header().ack = true;
            seg.header().ackno = _receiver.ackno().value();
            seg.header().win = _receiver.window_size();
        }
        _segments_out.push(seg);
    }
    if (_receiver.stream_out().input_ended()) {
        if (!_sender.stream_in().eof()) {
            _linger_after_streams_finish = false;
        } else if (_sender.bytes_in_flight() == 0) {
            if (_linger_after_streams_finish == false || time >= 10 * _cfg.rt_timeout) {
                activ = false;
            }
        }
    }
}
