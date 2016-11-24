#include <sys/types.h>          /* See NOTES */
#include <string>

#include "general.hpp"
#include "forkp.hpp"

int main(int argc, char* argv[])
{
    forkp::user_init_register( []() -> bool {
                            std::cout << "User Init Func Called!" << std::endl;
                            return true; });
    forkp::forkp_init();

    return 0;
}

