#include "mini_socket.hpp"

#include <sstream>
#include <cstring>

#ifndef NDEBUG
#include <iostream>
#endif

using namespace std;

#if defined WIN32 or defined _WIN32

#ifndef WSVERS
#define WSVERS MAKEWORD(2, 0)
#endif

namespace MiniSocket {

class _WSAStartupHolder_ {
public:
    _WSAStartupHolder_()
    {
        if (WSAStartup(WSVERS, &wsadata) != 0)
            throw SocketException("WSAStartup error");
#ifndef NDEBUG
        cout << "WSAStartup ok" << endl;
#endif
    } 

    ~_WSAStartupHolder_()
    {
        WSACleanup();
#ifndef NDEBUG
        cout << "WSACleanup ok" << endl;
#endif
    }

private:
    WSADATA wsadata;
};

shared_ptr<_WSAStartupHolder_> makeSharedWSAStartupHolder()
{
    static shared_ptr<_WSAStartupHolder_> shared_holder(new _WSAStartupHolder_);
    return shared_holder;
}

_WSAStartupSharedHolder_::_WSAStartupSharedHolder_()
{
#ifndef NDEBUG
    cout << "_WSAStartupSharedHolder_::_WSAStartupSharedHolder_()" << endl;
#endif
    sharedHolder_ = makeSharedWSAStartupHolder();
}

_WSAStartupSharedHolder_::~_WSAStartupSharedHolder_()
{
#ifndef NDEBUG
    cout << "_WSAStartupSharedHolder_::~_WSAStartupSharedHolder_()" << endl;
#endif
}

string getLastSystemErrorStr()
{
    HLOCAL hlocal = NULL;
    FormatMessage(
        FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_ALLOCATE_BUFFER, NULL,
        WSAGetLastError(), MAKELANGID(LANG_ENGLISH, SUBLANG_ENGLISH_US),
        (LPTSTR) &hlocal, 0, NULL);
    std::string err_msg((const char *)hlocal);
    LocalFree(hlocal);

    return err_msg;
}

}

#else   // not win32

namespace MiniSocket {

string getLastSystemErrorStr()
{
    return strerror(errno);
}

}   // namespace MiniSocket

#endif

namespace MiniSocket {

// SocketException
SocketException::SocketException(const string &message) :
    runtime_error(message) 
{
}

SocketException::SocketException(const string &message, const string &detail) :
    runtime_error(message + ": " + detail) 
{
}

// SocketAddressView
tuple<string, uint16_t> SocketAddressView::getAddressPort(const sockaddr *sa, socklen_t salen)
{
	static const tuple<string, uint16_t> null_result;
	char str[128] = {0};

	switch (sa->sa_family) {
	case AF_INET: {
		sockaddr_in	*sin = (sockaddr_in *) sa;

		if (inet_ntop(AF_INET, &sin->sin_addr, str, sizeof(str)) == NULL)
			return null_result;

        return make_tuple(std::string(str), ntohs(sin->sin_port));
	}

	case AF_INET6: {
		sockaddr_in6	*sin6 = (sockaddr_in6 *) sa;

		if (inet_ntop(AF_INET6, &sin6->sin6_addr, str, sizeof(str)) == NULL)
			return null_result;

        return make_tuple(std::string(str), ntohs(sin6->sin6_port));
	}

	default:
        return null_result;
	}
    return null_result;
}

string SocketAddressView::toString(const sockaddr *sa, socklen_t salen)
{
    static const std::string null_result;

    string address;
    uint16_t port;
    std::tie(address, port) = getAddressPort(sa, salen);
    if (port == 0)
        return null_result;

    ostringstream os;
    if (sa->sa_family == AF_INET6)
        os << "[";
    os << address;
    if (sa->sa_family == AF_INET6)
        os << "]";
    os << ":" << port;
    return os.str();
}

SocketAddressView::SocketAddressView(const sockaddr *addrVal, socklen_t addrLenVal) : addr_(addrVal), addrLen_(addrLenVal)
{
}

string SocketAddressView::toString() const
{
    return toString(getSockaddr(), getSockaddrLen());
}

tuple<std::string, uint16_t> SocketAddressView::getAddressPort() const
{
    return getAddressPort(getSockaddr(), getSockaddrLen());
}

const sockaddr *SocketAddressView::getSockaddr() const 
{
    return addr_;
}

socklen_t SocketAddressView::getSockaddrLen() const 
{
    return addrLen_;
}

SocketAddress::SocketAddress(): addrLen_(sizeof(sockaddr_storage))
{
    memset(&addr_, 0, sizeof(addr_));
}

// SocketAddress
SocketAddress::SocketAddress(const char *address, uint16_t port): SocketAddress()
{
    if (setAddressPort(address, port))
        return;

    ostringstream os;
    os << "the address is [" << address << "], and port is [" << port << "]";
    throw SocketException("Construct SocketAddress error", os.str());
}

SocketAddress::SocketAddress(sockaddr *addrVal, socklen_t addrLenVal):
    addrLen_(addrLenVal) 
{
    memcpy( &addr_, addrVal, addrLenVal );
}

bool SocketAddress::setAddressPort(const char *address, uint16_t port)
{
    sockaddr *sa = (sockaddr *) &addr_;

    sa->sa_family = AF_INET;
    sockaddr_in	*sin = (sockaddr_in *) sa;
    if (inet_pton(sa->sa_family, address, &sin->sin_addr) == 1) { // success
        sin->sin_port = htons(port);
        return true;
    }

    sa->sa_family = AF_INET6;
    sockaddr_in6 *sin6 = (sockaddr_in6 *) sa;
    if (inet_pton(sa->sa_family, address, &sin6->sin6_addr) == 1) { // success
        sin6->sin6_port = htons(port);
        return true;
    }

    return false;
}

string SocketAddress::toString() const
{
    return SocketAddressView::toString(getSockaddr(), getSockaddrLen());
}

tuple<std::string, uint16_t> SocketAddress::getAddressPort() const
{
    return SocketAddressView::getAddressPort(getSockaddr(), getSockaddrLen());
}

sockaddr *SocketAddress::getSockaddr() const 
{
    return (sockaddr *)&addr_;
}

socklen_t SocketAddress::getSockaddrLen() const 
{
    return addrLen_;
}

// Socket
Socket::~Socket()
{
    if (isValid())
        closeSocket();
}

void Socket::closeSocket()
{
#if defined WIN32 or defined _WIN32
  closesocket(sockDesc_);
#else
  shutdown(sockDesc_, SHUT_RD);
  close(sockDesc_);
#endif
  sockDesc_ = INVALID_SOCKET;
}

bool Socket::isValid() const
{
    return sockDesc_ != INVALID_SOCKET; 
}

SocketAddress Socket::getLocalAddress() const
{
    sockaddr_storage addr;
    socklen_t addrLen = sizeof(addr);

    if (getsockname(sockDesc_, (sockaddr *) &addr, &addrLen) != 0) {
        throw SocketException("Fetch of local address failed (getsockname())",
                getLastSystemErrorStr());
    }

    return SocketAddress((sockaddr *)&addr, addrLen);
}

Socket::Socket(): sockDesc_(INVALID_SOCKET)
{
}

void Socket::createSocket(int domain, int type, int protocol)
{
    if (isValid())
        closeSocket();

    sockDesc_ = socket(domain, type, protocol);
    if (!isValid()) {
        throw SocketException("Can't create socket", 
                getLastSystemErrorStr());
    }
}

void Socket::bind(const SocketAddress &localAddress)
{
    if (::bind(sockDesc_, localAddress.getSockaddr(), localAddress.getSockaddrLen()) != 0) {
        throw SocketException("bind error", 
                getLastSystemErrorStr());
    }
}

// CommunicatingSocket 
void CommunicatingSocket::connect(const SocketAddress &foreignAddress)
{
    if (::connect(sockDesc_, foreignAddress.getSockaddr(), foreignAddress.getSockaddrLen()) != 0) {
        throw SocketException("connect error", 
                getLastSystemErrorStr());
    }
}

bool CommunicatingSocket::connect(const SocketAddress &foreignAddress, const std::nothrow_t &nothrow_value)
{
    int ret = ::connect(sockDesc_, foreignAddress.getSockaddr(), foreignAddress.getSockaddrLen());
    return (ret == 0);
}

size_t CommunicatingSocket::send(const char *buffer, int bufferLen)
{
    int n = ::send(sockDesc_, buffer, bufferLen, 0);
    if ( n < 0 ) {
        throw SocketException("Send failed (send())",
                getLastSystemErrorStr());
    }

    return n;
}

size_t CommunicatingSocket::recv(char *buffer, int bufferLen)
{
    int n = ::recv(sockDesc_, buffer, bufferLen, 0); 
    if ( n < 0 ) {
        throw SocketException("Receive failed (recv())",
                getLastSystemErrorStr());
    }

    return n;
}

SocketAddress CommunicatingSocket::getForeignAddress() const
{
    sockaddr_storage addr;
    socklen_t addrLen = sizeof(addr);

    if (getpeername(sockDesc_, (sockaddr *) &addr, &addrLen) != 0) {
        throw SocketException("Fetch of foreign address failed (getpeername())",
                getLastSystemErrorStr());
    }

    return SocketAddress((sockaddr *)&addr, addrLen);
}

// SocketStreamBuffer 
template <class CharT, class Traits = std::char_traits<CharT> >
class SocketStreamBuffer : public std::basic_streambuf<CharT, Traits> {
public:
  typedef typename Traits::int_type                 int_type;

  SocketStreamBuffer(TCPSocket *sock): sock_(sock) {
    this->setg(inBuffer_, inBuffer_ + sizeof(inBuffer_),
         inBuffer_ + sizeof(inBuffer_));
    this->setp(outBuffer_, outBuffer_ + sizeof(outBuffer_));
  }

protected:
  int_type overflow(int_type c = Traits::eof()) {
    this->sync();

    if (c != Traits::eof()) {
      this->sputc(Traits::to_char_type(c));
    }

    return 0;
  }

  int sync() {
    sock_->sendAll(outBuffer_, (this->pptr() - outBuffer_) * sizeof(CharT));
    this->setp(outBuffer_, outBuffer_ + sizeof(outBuffer_));
    return 0;
  }

  int_type underflow() {
    size_t len = sock_->recv(inBuffer_, sizeof(inBuffer_) * sizeof(CharT));

    if (len == 0) {
      return Traits::eof();
    }

    this->setg(inBuffer_, inBuffer_, inBuffer_ + len / sizeof(CharT));
    return this->sgetc();
  }

private:
  TCPSocket *sock_;

  CharT inBuffer_[1024];
  CharT outBuffer_[1024];
};

// TCPSocket 
TCPSocket::TCPSocket()
{
}

TCPSocket::TCPSocket(SOCKET sockDesc)
{
    sockDesc_ = sockDesc;
}

TCPSocket::TCPSocket(const SocketAddress &foreignAddress)
{
    int domain = foreignAddress.getSockaddr()->sa_family;
    createSocket(domain, SOCK_STREAM, 0);
    connect(foreignAddress);
}

void TCPSocket::sendAll(const char *buffer, int bufferLen)
{
	auto ptr = (const char *) buffer;
	auto nleft = bufferLen;
    int nwritten = 0;
	while (nleft > 0) {
		nwritten = send(ptr, nleft);
		nleft -= nwritten;
		ptr   += nwritten;
	}
}

std::iostream &TCPSocket::getStream()
{
    if (myStream_ == NULL) {
        myStreambuf_ = new SocketStreamBuffer<char>(this);
        myStream_ = new iostream(myStreambuf_);
    }
    return *myStream_;
}

// TCPServerSocket
TCPServerSocket::TCPServerSocket(const SocketAddress &localAddress)
{
    int domain = localAddress.getSockaddr()->sa_family;
    createSocket(domain, SOCK_STREAM, 0);
    bind(localAddress);
	const int LISTENQ = 1024;
	listen(LISTENQ);
}

void TCPServerSocket::listen(int backlog)
{
    if (::listen(sockDesc_, backlog) != 0) {
        throw SocketException("listen error", 
                getLastSystemErrorStr());
    }
}

shared_ptr<TCPSocket> TCPServerSocket::accept()
{
    SOCKET newConnSD;
    if ((newConnSD = ::accept(sockDesc_, NULL, 0)) == INVALID_SOCKET) {
        throw SocketException("Accept failed (accept())",
                getLastSystemErrorStr());
    }

    return shared_ptr<TCPSocket>(new TCPSocket(newConnSD));
}

}   // namesapce MiniSocket