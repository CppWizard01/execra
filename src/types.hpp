#ifndef TYPES_HPP
#define TYPES_HPP

#include <string>
#include <vector>
#include <sys/types.h>

struct Command {
    std::vector<std::string> args;
    std::string inFile;            // '<'
    std::string outFile;           // '>'
    bool appendOut = false;        // true if outFile came from '>>'
    std::string heredocContent;    // collected body for '<<'
    bool hasHeredoc = false;
};

enum class Connector { NONE, SEQ, AND, OR };

struct PipelineJob {
    std::vector<Command> pipeline;
    bool background = false;
    std::string text;
};

struct BgJob {
    std::vector<pid_t> pids;
    int remaining = 0;
    pid_t pgid = -1;
    std::string text;
    int id = 0;
    bool is_stopped = false;
};

#endif
