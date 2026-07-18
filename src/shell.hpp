#ifndef SHELL_HPP
#define SHELL_HPP

#include <string>
#include <vector>
#include <termios.h>
#include "types.hpp"
#include "history.hpp"
#include "input.hpp"

class Shell {
private:
    History history;
    Input input;

    std::vector<BgJob> jobs_list;
    int nextJobId = 1;
    int lastStatus = 0;
    std::string prevDir;

    void p_logo();
    std::string prompt();
    void reapJobs();

    std::string readHeredoc(const std::string& delimiter);

    bool runBuiltin(std::vector<std::string>& args, int& status);

public:
    Shell();
    ~Shell();

    void run();
};

#endif
