#ifndef _FORKP_HPP_
#define _FORKP_HPP_

#include "general.hpp"
#include <functional>

namespace forkp {

using InitFunc = std::function<bool()>;
extern bool user_init_register(const InitFunc& func);

extern bool forkp_init();

}



#endif // _FORKP_HPP_
