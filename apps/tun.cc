#include "tun.hh"

#include "ipv4_datagram.hh"
#include "parser.hh"
#include "tcp_segment.hh"
#include "util.hh"

#include <cstdlib>
#include <exception>
#include <iostream>
#include <vector>

using namespace std;

int main() {
    try {
        TunFD tun("tun144");
        while (true) {
            auto buffer = tun.read();
            cerr << "\n\n***\n*** Got packet:\n***\n";
            hexdump(buffer.data(), buffer.size());

            IPv4Datagram ip_dgram;

            cerr << "attempting to parse as ipv4 datagram... ";
            if (ip_dgram.parse(move(buffer)) != ParseResult::NoError) {
                cerr << "failed.\n";
                continue;
            }

            cerr << "success! totlen=" << ip_dgram.header().len << ", IPv4 header contents:\n";
            cerr << ip_dgram.header().to_string();

            if (ip_dgram.header().proto != IPv4Header::PROTO_TCP) {
                cerr << "\nNot TCP, skipping.\n";
                continue;
            }

            cerr << "\nAttempting to parse as a TCP segment... ";

            TCPSegment tcp_seg;

            if (tcp_seg.parse(ip_dgram.payload(), ip_dgram.header().pseudo_cksum()) != ParseResult::NoError) {
                cerr << "failed.\n";
                continue;
            }

            cerr << "success! payload len=" << tcp_seg.payload().size() << ", TCP header contents:\n";
            cerr << tcp_seg.header().to_string() << endl;
        }
    } catch (const exception &e) {
        cerr << "Exception: " << e.what() << endl;
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
