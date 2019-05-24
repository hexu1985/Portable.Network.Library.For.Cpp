#include <iostream>
#include "mini_socket.hpp"

using namespace std;
using namespace MiniSocket;

int main(int argc, char *argv[])
{
    DNSResolver resolver;
    SocketAddressView addr;
    for (int i = 1; i < argc; i++) {
        cout << "resovle dns of " << argv[i] << endl;
        try {
            auto iter = resolver.query(argv[i], NULL, TransportLayerType::TCP);
            while (iter.hasNext()) {
                addr = iter.next();
                cout << "\taddress: " << addr.toString() << endl;
            }
        } catch(const SocketException &e) {
            cout << e.what() << endl;
        }
    }

    return 0;
}