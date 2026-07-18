#include "shell.hpp"
#include "parser.hpp"
#include "exec.hpp"
#include "colors.hpp"
#include <iostream>
#include <filesystem>
#include <algorithm>
#include <unistd.h>
#include <sys/wait.h>
#include <csignal>
#include <cstdlib>
#include <stdexcept>

using namespace std;
namespace fs = std::filesystem;

Shell::Shell() {
    history.load();
}

Shell::~Shell() {}

void Shell::p_logo() {
    cout << CYAN << R"(
  #######  ##   ##  #######  ######   ######     ###
  ##        ## ##   ##      ##    ##  ##   ##   ## ##
  #####      ###    #####   ##        ######   #######
  ##        ## ##   ##      ##    ##  ##   ##  ##   ##
  #######  ##   ##  #######  ######   ##   ##  ##   ##
)" << RESET << endl;
    cout << ORANGE << "v2.0.0" << RESET << "\n\n";
}

string Shell::prompt() {
    return string(GREEN) + fs::current_path().string() + RESET + ORANGE + "$ " + RESET;
}

void Shell::reapJobs() {
    int status;
    pid_t pid;
    while ((pid = waitpid(-1, &status, WNOHANG | WUNTRACED | WCONTINUED)) > 0) {
        for (auto it = jobs_list.begin(); it != jobs_list.end(); ++it) {
            auto pit = find(it->pids.begin(), it->pids.end(), pid);
            if (pit == it->pids.end()) continue;

            if (WIFEXITED(status) || WIFSIGNALED(status)) {
                it->remaining--;
                if (it->remaining <= 0) {
                    cout << ORANGE << "[" << it->id << "]+  "
                         << (WIFSIGNALED(status) ? "Terminated" : "Done") << RESET
                         << "    " << it->text << "\n";
                    jobs_list.erase(it);
                }
            } else if (WIFSTOPPED(status)) {
                it->is_stopped = true;
            } else if (WIFCONTINUED(status)) {
                it->is_stopped = false;
            }
            break;
        }
    }
}

static bool readRawLine(string& out) {
    out.clear();
    char c;
    ssize_t n;
    while ((n = read(STDIN_FILENO, &c, 1)) > 0) {
        if (c == '\n') return true;
        out += c;
    }
    return n > 0 || !out.empty();
}

string Shell::readHeredoc(const string& delimiter) {
    string heredoc, line;
    while (true) {
        cout << "> ";
        cout.flush();
        if (!readRawLine(line)) break;
        if (line == delimiter) break;
        heredoc += line + "\n";
    }
    return heredoc;
}

bool Shell::runBuiltin(vector<string>& args, int& status) {
    const string& cmd = args[0];

    if (cmd == "cd") {
        string target;
        if (args.size() > 1) {
            if (args[1] == "-") {
                if (prevDir.empty()) { cerr << "execra: cd: OLDPWD not set\n"; status = 1; return true; }
                target = prevDir;
            } else {
                target = args[1];
            }
        } else {
            const char* home = getenv("HOME");
            if (!home) { cerr << "execra: cd: HOME not set\n"; status = 1; return true; }
            target = home;
        }
        string cur = fs::current_path().string();
        if (chdir(target.c_str()) != 0) {
            perror(("execra: cd: " + target).c_str());
            status = 1;
        } else {
            prevDir = cur;
            status = 0;
        }
        return true;
    }

    if (cmd == "history") {
        for (size_t i = 0; i < history.entries.size(); i++)
            cout << " " << i + 1 << "  " << history.entries[i] << "\n";
        status = 0;
        return true;
    }

    if (cmd == "help") {
        cout << CYAN << "\nExecra Shell\n" << RESET
             << "Builtins: cd [-|dir], history, jobs, fg [%n], bg [%n], "
                "kill <pid|%n>, exit [code]\n";
        status = 0;
        return true;
    }

    if (cmd == "jobs") {
        for (auto& j : jobs_list)
            cout << "[" << j.id << "]  " << j.pgid << "  "
                 << (j.is_stopped ? "Stopped" : "Running") << "    " << j.text << "\n";
        status = 0;
        return true;
    }

    if (cmd == "fg" || cmd == "bg") {
        if (jobs_list.empty()) {
            cerr << "execra: " << cmd << ": current: no such job\n";
            status = 1;
            return true;
        }
        int idx = (int)jobs_list.size() - 1;
        if (args.size() > 1 && !args[1].empty() && args[1][0] == '%') {
            try { idx = stoi(args[1].substr(1)) - 1; }
            catch (...) { idx = -1; }
        }
        if (idx < 0 || idx >= (int)jobs_list.size()) {
            cerr << "execra: " << cmd << ": " << (args.size() > 1 ? args[1] : "current")
                 << ": no such job\n";
            status = 1;
            return true;
        }

        if (cmd == "bg") {
            Exec::continueBackground(jobs_list[idx]);
            status = 0;
        } else {
            BgJob job = jobs_list[idx];
            jobs_list.erase(jobs_list.begin() + idx);
            cout << job.text << "\n";
            status = Exec::continueForeground(job, input.orig, jobs_list);
        }
        return true;
    }

    if (cmd == "kill") {
        if (args.size() < 2) {
            cerr << "kill: usage: kill <pid> | %<job_id>\n";
            status = 1;
            return true;
        }
        string target = args[1];
        try {
            if (target[0] == '%') {
                int idx = stoi(target.substr(1)) - 1;
                if (idx < 0 || idx >= (int)jobs_list.size()) {
                    cerr << "execra: kill: " << target << ": no such job\n";
                    status = 1;
                    return true;
                }
                status = Exec::killJob(jobs_list[idx].pgid) ? 0 : 1;
            } else {
                pid_t pid = (pid_t)stoi(target);
                status = (kill(pid, SIGTERM) == 0) ? 0 : 1;
            }
        } catch (const exception&) {
            cerr << "execra: kill: " << target << ": invalid argument\n";
            status = 1;
        }
        return true;
    }

    return false;
}

void Shell::run() {
    p_logo();

    while (true) {
        reapJobs();

        input.enableRM();
        string p = prompt();
        auto lineOpt = input.read_line(p, history);
        input.disableRM();

        if (!lineOpt) {
            history.save();
            return;
        }

        string line = *lineOpt;
        if (line.empty()) continue;

        vector<string> tokens;
        vector<pair<PipelineJob, Connector>> jobs;
        try {
            tokens = Parser::tokenize(line);
            if (tokens.empty()) continue;
            auto heredocCb = [this](const string& delim) { return readHeredoc(delim); };
            jobs = Parser::parseLine(tokens, heredocCb);
        } catch (const exception& e) {
            cerr << RED << "execra: " << e.what() << RESET << "\n";
            continue;
        }

        bool exitRequested = false;
        int exitCode = 0;
        Connector pendingConnector = Connector::NONE;

        for (auto& entry : jobs) {
            PipelineJob& job = entry.first;

            bool skip = false;
            if (pendingConnector == Connector::AND && lastStatus != 0) skip = true;
            if (pendingConnector == Connector::OR && lastStatus == 0) skip = true;

            if (!skip) {
                if (job.pipeline.size() == 1 && job.pipeline[0].args[0] == "exit") {
                    exitRequested = true;
                    exitCode = (job.pipeline[0].args.size() > 1)
                                   ? atoi(job.pipeline[0].args[1].c_str())
                                   : lastStatus;
                    pendingConnector = entry.second;
                    break;
                }

                int status = 0;
                bool isBuiltinShape = (job.pipeline.size() == 1 && !job.background &&
                                        job.pipeline[0].inFile.empty() &&
                                        job.pipeline[0].outFile.empty() &&
                                        !job.pipeline[0].hasHeredoc);
                bool handled = isBuiltinShape && runBuiltin(job.pipeline[0].args, status);
                if (!handled) status = Exec::runPipeline(job, input.orig, jobs_list, nextJobId);
                lastStatus = status;
            }

            pendingConnector = entry.second;
        }

        if (exitRequested) {
            history.save();
            exit(exitCode);
        }
    }
}
