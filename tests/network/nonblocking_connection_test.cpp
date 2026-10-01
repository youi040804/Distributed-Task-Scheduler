#include <cassert>      
#include <fcntl.h>      // fcntl, F_GETFL, O_NONBLOCK
#include <iostream>    
#include <sys/socket.h> // socketpair, AF_UNIX, SOCK_STREAM
#include <unistd.h>     // close

#include "network/Connection.h"

int main() {
    int fds[2];

    const int result = ::socketpair(AF_UNIX, SOCK_STREAM, 0, fds);

    assert(result == 0);

    sockaddr_in dummyAddr{};

    dts::Connection connection(fds[0], dummyAddr);

    assert(connection.setNonBlocking());

    const int flags = ::fcntl(connection.fd(), F_GETFL, 0);

    assert(flags != -1);
    assert((flags & O_NONBLOCK) != 0);

    connection.disconnect();
    ::close(fds[1]);

    std::cout << "nonblocking_connection_test passed" << std::endl;
    return 0;
}