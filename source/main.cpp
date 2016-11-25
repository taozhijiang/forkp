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

bool init_func() {
    std::cout << "User Init Func Called!" << std::endl;
    return true;
}


int main(int argc, char* argv[])
{
    // for change process name
    forkp::exec_main_argv = argv;

    MasterIntance.user_init_register(init_func);

    MasterIntance.userInitProc();

    MasterIntance.spawnWorkers("test proc", std::bind(taskFu, 2));
    MasterIntance.spawnWorkers("test proc", std::bind(taskFu, 8));

    char *args[] = {"forkp worker exec_test", (char *) 0 };
    MasterIntance.spawnWorkers("execTest", "/home/taozj/forkp/test/exec_test", args);


    MasterIntance.masterLoop();

    return 0;
}

