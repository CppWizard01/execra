#ifndef EXEC_HPP
#define EXEC_HPP

#include <vector>
#include <string>
#include <termios.h>
#include <sys/types.h>

struct Job {
    pid_t pid;
    std::string command;
    bool is_stopped;
};

struct Command {
    std::vector<std::string> args;
    std::string inFile = "";
    std::string outFile = "";
    bool appendOut = false;
    std::string heredocContent = "";
};

class Exec {
private:
    static void handleRedirections(Command& cmd);

public:
    static std::vector<char*> getArgs(std::vector<std::string>& args);
    static void execSingle(Command& cmd, struct termios& orig, std::vector<Job>& jobs_list, bool bg);
    static void execPipe(std::vector<Command>& cmds, struct termios& orig, std::vector<Job>& jobs_list, bool bg);
};

#endif