#include "byte_stream.hh"

// Dummy implementation of a flow-controlled in-memory byte stream.

// For Lab 0, please replace with a real implementation that passes the
// automated checks run by `make check_lab0`.

// You will need to add private members to the class declaration in `byte_stream.hh`

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

ByteStream::ByteStream(const size_t capacity): _error(false), _endin(false),
                                                _Stream(),
                                                _Capacity(capacity), 
                                                _TotalWritten(0), _TotalRead(0){}

size_t ByteStream::write(const string &data) {
    int r = remaining_capacity();
    int bytewrite = r > data.size()?data.size():r;
    int count = 0;
    while (count < bytewrite){
	    _Stream.push_back(data[count++]);
    }
    _TotalWritten += count;
    return bytewrite;
	
}

//! \param[in] len bytes will be copied from the output side of the _Stream
//in short, return string with length of len when possible 
string ByteStream::peek_output(const size_t len) const {
	std::string peek = "";
    size_t Remainder =  this->buffer_size();
      /*2.the number of byte can be popped is the smaller value of Remainder and len*/
    size_t ByteRemoved =  (len >= Remainder) ? Remainder : len;
    int count = 0;
    while(count < ByteRemoved)
	   peek.push_back(_Stream[count++]);
    return peek;
}

//! \param[in] len bytes will be removed from the output side of the _Stream
void ByteStream::pop_output(const size_t len) {
/*1.get how many bytes has not been read*/
    if (len > _Stream.size()) {
        set_error();
    }
    size_t Remainder =  this->buffer_size();

    /*2.the number of byte can be popped is the smaller value of Remainder and len*/
    size_t ByteRemoved =  len >= Remainder ? Remainder : len; 

    /*3.modify the _ReadPtr to the appropriate position*/
    size_t count = 0;
    while(count++ < ByteRemoved)
        _Stream.pop_front();
    
    /*4.add the ByteRemoved to _TotalRead*/
    _TotalRead += ByteRemoved;
}

//! Read (i.e., copy and then pop) the next "len" bytes of the stream
//! \param[in] len bytes will be popped and returned
//! \returns a string
std::string ByteStream::read(const size_t len) {

    if (len > _Stream.size()) {
        set_error();
	return "";
    }
	std::string peek = peek_output(len);
	pop_output(len);
	return peek;
}

void ByteStream::end_input() {
_endin=true;
}

bool ByteStream::input_ended() const { return _endin == true; }

size_t ByteStream::buffer_size() const { return _Stream.size(); }

bool ByteStream::buffer_empty() const { return _Stream.empty(); }

bool ByteStream::eof() const { 
return input_ended() && buffer_empty();
}

size_t ByteStream::bytes_written() const { return _TotalWritten;}

size_t ByteStream::bytes_read() const { return _TotalRead; }

size_t ByteStream::remaining_capacity() const { 
	return _Capacity-_Stream.size(); 
}
