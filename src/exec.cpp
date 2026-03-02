#include "exec.hpp"
#include <iostream>
#include <unistd.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <csignal>

using namespace std;

vector<char*> Exec::getArgs(vector<string>& args) {
    vector<char*> c_args;
    for(auto& arg: args) {
        c_args.push_back(const_cast<char*>(arg.c_str()));
    }
    c_args.push_back(nullptr);
    return c_args;
}

void Exec::handleRedirections(Command& cmd) {
    if (!cmd.heredocContent.empty()) {
        int hpipe[2];
        if (pipe(hpipe) == 0) {
            write(hpipe[1], cmd.heredocContent.c_str(), cmd.heredocContent.size());
            close(hpipe[1]);
            dup2(hpipe[0], STDIN_FILENO);
            close(hpipe[0]);
        } else {
            perror("Heredoc pipe failed");
            exit(1);
        }
    } 
    else if (!cmd.inFile.empty()) {
        int fd = open(cmd.inFile.c_str(), O_RDONLY);
        if (fd < 0) {
            perror("Input redirection failed");
            exit(1);
        }
        dup2(fd, STDIN_FILENO);
        close(fd);
    }

    if (!cmd.outFile.empty()) {
        int flags = O_WRONLY | O_CREAT;
        flags |= cmd.appendOut ? O_APPEND : O_TRUNC;

        int fd = open(cmd.outFile.c_str(), flags, 0644);
        if (fd < 0) {
            perror("Output redirection failed");
            exit(1);
        }
        dup2(fd, STDOUT_FILENO);
        close(fd);
    }
}

void Exec::execSingle(Command& cmd, struct termios& orig) {
    pid_t pid = fork();

    if (pid < 0) {
        perror("Fork failed");
        return;
    }

    if (pid == 0) { 
        tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig);
        signal(SIGINT, SIG_DFL);
        
        handleRedirections(cmd);

        vector<char*> args = getArgs(cmd.args);
        execvp(args[0], args.data());
        
        perror("Execution failed");
        exit(1);
    } 
    else {
        waitpid(pid, nullptr, 0);
    }
}

void Exec::execPipe(vector<Command>& cmds, struct termios& orig) {
    int num_cmds = cmds.size();
    int prev_pipe_fd = -1; 
    vector<pid_t> pids;    

    for (int i = 0; i < num_cmds; i++) {
        int pipefd[2];

        if (i < num_cmds - 1) {
            if (pipe(pipefd) == -1) {
                perror("Pipe creation failed");
                return;
            }
        }

        pid_t pid = fork();
        if (pid < 0) {
            perror("Fork failed");
            return;
        }

        if (pid == 0) { 
            tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig);
            signal(SIGINT, SIG_DFL);

            if (prev_pipe_fd != -1) {
                dup2(prev_pipe_fd, STDIN_FILENO);
                close(prev_pipe_fd);
            }
            if (i < num_cmds - 1) {
                dup2(pipefd[1], STDOUT_FILENO);
                close(pipefd[0]); 
                close(pipefd[1]);
            }

            handleRedirections(cmds[i]);

            vector<char*> args = getArgs(cmds[i].args);
            execvp(args[0], args.data());
            perror("Execution failed");
            exit(1);
        } 
        else { 
            if (prev_pipe_fd != -1) close(prev_pipe_fd);
            if (i < num_cmds - 1) {
                close(pipefd[1]); 
                prev_pipe_fd = pipefd[0]; 
            }
            pids.push_back(pid);
        }
    }

    for (pid_t p : pids) {
        waitpid(p, nullptr, 0);
    }
}