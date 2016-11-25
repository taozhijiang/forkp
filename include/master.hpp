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

typedef struct {
    pid_t        this_pid;
    time_t       this_start_tm;
    std::size_t this_miss_cnt; // 当前启动时候错过的心跳

    time_t       start_tm;
    std::size_t restart_cnt;   // 被启动次数

    Worker_Ptr   worker;
} WorkerStat_t;
using WorkerStat_Ptr = std::shared_ptr<WorkerStat_t>;

class Master final{

public:
    Master(): workers_(), dead_workers_(), init_list_()
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

    bool spawn_workers(const char* name, const taskFunc& func){
        Worker_Ptr node = std::make_shared<Worker>(name, func);
        WorkerStat_Ptr workstat = std::make_shared<WorkerStat_t>();
        if (!node || !workstat)
            return false;

        // 首次启动参数
        workstat->start_tm = time(NULL);
        workstat->restart_cnt = 0;
        workstat->worker = node;

        if (!fork_prepare(workstat)) {
            BOOST_LOG_T(error) << "fork_prepare worker failed, push to dead_workers!";
            dead_workers_.emplace_back(workstat);
            return false;
        }

        pid_t worker_pid = spawn_workers_internel(workstat);

        // parent process
        if (worker_pid < 0) {
            BOOST_LOG_T(error) << "start worker failed, push to dead_workers!";
            dead_workers_.emplace_back(workstat);
            return false;
        }

        assert(workers_.find(worker_pid) == workers_.end());
        workers_[worker_pid] = workstat;

        BOOST_LOG_T(debug) << "start worker OK for " << workstat->worker->proc_title_ <<
            ", with pid=" << worker_pid ;
        return true;
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

    bool fork_prepare(WorkerStat_Ptr& workstat) {
        int pipefd[2];

        workstat->worker->workerReset();
        if (pipe(pipefd) < 0)
        {
            BOOST_LOG_T(error) << "pipe() Error!";
            return false;
        }

        st_make_nonblock(pipefd[0]);
        st_make_nonblock(pipefd[1]);

        workstat->worker->notify_.read_ = pipefd[0];
        workstat->worker->notify_.write_ = pipefd[1];

        workstat->this_miss_cnt = 0;

        return true;
    }


    // -1 for error case
    pid_t spawn_workers_internel(WorkerStat_Ptr& workstat)
    {
        pid_t pid = fork();
        if (pid < 0) {
            BOOST_LOG_T(error) << "Fork() Error!";
            return -1;
        }

        if (pid == 0) // child process
        {
            std::swap(workstat->worker->notify_.read_, workstat->worker->notify_.write_);
            workstat->worker->startProcess();
        }

        workstat->this_pid = pid;
        workstat->this_start_tm = time(NULL);

        // parent process continue
        return pid;
    }

private:
    const char* name_ = "forkp master";
    std::map<pid_t, WorkerStat_Ptr> workers_;
    std::vector<WorkerStat_Ptr>     dead_workers_;  //没有启动成功的任务
    std::vector<InitFunc> init_list_;  // container set not supportted
};

}



#endif // _MASTER_HPP_
