#ifndef _WORKER_HPP_
#define _WORKER_HPP_

#include "general.hpp"
#include <sys/types.h>
#include <sys/socket.h>

#include <unistd.h>
#include <cassert>

namespace forkp {

typedef struct {
	int read_;
	int write_;
} Notify;

class Master;
typedef function<void()> taskFunc;

extern bool st_rename_process(const char* name);

enum class WorkerType {
    WorkerProcess = 1,
    WorkerExec = 2,
};
class Worker {

    friend class Master;

public:
    Worker(const char* name, const taskFunc& func):
        type_(WorkerType::WorkerProcess),
        cwd_(nullptr),
        exec_(nullptr),
        exec_argv_(nullptr),
        func_(func),
        pid_ ( getpid() ),
        ppid_ ( getpid() ),
        channel_( {-1, -1} )
    {
        strncpy(proc_title_, name, 16);
        proc_title_[sizeof(proc_title_)-1] = 0;
    }

    Worker(const char* name, const char* cwd,
           const char* const exec, char *const *argv):
        type_(WorkerType::WorkerExec),
        cwd_(cwd),
        exec_(exec),
        exec_argv_(argv),
        func_(taskFunc()),
        pid_ ( getpid() ),
        ppid_ ( getpid() ),
        channel_( {-1, -1} )
    {
        strncpy(proc_title_, name, 16);
        proc_title_[sizeof(proc_title_)-1] = 0;
    }


    virtual ~Worker(){
        workerReset();
    }

    // 重置状态
    void workerReset() {
        pid_ = ppid_ = getpid();

        if (channel_.read_ != -1)
            close(channel_.read_);
        if (channel_.write_ != -1)
            close(channel_.write_);

        notify_.read_ = notify_.write_ = -1;

        return;
    }

    // 使用函数开辟进程
    void startProcess() {

        assert(pid_ == getppid());
        pid_ = getpid();
        st_rename_process(proc_title_);

        func_();
    }

    // 使用外部exec开辟进程
    void startExec() {
        assert(pid_ == getppid());
        pid_ = getpid();

        // Can not handle exec rename ... but just exec argv[1] specify it!
        // st_rename_process(proc_title_);
        if (cwd_){
            BOOST_LOG_T(info) << "Changing working dir to " << cwd_;
            ::chdir(cwd_);
        }
        ::execv(exec_, exec_argv_);
    }

private:
    char proc_title_[16];
    WorkerType type_;

    const char* cwd_;
    const char* exec_;
    char *const *exec_argv_;
    const taskFunc func_;

    pid_t pid_;
    pid_t ppid_;
    Notify notify_;
    Notify channel_;  // STDOUt/STDERR redirect
};

typedef shared_ptr<Worker> Worker_Ptr;

}



#endif // _WORKER_HPP_
