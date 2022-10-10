#include "tcp_receiver.hh"

// Dummy implementation of a TCP receiver

// For Lab 2, please replace with a real implementation that passes the
// automated checks run by `make check_lab2`.

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

void TCPReceiver::segment_received(const TCPSegment &seg) {
    //WrappingInt32 isn;
    if(seg.header().syn&&!syn){
           isn = seg.header().seqno;
           syn = true;
           if(seg.header().fin){
                fin = true;
           }
           _reassembler.push_substring(seg.payload().copy(),0,fin);
           return;
    }
    if(seg.header().fin&&syn){
        fin=seg.header().fin;
    }
    _reassembler.push_substring(seg.payload().copy(),unwrap(seg.header().seqno,isn,_reassembler.unass-1)-1,fin);
    
    //DUMMY_CODE(seg);
}

optional<WrappingInt32> TCPReceiver::ackno() const { 
    if(!syn){
        return {};
    }
    return wrap(_reassembler.unass+1+(_reassembler.empty() &&fin),isn); 
}

size_t TCPReceiver::window_size() const { return _capacity-_reassembler.stream_out().buffer_size(); }
