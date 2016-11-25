#ifndef _WORKER_HPP_
#define _WORKER_HPP_

#include "general.hpp"
#include <sys/types.h>
#include <unistd.h>

namespace forkp {

typedef struct {
	int read_;
	int write_;
} Notify;

class Master;
using taskFunc = std::function<void()>;

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
        exec_(nullptr),
        exec_argv_(nullptr),
        func_(func),
        pid_ ( getpid() ),
        ppid_ ( getpid() ),
        notify_( {-1, -1} )
    {
        strncpy(proc_title_, name, 16);
        proc_title_[sizeof(proc_title_)-1] = 0;
    }

    Worker(const char* name, const char* const exec, char *const *argv):
        type_(WorkerType::WorkerExec),
        exec_(exec),
        exec_argv_(argv),
        func_(taskFunc()),
        pid_ ( getpid() ),
        ppid_ ( getpid() ),
        notify_( {-1, -1} )
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

        if (notify_.read_ != -1)
            close(notify_.read_);

        if (notify_.write_ != -1)
            close(notify_.write_);

        notify_.read_ = notify_.write_ = -1;
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
        ::execv(exec_, exec_argv_);
    }

private:
    char proc_title_[16];
    WorkerType type_;

    const char* exec_;
    char *const *exec_argv_;
    const taskFunc func_;

    pid_t pid_;
    pid_t ppid_;
    Notify notify_;
};

using Worker_Ptr = std::shared_ptr<Worker>;

}



#endif // _WORKER_HPP_
