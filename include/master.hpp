#ifndef _MASTER_HPP_
#define _MASTER_HPP_

#include "general.hpp"
#include "worker.hpp"

#include <map>
#include <set>
#include <sys/types.h>
#include <unistd.h>

#include <boost/format.hpp>

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
    std::size_t restart_cnt;     // 启动成功次数
    std::size_t restart_err_cnt; // 启动失败次数

    Worker_Ptr   worker;
} WorkerStat_t;
using WorkerStat_Ptr = std::shared_ptr<WorkerStat_t>;

class Master final{

public:
    static Master& getInstance() {
        if (!master_instance_)
            createInstance();

        return *master_instance_;
    }

    bool user_init_register(const InitFunc& func){
        init_list_.emplace_back(func);
        return true;
    }

    WorkerStat_Ptr getWorkStatObj(pid_t pid) {
        if (workers_.find(pid) == workers_.end())
            return nullptr;

        return workers_[pid];
    }

    bool trimWorkStatObj(pid_t pid) {
        if (workers_.find(pid) == workers_.end())
            return false;
        return !!workers_.erase(pid);
    }

    bool spawnWorkers(const char* name, const char* exec, char *const *argv) {
        Worker_Ptr node = std::make_shared<Worker>(name, exec, argv);
        WorkerStat_Ptr workstat = std::make_shared<WorkerStat_t>();
        if (!node || !workstat)
            return false;

        // 首次启动参数
        workstat->start_tm = time(NULL);
        workstat->restart_cnt = 0;
        workstat->restart_err_cnt = 0;
        workstat->worker = node;

        return trySpawnWorkers(workstat);
    }

    // 用户空间启动进程
    bool spawnWorkers(const char* name, const taskFunc& func){
        Worker_Ptr node = std::make_shared<Worker>(name, func);
        WorkerStat_Ptr workstat = std::make_shared<WorkerStat_t>();
        if (!node || !workstat)
            return false;

        // 首次启动参数
        workstat->start_tm = time(NULL);
        workstat->restart_cnt = 0;
        workstat->restart_err_cnt = 0;
        workstat->worker = node;

        return trySpawnWorkers(workstat);
    }

    // 内部重启进程的接口
    bool trySpawnWorkers(WorkerStat_Ptr& workstat){

        if (!forkPrepare(workstat)) {
            BOOST_LOG_T(error) << "fork_prepare worker failed, push to dead_workers!";
            ++ workstat->restart_err_cnt;
            dead_workers_.insert(workstat);
            return false;
        }

        pid_t worker_pid = spawn_workers_internel(workstat);

        // parent process
        if (worker_pid < 0) {
            BOOST_LOG_T(error) << "start worker failed, push to dead_workers!";
            ++ workstat->restart_err_cnt;
            dead_workers_.insert(workstat);
            return false;
        }

        ++ workstat->restart_cnt;
        assert(workers_.find(worker_pid) == workers_.end());
        workers_[worker_pid] = workstat;

        BOOST_LOG_T(debug) << "start worker OK for " << workstat->worker->proc_title_ <<
            ", with pid=" << worker_pid ;
        return true;
    }

    void masterLoop() {

    	for( ; ; ) {
            ::sleep(1);
        }

    }

    ~Master() = default;

    bool userInitProc() {
        for (const auto& it: init_list_){
            if (!it())
                return false;
        }

        BOOST_LOG_T(info) << "User Init OK!";
        return true;
    }

    void showAllStat() {
        std::cerr << "forkp status info " << std::endl;
        std::cerr << "active workers:" << std::endl;
        if (workers_.empty())
            std::cerr << "None" << std::endl;
        for (const auto &item: workers_) {
            std::cerr << boost::format("[%c]proc:%s, pid:%d, start_tm:%lu, this_start_tm:%lu, restart_cnt:%lu ")
            % (item.second->worker->type_ == WorkerType::WorkerProcess ? 'P':'E') %
                item.second->worker->proc_title_ % item.second->this_pid % item.second->start_tm %
                item.second->this_start_tm % item.second->restart_cnt << std::endl;
        }
        std::cerr << "dead workers:" << std::endl;
        if (dead_workers_.empty())
            std::cerr << "None" << std::endl;
        for (const auto &item: dead_workers_) {
            std::cerr << boost::format("[%c]proc:%s, pid:%d, start_tm:%lu, this_start_tm:%lu, restart_cnt:%lu ")
            % (item->worker->type_ == WorkerType::WorkerProcess ? 'P':'E') %
                item->worker->proc_title_ % item->this_pid % item->start_tm %
                item->this_start_tm % item->restart_cnt << std::endl;
        }
    }

private:
    bool forkPrepare(WorkerStat_Ptr& workstat) {
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

            if (workstat->worker->type_ == WorkerType::WorkerProcess) {
                workstat->worker->startProcess();
            }
            else {
                workstat->worker->startExec();
            }
        }

        workstat->this_pid = pid;
        workstat->this_start_tm = time(NULL);

        // parent process continue
        return pid;
    }

private:
    static void createInstance() {
        static Master master;
        master.Init();
        master_instance_ = &master;
    }

    Master(): workers_(), dead_workers_(), init_list_()
    {}

    bool Init() {
    	boost_log_init("forkpRun");
        backtrace_init();
        signal_init();

        st_rename_process(name_);
        return true;
    }

    // defined at signal.cpp
    static Master* master_instance_;

private:
    const char* name_ = "forkp master";
    std::map<pid_t, WorkerStat_Ptr> workers_;
    std::set<WorkerStat_Ptr>        dead_workers_;  //没有启动成功的任务
    std::vector<InitFunc> init_list_;  // container set not supportted
};

}



#endif // _MASTER_HPP_
