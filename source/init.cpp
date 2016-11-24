#include "general.hpp"
#include "forkp.hpp"

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

#include <vector>

namespace forkp {

    static void boost_log_init(const string prefix);
    extern void backtrace_init();
    extern void signal_init();
    static bool user_init();


    extern bool forkp_init() {
        // ignore sigpipe
        ::signal(SIGPIPE, SIG_IGN);

        boost_log_init("forkpRun");
        backtrace_init();
        signal_init();

        if (!user_init())
            return false;

        BOOST_LOG_T(info) << "Forkp System Init OK!";
        return true;
    }

    /* user specified init*/
    std::vector<InitFunc> init_list;  // set not supportted
    extern bool user_init_register(const InitFunc& func){
        init_list.emplace_back(func);
        return true;
    }

    static bool user_init() {
        for (const auto& it: init_list){
            if (!it())
                return false;
        }

        BOOST_LOG_T(info) << "User Init OK!";
        return true;
    }


namespace blog_sink = boost::log::sinks;
namespace blog_expr = boost::log::expressions;
namespace blog_keyw = boost::log::keywords;
namespace blog_attr = boost::log::attributes;

    static void boost_log_init(const string prefix) {
        boost::log::add_common_attributes();
        //boost::log::core::get()->add_global_attribute("Scope",  blog_attr::named_scope());
        boost::log::core::get()->add_global_attribute("Uptime", blog_attr::timer());

        boost::log::add_file_log(
            blog_keyw::file_name = prefix+"_%N.log",
            blog_keyw::time_based_rotation =
                    blog_sink::file::rotation_at_time_point(0, 0, 0),
            blog_keyw::open_mode = std::ios_base::app,
            blog_keyw::format = blog_expr::stream
               // << std::setw(7) << std::setfill(' ') << blog_expr::attr< unsigned int >("LineID") << std::setfill(' ') << " | "
                << "["   << blog_expr::format_date_time< boost::posix_time::ptime >("TimeStamp", "%Y-%m-%d, %H:%M:%S.%f")
                << "] [" << blog_expr::format_date_time< blog_attr::timer::value_type >("Uptime", "%O:%M:%S")
               // << "] [" << blog_expr::format_named_scope("Scope", blog_keyw::format = "%n (%F:%l)")
                << "] <"  << boost::log::trivial::severity << "> "
                << blog_expr::message,
            blog_keyw::auto_flush = true
            );

        // trace debug info warning error fatal
        boost::log::core::get()->set_filter (
            boost::log::trivial::severity >= boost::log::trivial::trace);

        BOOST_LOG_T(info) << "BOOST LOG V2 Initlized OK!";

        return;
    }

}

