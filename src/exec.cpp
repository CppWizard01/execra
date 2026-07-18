#include "exec.hpp"
#include <iostream>
#include <unistd.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <csignal>
#include <cstring>

using namespace std;

namespace {

void resetChildSignals() {
    signal(SIGINT, SIG_DFL);
    signal(SIGQUIT, SIG_DFL);
    signal(SIGTSTP, SIG_DFL);
    signal(SIGTTIN, SIG_DFL);
    signal(SIGTTOU, SIG_DFL);
}

void applyRedirections(Command& cmd) {
    if (cmd.hasHeredoc) {
        int hpipe[2];
        if (pipe(hpipe) == 0) {
            ssize_t written = write(hpipe[1], cmd.heredocContent.c_str(), cmd.heredocContent.size());
            (void)written;
            close(hpipe[1]);
            dup2(hpipe[0], STDIN_FILENO);
            close(hpipe[0]);
        } else {
            perror("execra: heredoc pipe");
            exit(1);
        }
    } else if (!cmd.inFile.empty()) {
        int fd = open(cmd.inFile.c_str(), O_RDONLY);
        if (fd < 0) { perror(("execra: " + cmd.inFile).c_str()); exit(1); }
        dup2(fd, STDIN_FILENO);
        close(fd);
    }

    if (!cmd.outFile.empty()) {
        int flags = O_WRONLY | O_CREAT | (cmd.appendOut ? O_APPEND : O_TRUNC);
        int fd = open(cmd.outFile.c_str(), flags, 0644);
        if (fd < 0) { perror(("execra: " + cmd.outFile).c_str()); exit(1); }
        dup2(fd, STDOUT_FILENO);
        close(fd);
    }
}

int waitForeground(const vector<pid_t>& pids, bool& stopped) {
    stopped = false;
    int status = 0;
    for (pid_t pid : pids) {
        int st;
        waitpid(pid, &st, WUNTRACED);
        if (WIFSTOPPED(st)) stopped = true;
        status = st;
    }
    return status;
}

int statusToExitCode(int status) {
    if (WIFEXITED(status)) return WEXITSTATUS(status);
    if (WIFSIGNALED(status)) return 128 + WTERMSIG(status);
    if (WIFSTOPPED(status)) return 128 + WSTOPSIG(status);
    return 1;
}

}

namespace Exec {

vector<char*> getArgs(vector<string>& args) {
    vector<char*> c_args;
    for (auto& arg : args) c_args.push_back(arg.data());
    c_args.push_back(nullptr);
    return c_args;
}

int runPipeline(PipelineJob& job, struct termios& orig, vector<BgJob>& jobs_list, int& nextJobId) {
    size_t n = job.pipeline.size();
    vector<int> pipeFds(n > 1 ? (n - 1) * 2 : 0);

    for (size_t i = 0; i + 1 < n; i++) {
        int fd[2];
        if (pipe(fd) == -1) { perror("execra: pipe"); return 1; }
        pipeFds[i * 2] = fd[0];
        pipeFds[i * 2 + 1] = fd[1];
    }

    vector<pid_t> pids;
    pid_t pgid = -1;

    for (size_t i = 0; i < n; i++) {
        Command& cmd = job.pipeline[i];
        pid_t pid = fork();

        if (pid == 0) {
            if (i == 0) setpgid(0, 0);
            else setpgid(0, pgid);

            tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig);
            resetChildSignals();

            if (i > 0) dup2(pipeFds[(i - 1) * 2], STDIN_FILENO);
            if (i + 1 < n) dup2(pipeFds[i * 2 + 1], STDOUT_FILENO);
            for (int fd : pipeFds) close(fd);

            applyRedirections(cmd);

            vector<char*> args = getArgs(cmd.args);
            execvp(args[0], args.data());
            cerr << "execra: " << cmd.args[0] << ": command not found\n";
            exit(127);
        } else if (pid > 0) {
            if (i == 0) pgid = pid;
            setpgid(pid, pgid);
            pids.push_back(pid);
        } else {
            perror("execra: fork");
        }
    }

    for (int fd : pipeFds) close(fd);

    if (job.background) {
        BgJob bj;
        bj.pids = pids;
        bj.remaining = (int)pids.size();
        bj.pgid = pgid;
        bj.text = job.text;
        bj.id = nextJobId++;
        cout << "[" << bj.id << "] " << bj.pgid << "\n";
        jobs_list.push_back(bj);
        return 0;
    }

    if (pids.empty()) return 1;

    tcsetpgrp(STDIN_FILENO, pgid);
    bool stopped = false;
    int status = waitForeground(pids, stopped);
    tcsetpgrp(STDIN_FILENO, getpid());

    if (stopped) {
        cout << "\n[Stopped]  " << job.text << "\n";
        BgJob bj;
        bj.pids = pids;
        bj.remaining = (int)pids.size();
        bj.pgid = pgid;
        bj.text = job.text;
        bj.id = nextJobId++;
        bj.is_stopped = true;
        jobs_list.push_back(bj);
    }

    return statusToExitCode(status);
}

int continueForeground(BgJob& job, struct termios&, vector<BgJob>& jobs_list) {
    job.is_stopped = false;
    kill(-job.pgid, SIGCONT);

    tcsetpgrp(STDIN_FILENO, job.pgid);
    bool stopped = false;
    int status = waitForeground(job.pids, stopped);
    tcsetpgrp(STDIN_FILENO, getpid());

    if (stopped) {
        cout << "\n[Stopped]  " << job.text << "\n";
        job.is_stopped = true;
        jobs_list.push_back(job);
    }

    return statusToExitCode(status);
}

void continueBackground(BgJob& job) {
    job.is_stopped = false;
    kill(-job.pgid, SIGCONT);
    cout << "[" << job.id << "] " << job.text << " &\n";
}

bool killJob(pid_t pgid) {
    return kill(-pgid, SIGTERM) == 0;
}

}
