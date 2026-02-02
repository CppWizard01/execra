#ifndef SHELL_HPP
#define SHELL_HPP

#include <iostream>
#include <vector>
#include <string>
#include <termios.h>
#include <filesystem>

namespace fs = std::filesystem;

class Shell {
private:

    const std::string GREEN = "\033[1;32m";
    const std::string ORANGE = "\033[38;5;208m";
    const std::string CYAN = "\033[36m";
    const std::string RED = "\033[31m";
    const std::string RESET = "\033[0m";

    std::vector<std::string> history;
    int h_ind = 0;
    struct termios orig;

    void disableRM();
    void enableRM();
    void p_logo();

    std::vector<char*> getArgs(std::vector<std::string>& args);
    void execCommand(std::vector<std::string>& user_ip, std::string opFile = "");
    void execPipe(std::vector<std::string>& cmd1, std::vector<std::string>& cmd2);

public:
    Shell();
    ~Shell();

    void run();
};

#endif