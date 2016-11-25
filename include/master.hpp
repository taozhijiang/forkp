#ifndef _MASTER_HPP_
#define _MASTER_HPP_

#include "general.hpp"
#include "worker.hpp"
#include "epoll.hpp"

#include <map>
#include <set>
#include <sys/types.h>
#include <unistd.h>

#include <boost/format.hpp>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

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

typedef function<bool()> InitFunc;
typedef function<void()> taskFunc;
typedef function<bool()> epollHandler;

extern bool st_rename_process(const char* name);
extern bool st_transform_to_fd(int src,  int des);

typedef struct {
    pid_t        this_pid;
    time_t       this_start_tm;
    std::size_t  this_miss_cnt; // 当前启动时候错过的心跳

    pid_t        start_pid;       //最初始的pid
    time_t       start_tm;
    std::size_t  restart_cnt;     // 启动成功次数
    std::size_t  restart_err_cnt; // 启动失败次数

    int          out_log_fd;

    Worker_Ptr   worker;
} WorkerStat_t;

typedef shared_ptr<WorkerStat_t> WorkerStat_Ptr;

class Master{

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
            return WorkerStat_Ptr();

        return workers_[pid];
    }

    bool trimWorkStatObj(pid_t pid) {
        if (workers_.find(pid) == workers_.end())
            return false;
        return !!workers_.erase(pid);
    }

    bool spawnWorkers( const char* name, const char* cwd,
                        const char* exec, char *const *argv) {
        Worker_Ptr node = make_shared<Worker>(name, cwd, exec, argv);
        WorkerStat_Ptr workstat = make_shared<WorkerStat_t>();
        if (!node || !workstat)
            return false;

        // 首次启动参数
        workstat->start_tm = ::time(NULL);
        workstat->restart_cnt = 0;
        workstat->restart_err_cnt = 0;
        workstat->out_log_fd = -1;
        workstat->worker = node;

        return trySpawnWorkers(workstat);
    }

    // 用户空间启动进程
    bool spawnWorkers(const char* name, const taskFunc& func){
        Worker_Ptr node = make_shared<Worker>(name, func);
        WorkerStat_Ptr workstat = make_shared<WorkerStat_t>();
        if (!node || !workstat)
            return false;

        // 首次启动参数
        workstat->start_tm = ::time(NULL);
        workstat->restart_cnt = 0;
        workstat->restart_err_cnt = 0;
        workstat->out_log_fd = -1;
        workstat->worker = node;

        return trySpawnWorkers(workstat);
    }

    // 内部重启进程的接口
    bool trySpawnWorkers(WorkerStat_Ptr& workstat){
        bool ret = false;

        if (!forkPrepare(workstat)) {
            BOOST_LOG_T(error) << "fork_prepare worker failed, push to dead_workers!";
            ++ workstat->restart_err_cnt;
            dead_workers_.insert(workstat);
            return false;
        }

        pid_t worker_pid = spawn_workers_internel(workstat);

        // parent process routine

        if (worker_pid < 0) {
            BOOST_LOG_T(error) << "start worker failed, push to dead_workers!";
            ++ workstat->restart_err_cnt;
            dead_workers_.insert(workstat);
            return false;
        }


        close(workstat->worker->channel_.write_);
        workstat->worker->channel_.write_ = -1;

        if (!workstat->start_pid)
            workstat->start_pid = worker_pid;

        if (workstat->out_log_fd < 0) {
            char file[PATH_MAX];
            snprintf(file, PATH_MAX, "./log/%s_%d.log", workstat->worker->proc_title_,
                     workstat->start_pid);
            workstat->out_log_fd = open(file, O_CREAT|O_WRONLY|O_APPEND, 0640);
            if (workstat->out_log_fd < 0)
                BOOST_LOG_T(error) << "redirect target child process stdout/stderr to " << file << " failed!";
        }

        if (workstat->out_log_fd > 0) {
            ret = epollh_->addEvent(workstat->worker->channel_.read_,
                                EPOLLIN | EPOLLERR,
                                bind(st_transform_to_fd,
                                     workstat->worker->channel_.read_,
                                     workstat->out_log_fd));
            if (!ret)
                BOOST_LOG_T(error) << "register failed for " << workstat->worker->channel_.read_;
        }

        ++workstat->restart_cnt;
        assert(workers_.find(worker_pid) == workers_.end());
        workers_[worker_pid] = workstat;

        BOOST_LOG_T(debug) << "start worker OK for " << workstat->worker->proc_title_ <<
            ", with pid=" << worker_pid ;
        return true;
    }

    void masterLoop() {

    	for( ; ; ) {
            epollh_->traverseAndHandleEvent(200);
        }

    }

    ~Master() = default;

    bool userInitProc() {
        std::vector<InitFunc>::const_iterator it;
        for (it = init_list_.cbegin(); it != init_list_.cend(); ++it){
            if (!(*it)())
                return false;
        }

        BOOST_LOG_T(info) << "User Init OK!";
        return true;
    }

    void showAllStat() {
        std::cerr << "!!!! forkp status info !!!!" << std::endl;
        std::cerr << "!!!! active workers:" << std::endl;
        if (workers_.empty())
            std::cerr << "None" << std::endl;
        std::map<pid_t, WorkerStat_Ptr>::const_iterator m_it;
        for (m_it = workers_.cbegin(); m_it != workers_.cend(); ++m_it) {
            std::cerr << boost::format("[%c]proc:%s, pid:%d, start_pid:%lu, start_tm:%lu, this_start_tm:%lu, restart_cnt:%lu ")
            % (m_it->second->worker->type_ == WorkerType::WorkerProcess ? 'P':'E') %
                m_it->second->worker->proc_title_ % m_it->second->this_pid % m_it->second->start_pid %
                m_it->second->start_tm %
                m_it->second->this_start_tm % m_it->second->restart_cnt << std::endl;
        }
        std::cerr << "!!!! dead workers:" << std::endl;
        if (dead_workers_.empty())
            std::cerr << "None" << std::endl;
        std::set<WorkerStat_Ptr>::const_iterator s_it;
        for (s_it = dead_workers_.cbegin(); s_it != dead_workers_.cend(); ++s_it) {
            std::cerr << boost::format("[%c]proc:%s, pid:%d, start_pid:%lu, start_tm:%lu, this_start_tm:%lu, restart_cnt:%lu ")
            % ((*s_it)->worker->type_ == WorkerType::WorkerProcess ? 'P':'E') %
                (*s_it)->worker->proc_title_ % (*s_it)->this_pid % (*s_it)->start_pid % (*s_it)->start_tm %
                (*s_it)->this_start_tm % (*s_it)->restart_cnt << std::endl;
        }
    }

private:
    bool forkPrepare(WorkerStat_Ptr& workstat) {
        int pipefd[2];

        workstat->worker->workerReset();

        // channel_，用于子进程的STDOUT STDERR的重定向

        if ( pipe(pipefd) < 0 ) {
            BOOST_LOG_T(error) << "pipe() Error!";
            return false;
        }

        st_make_nonblock(pipefd[0]);
        st_make_nonblock(pipefd[1]);

        workstat->worker->channel_.read_ = pipefd[0];
        workstat->worker->channel_.write_ = pipefd[1];
        workstat->this_miss_cnt = 0;

        return true;
    }

    // -1 for error case
    pid_t spawn_workers_internel(WorkerStat_Ptr& workstat)
    {
        time_t now = ::time(NULL);
        // 避免过快的fork()
        if (now - workstat->this_start_tm < 2)
            ::sleep(1);

        pid_t pid = fork();
        if (pid < 0) {
            BOOST_LOG_T(error) << "Fork() Error!";
            return -1;
        }

        if (pid == 0) // child process
        {

            // 重定向子进程的输出、标准错误输出到channel
            int ret = 0;
            close(workstat->worker->channel_.read_);
            workstat->worker->channel_.read_ = -1;

            ret = ::dup2(workstat->worker->channel_.write_, STDOUT_FILENO);
            ret += ::dup2(workstat->worker->channel_.write_, STDERR_FILENO);

            if (ret < 0){
                BOOST_LOG_T(error) << "dup2 STDOUT_FILENO STDERR_FILENO  Error!";
            }

            if (workstat->worker->type_ == WorkerType::WorkerProcess) {
                workstat->worker->startProcess();
            }
            else {
                workstat->worker->startExec();
            }
        }

        // parent process continue

        workstat->this_pid = pid;
        workstat->this_start_tm = ::time(NULL);

        return pid;
    }

private:
    static void createInstance() {
        static Master master;
        master.Init();
        master_instance_ = &master;
    }

    Master():
        name_("forkp master"),
        workers_(), dead_workers_(), init_list_(),
        epollh_(make_shared<Epoll>(128))
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
    const char* name_;
    std::map<pid_t, WorkerStat_Ptr> workers_;
    std::set<WorkerStat_Ptr>        dead_workers_;  //没有启动成功的任务
    std::vector<InitFunc> init_list_;  // container set not supportted
    shared_ptr<Epoll> epollh_;
};

}



#endif // _MASTER_HPP_
