#ifndef _GENERAL_HPP_
#define _GENERAL_HPP_

#include <iostream>
using std::cout;
using std::cerr;
using std::endl;
#include <cstring>
#include <string>
using std::string;

#include <cstdint>
using std::int64_t;
using std::uint64_t;

#include <boost/log/trivial.hpp>

// C++0x
#if __cplusplus <= 199711L
#define nullptr 0

#include <boost/function.hpp>
using boost::function;

#include <boost/shared_ptr.hpp>
#include <boost/make_shared.hpp>
using boost::shared_ptr;
using boost::make_shared;

#include <boost/bind.hpp>
using boost::bind;

#else

#include <memory>

using std::function;

using std::shared_ptr;
using std::make_shared;

using std::bind;

#endif

#include <signal.h>

namespace forkp {

static inline const char* basename(const char* file) {
    const char* p = strrchr(file, '/');
    if (p) return p + 1;

    p = strrchr(file, '\\');
    if (p) return p + 1;

    return file;
}

#if 0
#define BOOST_LOG_T(x) std::cerr<<std::endl<<#x<<":"<<basename(__FILE__)<<__LINE__<<"[@"<<__func__<<"]"<<" "
#else
#define BOOST_LOG_T(x) BOOST_LOG_TRIVIAL(x)<<basename(__FILE__)<<":"<<__LINE__<<"[@"<<__func__<<"]"<<" "
#endif


int st_make_nonblock(int socket);

enum class FORKP_SIG {
    FORKP_INFO = SIGUSR1,  /* 显示forkp信息 */
    SHDN_CHLD = SIGTERM,   /* 杀死所有children process*/
    REOP_CHLD = SIGUSR2,   /* 杀死所有children process*/
    WATCH_DOG = SIGWINCH,  /* 看门狗，只在Process模式下支持 */
    CHLD      = SIGCHLD,
    PIPE      = SIGPIPE,
};

struct forkp_sig_cmd {
    volatile bool terminate;
    volatile bool shutdown_child;
    volatile bool reopen_child;
};
extern struct forkp_sig_cmd  FORKP_SIG_CMD;

#define FORKP_SIG_R(x) (static_cast<int>(x))

#define FORKP_SIG_BLOCK(x) do { \
    sigset_t set; \
    ::sigemptyset(&set); \
    ::sigaddset(&set, FORKP_SIG_R(x)); \
    ::sigprocmask(SIG_BLOCK, &set, NULL); \
} while(0)

#define FORKP_SIG_UNBLOCK(x) do { \
    sigset_t set; \
    ::sigemptyset(&set); \
    ::sigaddset(&set, FORKP_SIG_R(x)); \
    ::sigprocmask(SIG_UNBLOCK, &set, NULL); \
} while(0)

// RAII signal helper
class FORKP_SIG_GUARD {
public:
    FORKP_SIG_GUARD(FORKP_SIG sig):
    sig_(sig) {
        FORKP_SIG_BLOCK(sig_);
    }

    ~FORKP_SIG_GUARD() {
        FORKP_SIG_UNBLOCK(sig_);
    }
private:
    FORKP_SIG sig_;
};

}



#endif // _GENERAL_HPP_
