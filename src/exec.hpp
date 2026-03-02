#ifndef EXEC_HPP
#define EXEC_HPP

#include <vector>
#include <string>
#include <termios.h>

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
    static void execSingle(Command& cmd, struct termios& orig);
    static void execPipe(std::vector<Command>& cmds, struct termios& orig);
};

#endif