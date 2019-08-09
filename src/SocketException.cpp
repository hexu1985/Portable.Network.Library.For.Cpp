#if defined WIN32 or defined _WIN32
#include <windows.h>
#endif

#include "SocketException.hpp"

namespace MiniSocket {

using std::string;

SocketException::SocketException(const string &message, const SocketError &error): 
    runtime_error(message), error_(error) 
{
}

SocketException::~SocketException()
{
}

}   // namespace MiniSocket