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

// C++0x
#if __cplusplus <= 199711L
#define nullptr 0
#include <boost/log/trivial.hpp>

#include <boost/function.hpp>
using boost::function;

#include <boost/shared_ptr.hpp>
#include <boost/make_shared.hpp>
using boost::shared_ptr;
using boost::make_shared;

#else

#include <memory>

using std::function;

using std::shared_ptr;
using std::make_shared;

#endif


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

}



#endif // _GENERAL_HPP_
