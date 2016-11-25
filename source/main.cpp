#include <sys/types.h>          /* See NOTES */
#include <string>

#include "general.hpp"
#include "forkp.hpp"

void taskFu(int id) {
    for (;;){

        std::cerr << "Start taskFunc with: " << id << std::endl;
        ::sleep(1);
    }
}


int main(int argc, char* argv[])
{
    // for change process name
    forkp::exec_main_argv = argv;

    forkp::Master master;
    master.user_init_register( []() -> bool {
                            std::cout << "User Init Func Called!" << std::endl;
                            return true; });
    master.Init();

    master.spawn_workers("test proc", std::bind(taskFu, 2));
    master.spawn_workers("test proc", std::bind(taskFu, 8));
    master.MasterLoop();

    return 0;
}

