#ifndef _MASTER_HPP_
#define _MASTER_HPP_

#include "general.hpp"
#include "worker.hpp"

#include <map>
#include <sys/types.h>
#include <unistd.h>

namespace forkp {

/**
 * Alive check method
 *
 * PIPE 'C'
 * kill(0) exist
 */

extern void signal_init();
extern void boost_log_init(const string& prefix);
extern void backtrace_init();

using InitFunc = std::function<bool()>;
using taskFunc = std::function<void()>;

extern bool st_rename_process(const char* name);

class Master final{

public:
    Master(): workers_(), init_list_()
    {}

    bool Init() {

    	boost_log_init("forkpRun");
        backtrace_init();
        signal_init();

        if (!user_init_proc())
            return false;

        st_rename_process(name_);

        return true;
    }

    bool user_init_register(const InitFunc& func){
        init_list_.emplace_back(func);
        return true;
    }

    std::shared_ptr<Worker> spawn_workers(const char* name, const taskFunc& func){
        std::shared_ptr<Worker> node = std::make_shared<Worker>(name, func);
        if (! node)
            return nullptr;

        pid_t pid = fork();
        if (pid < 0){
            BOOST_LOG_T(error) << "Fork() Error!";
            return nullptr;
        }

        int pipefd[2];
        if (pipe(pipefd) < 0)
        {
            BOOST_LOG_T(error) << "pipe() Error!";
            return nullptr;
        }

        st_make_nonblock(pipefd[0]);
        st_make_nonblock(pipefd[1]);

        node->notify_.read_ = pipefd[0];
        node->notify_.write_ = pipefd[1];

        if (pid == 0) // child process
        {
            std::swap(node->notify_.read_, node->notify_.write_);
            node->startProcess();
        }

        return node;
    }

    std::shared_ptr<Worker> exec_workers(){

        return nullptr;
    }

    void MasterLoop() {

    	for( ; ; ) {
            ::sleep(1);
        }

    }

    ~Master() = default;

private:
    bool user_init_proc() {
        for (const auto& it: init_list_){
            if (!it())
                return false;
        }

        BOOST_LOG_T(info) << "User Init OK!";
        return true;
    }

private:
    const char* name_ = "forkp master";
    std::map<pid_t, std::shared_ptr<Worker>> workers_;
    std::vector<InitFunc> init_list_;  // set not supportted
};

}



#endif // _MASTER_HPP_
