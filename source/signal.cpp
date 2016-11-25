#include "general.hpp"

#include <boost/log/core.hpp>
#include <boost/log/trivial.hpp>
#include <boost/log/expressions.hpp>
#include <boost/log/sinks/text_file_backend.hpp>
#include <boost/log/utility/setup/file.hpp>

#include <boost/log/utility/setup/common_attributes.hpp>
#include <boost/log/attributes/timer.hpp>
#include <boost/log/attributes/named_scope.hpp>
#include <boost/log/support/date_time.hpp>

#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>

#include <execinfo.h>
#include <signal.h>

#include <sys/wait.h>

#include <set>
#include "master.hpp"

namespace forkp {

    #define MasterIntance (forkp::Master::getInstance())
    Master* Master::master_instance_ = nullptr;

    void signalHander(int signo) {
        if (signo == SIGCHLD) {
            int stat;
            pid_t pid = waitpid(-1, &stat, WNOHANG);
            if (pid == 0)
                return;

            if (pid < 0)
            {
                BOOST_LOG_T(error) << "SIGCHLD wait error!";
                return;
            }

            if (WIFEXITED(stat))
                BOOST_LOG_T(debug) << "child process exit normal!";
            else
                BOOST_LOG_T(error) << "child process exit not normal!";

            WorkerStat_Ptr workstat = MasterIntance.getWorkStatObj(pid);
            if (!workstat) {
                BOOST_LOG_T(error) << "get child process obj failed => " << pid;
                return;
            }

            BOOST_LOG_T(debug) << "respown child process... ";
            MasterIntance.trimWorkStatObj(pid);
            MasterIntance.trySpawnWorkers(workstat);
        }
        else if (signo == SIGUSR1) {
            MasterIntance.showAllStat();
        }
        return;
    }

    /* signal init */
    struct signal_t {
        int signo;
        const char* desc;
        //void (*sighandler_t)(int)
        sighandler_t handler;
    };

    static std::vector<signal_t> signal_list {
        {SIGUSR1, "USR1: print info", signalHander},
        {SIGCHLD, "SIGCHLD: restart process", signalHander},
        {SIGPIPE, "PIPE: SIG_IGN", SIG_IGN},
    };

    extern void signal_init() {
        for (const auto& cit : signal_list) {
            BOOST_LOG_T(debug) << "Signal Hook for " << cit.desc;
            ::signal(cit.signo, cit.handler);
        }

        BOOST_LOG_T(info) << "Signal Init OK!";
        return;
    }

    static void backtrace_info(int sig, siginfo_t *info, void *f)
    {
        int j, nptrs;
    #define BT_SIZE 100
        char **strings;
        void *buffer[BT_SIZE];

        fprintf(stderr,       "\nSignal [%d] received.\n", sig);
        BOOST_LOG_T(fatal) << "\nSignal [" << sig << "] received.\n";
        fprintf(stderr,       "======== Stack trace ========");
        BOOST_LOG_T(fatal) << "======== Stack trace ========\n";

        nptrs = ::backtrace(buffer, BT_SIZE);
        BOOST_LOG_T(fatal) << "backtrace() returned %d addresses";
        fprintf(stderr,       "backtrace() returned %d addresses\n", nptrs);

        strings = ::backtrace_symbols(buffer, nptrs);
        if (strings == NULL)
        {
            perror("backtrace_symbols");
            BOOST_LOG_T(fatal) << "backtrace_symbols";
            exit(EXIT_FAILURE);
        }

        for (j = 0; j < nptrs; j++)
        {
            fprintf(stderr, "%s\n", strings[j]);
            BOOST_LOG_T(fatal) << strings[j];
        }

        free(strings);

        fprintf(stderr,       "Stack Done!\n");
        BOOST_LOG_T(fatal) << "Stack Done!";

        ::kill(getpid(), sig);
        ::abort();

    #undef BT_SIZE
    }

    extern void backtrace_init() {

        struct sigaction act;

        sigemptyset(&act.sa_mask);
        act.sa_flags     = SA_NODEFER | SA_ONSTACK | SA_RESETHAND | SA_SIGINFO;
        act.sa_sigaction = backtrace_info;
        sigaction(SIGABRT, &act, NULL);
        sigaction(SIGBUS,  &act, NULL);
        sigaction(SIGFPE,  &act, NULL);
        sigaction(SIGSEGV, &act, NULL);

        BOOST_LOG_T(info) << "Backtrace Init OK!";
        return;
    }

}

