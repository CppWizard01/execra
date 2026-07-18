#ifndef EXEC_HPP
#define EXEC_HPP

#include <vector>
#include <string>
#include <termios.h>
#include "types.hpp"

namespace Exec {

    std::vector<char*> getArgs(std::vector<std::string>& args);

    int runPipeline(PipelineJob& job, struct termios& orig,
                     std::vector<BgJob>& jobs_list, int& nextJobId);

    int continueForeground(BgJob& job, struct termios& orig, std::vector<BgJob>& jobs_list);

    void continueBackground(BgJob& job);

    bool killJob(pid_t pgid);

}

#endif
