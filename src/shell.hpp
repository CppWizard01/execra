#ifndef SHELL_HPP
#define SHELL_HPP

#include <iostream>
#include <vector>
#include <string>
#include <termios.h>
#include <filesystem>
#include <sys/types.h>

namespace fs = std::filesystem;

struct Command {
    std::vector<std::string> args;
    std::string inFile;      // '<'
    std::string outFile;     // '>'
    std::string appendFile;  // '>>'
};

struct Job {
    std::vector<Command> pipeline;
    bool background = false;
    std::string text;
};

enum class Connector { NONE, SEQ, AND, OR };

struct BgJob {
    std::vector<pid_t> pids;
    pid_t displayPid;
    std::string text;
    int id;
};

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

    int lastStatus = 0;
    std::string prevDir;
    std::vector<BgJob> bgJobs;
    int nextJobId = 1;

    void disableRM();
    void enableRM();
    void p_logo();
    void loadHistory();
    void saveHistory();
    void reapBackgroundJobs();

    std::vector<char*> getArgs(std::vector<std::string>& args);

    std::vector<std::string> tokenize(const std::string& line);

    std::vector<std::pair<Job, Connector>> parseLine(const std::vector<std::string>& tokens);

    bool parseJob(std::vector<std::string> tokens, Job& job);

    int runJob(Job& job);
    int runPipeline(Job& job, bool background);
    bool runBuiltin(std::vector<std::string>& args, int& status);

public:
    Shell();
    ~Shell();

    void run();
};

#endif
