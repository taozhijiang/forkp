#ifndef _FORKP_HPP_
#define _FORKP_HPP_

#include "general.hpp"
#include <functional>
#include "master.hpp"

namespace forkp {

using InitFunc = std::function<bool()>;

extern char **exec_main_argv;

}



#endif // _FORKP_HPP_
