#include "general.hpp"

#include <unistd.h>
#include <fcntl.h>

namespace forkp {

    int st_make_nonblock(int socket)
    {
        int flags = 0;

        flags = fcntl (socket, F_GETFL, 0);
    	flags |= O_NONBLOCK;
        fcntl (socket, F_SETFL, flags);

        return 0;
    }

}

