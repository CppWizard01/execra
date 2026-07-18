#include "shell.hpp"
#include <iostream>
#include <vector>
#include <string>
#include <unistd.h>
#include <sys/wait.h>
#include <sstream>
#include <filesystem>
#include <fcntl.h>
#include <algorithm>
#include <csignal>
#include <termios.h>
#include <cstdlib>
#include <fstream>
#include <stdexcept>

using namespace std;
namespace fs = filesystem;

static const size_t HISTORY_LIMIT = 1000;

Shell::Shell(){
    h_ind = 0;
    loadHistory();
}

Shell::~Shell(){
    disableRM();
}

void Shell::disableRM(){
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig);
}

void Shell::enableRM(){
        tcgetattr(STDIN_FILENO, &orig);
        struct termios raw = orig;

        raw.c_lflag &= ~(ECHO | ICANON | ISIG);
        tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
}

void Shell::p_logo(){
        cout << CYAN << R"(
  #######  ##   ##  #######  ######   ######     ###
  ##        ## ##   ##      ##    ##  ##   ##   ## ##
  #####      ###    #####   ##        ######   #######
  ##        ## ##   ##      ##    ##  ##   ##  ##   ##
  #######  ##   ##  #######  ######   ##   ##  ##   ##
)" << RESET << endl;
        cout << ORANGE << "v1.1.0" << RESET << "\n\n";
}

static string historyFilePath(){
    const char* home = getenv("HOME");
    if (!home) return "";
    return string(home) + "/.execra_history";
}

void Shell::loadHistory(){
    string path = historyFilePath();
    if (path.empty()) return;
    ifstream in(path);
    if (!in.is_open()) return;
    string line;
    while (getline(in, line)) {
        if (!line.empty()) history.push_back(line);
    }
    h_ind = history.size();
}

void Shell::saveHistory(){
    string path = historyFilePath();
    if (path.empty()) return;
    ofstream out(path, ios::trunc);
    if (!out.is_open()) return;
    size_t start = (history.size() > HISTORY_LIMIT) ? history.size() - HISTORY_LIMIT : 0;
    for (size_t i = start; i < history.size(); i++) out << history[i] << "\n";
}

void Shell::reapBackgroundJobs(){
    for (auto it = bgJobs.begin(); it != bgJobs.end(); ) {
        bool allDone = true;
        for (pid_t pid : it->pids) {
            int status;
            pid_t r = waitpid(pid, &status, WNOHANG);
            if (r == 0) allDone = false;
        }
        if (allDone) {
            cout << ORANGE << "[" << it->id << "]+  Done" << RESET << "    " << it->text << "\n";
            it = bgJobs.erase(it);
        } else {
            ++it;
        }
    }
}

vector<char*> Shell::getArgs(vector<string>& args){
        vector<char*> c_args;
        for(auto& arg: args){
            c_args.push_back(arg.data());
        }
        c_args.push_back(nullptr);
        return c_args;
}

vector<string> Shell::tokenize(const string& line){
    vector<string> tokens;
    string cur;
    bool inWord = false;
    size_t i = 0, n = line.size();

    auto flush = [&](){
        if (inWord) { tokens.push_back(cur); cur.clear(); inWord = false; }
    };

    auto isOperatorChar = [](char c){
        return c == '|' || c == '>' || c == '<' || c == ';' || c == '&';
    };

    while (i < n) {
        char c = line[i];

        if (c == ' ' || c == '\t') { flush(); i++; continue; }

        if (c == '\'' || c == '"') {
            char q = c;
            i++;
            inWord = true;
            while (i < n && line[i] != q) {
                if (q == '"' && line[i] == '\\' && i + 1 < n &&
                    (line[i+1] == '"' || line[i+1] == '\\')) {
                    cur += line[i+1];
                    i += 2;
                    continue;
                }
                cur += line[i];
                i++;
            }
            if (i < n) { i++; }
            else throw runtime_error("unmatched quote");
            continue;
        }

        if (isOperatorChar(c)) {
            flush();
            if (c == '&' && i + 1 < n && line[i+1] == '&') { tokens.push_back("&&"); i += 2; continue; }
            if (c == '|' && i + 1 < n && line[i+1] == '|') { tokens.push_back("||"); i += 2; continue; }
            if (c == '>' && i + 1 < n && line[i+1] == '>') { tokens.push_back(">>"); i += 2; continue; }
            tokens.push_back(string(1, c));
            i++;
            continue;
        }

        if (c == '\\' && i + 1 < n) { cur += line[i+1]; inWord = true; i += 2; continue; }

        cur += c;
        inWord = true;
        i++;
    }
    flush();
    return tokens;
}

bool Shell::parseJob(vector<string> tokens, Job& job){
    if (tokens.empty()) return false;

    vector<vector<string>> stages;
    vector<string> cur;
    for (auto& t : tokens) {
        if (t == "|") {
            if (cur.empty()) throw runtime_error("syntax error near unexpected token '|'");
            stages.push_back(cur);
            cur.clear();
        } else {
            cur.push_back(t);
        }
    }
    if (cur.empty()) throw runtime_error("syntax error near unexpected token '|'");
    stages.push_back(cur);

    job.pipeline.clear();
    for (auto& stageTokens : stages) {
        Command cmd;
        for (size_t i = 0; i < stageTokens.size(); i++) {
            string& t = stageTokens[i];
            if (t == "<" || t == ">" || t == ">>") {
                if (i + 1 >= stageTokens.size())
                    throw runtime_error("expected filename after '" + t + "'");
                string file = stageTokens[++i];
                if (t == "<") cmd.inFile = file;
                else if (t == ">") cmd.outFile = file;
                else cmd.appendFile = file;
            } else {
                cmd.args.push_back(t);
            }
        }
        if (cmd.args.empty()) throw runtime_error("syntax error: empty command in pipeline");
        job.pipeline.push_back(cmd);
    }
    return true;
}

vector<pair<Job, Connector>> Shell::parseLine(const vector<string>& tokens){
    vector<pair<Job, Connector>> result;
    vector<string> seg;

    auto flushSeg = [&](Connector conn, bool background){
        if (seg.empty()) {
            if (conn == Connector::NONE && !background) return;
            throw runtime_error("syntax error: unexpected token");
        }
        Job job;
        parseJob(seg, job);
        job.background = background;
        for (auto& t : seg) { if (!job.text.empty()) job.text += " "; job.text += t; }
        result.push_back({job, conn});
        seg.clear();
    };

    for (auto& t : tokens) {
        if (t == ";" ) { flushSeg(Connector::SEQ, false); }
        else if (t == "&&") { flushSeg(Connector::AND, false); }
        else if (t == "||") { flushSeg(Connector::OR, false); }
        else if (t == "&") { flushSeg(Connector::SEQ, true); }
        else seg.push_back(t);
    }
    if (!seg.empty()) flushSeg(Connector::NONE, false);

    return result;
}

int Shell::runPipeline(Job& job, bool background){
    size_t n = job.pipeline.size();
    vector<int> pipeFds(n > 1 ? (n - 1) * 2 : 0);

    for (size_t i = 0; i + 1 < n; i++) {
        int fd[2];
        if (pipe(fd) == -1) { perror("execra: pipe"); return 1; }
        pipeFds[i*2] = fd[0];
        pipeFds[i*2 + 1] = fd[1];
    }

    vector<pid_t> pids;
    for (size_t i = 0; i < n; i++) {
        Command& cmd = job.pipeline[i];
        pid_t pid = fork();

        if (pid == 0) {
            tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig);
            signal(SIGINT, SIG_DFL);

            if (i > 0) dup2(pipeFds[(i-1)*2], STDIN_FILENO);
            if (i + 1 < n) dup2(pipeFds[i*2 + 1], STDOUT_FILENO);

            for (int fd : pipeFds) close(fd);

            if (!cmd.inFile.empty()) {
                int fd = open(cmd.inFile.c_str(), O_RDONLY);
                if (fd < 0) { perror(("execra: " + cmd.inFile).c_str()); exit(1); }
                dup2(fd, STDIN_FILENO);
                close(fd);
            }
            if (!cmd.outFile.empty()) {
                int fd = open(cmd.outFile.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
                if (fd < 0) { perror(("execra: " + cmd.outFile).c_str()); exit(1); }
                dup2(fd, STDOUT_FILENO);
                close(fd);
            }
            if (!cmd.appendFile.empty()) {
                int fd = open(cmd.appendFile.c_str(), O_WRONLY | O_CREAT | O_APPEND, 0644);
                if (fd < 0) { perror(("execra: " + cmd.appendFile).c_str()); exit(1); }
                dup2(fd, STDOUT_FILENO);
                close(fd);
            }

            vector<char*> args = getArgs(cmd.args);
            execvp(args[0], args.data());
            cerr << "execra: " << cmd.args[0] << ": command not found\n";
            exit(127);
        } else if (pid > 0) {
            pids.push_back(pid);
        } else {
            perror("execra: fork");
        }
    }

    for (int fd : pipeFds) close(fd);

    if (background) {
        pid_t display = pids.empty() ? (pid_t)-1 : pids.back();
        BgJob bj{pids, display, job.text, nextJobId++};
        bgJobs.push_back(bj);
        cout << "[" << bj.id << "] " << bj.displayPid << "\n";
        return 0;
    }

    int status = 0;
    for (pid_t pid : pids) {
        int st;
        waitpid(pid, &st, 0);
        status = st;
    }
    if (pids.empty()) return 1;
    if (WIFEXITED(status)) return WEXITSTATUS(status);
    if (WIFSIGNALED(status)) return 128 + WTERMSIG(status);
    return 1;
}

bool Shell::runBuiltin(vector<string>& args, int& status){
    if (args[0] == "cd") {
        string target;
        if (args.size() > 1) {
            if (args[1] == "-") {
                if (prevDir.empty()) { cerr << "execra: cd: OLDPWD not set\n"; status = 1; return true; }
                target = prevDir;
            } else target = args[1];
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

    if (args[0] == "history") {
        for (size_t i = 0; i < history.size(); i++)
            cout << " " << i + 1 << "  " << history[i] << "\n";
        status = 0;
        return true;
    }

    if (args[0] == "jobs") {
        reapBackgroundJobs();
        for (auto& bj : bgJobs)
            cout << "[" << bj.id << "]  " << bj.displayPid << "  Running    " << bj.text << "\n";
        status = 0;
        return true;
    }

    return false;
}

void Shell::run(){

        p_logo();

        while(true){

            reapBackgroundJobs();

            enableRM();
            string user_ip_word = "";
            int cur_pos = 0;

            cout << GREEN << fs::current_path().string() << RESET << ORANGE << "$ " << RESET;
            cout.flush();

            char c;
            while(true){
                read(STDIN_FILENO , &c, 1);
                if (c == '\n'){
                    if(!user_ip_word.empty()){
                        history.push_back(user_ip_word);
                        h_ind = history.size();
                    }
                    cout << '\n';
                    cout.flush();
                    break;
                }

                else if(c == 3){
                    cout << "^C\n";
                    cout.flush();
                    user_ip_word = "";
                    break;
                }

                else if(c==27){
                    char arr[2];

                    if(read(STDIN_FILENO, &arr, 2) == 2 && arr[0] == '['){
                        switch(arr[1]){
                            case 'A':
                                if(h_ind > 0){
                                    h_ind--;
                                    cout << "\33[2K\r";
                                    cout << GREEN << fs::current_path().string() << RESET << ORANGE << "$ " << RESET;

                                    user_ip_word = history[h_ind];
                                    cur_pos = user_ip_word.size();
                                    cout << user_ip_word;
                                    cout.flush();
                                }

                                break;

                            case 'B':
                                if(h_ind < (int)history.size()){
                                    h_ind++;
                                    cout << "\33[2K\r";
                                    cout << GREEN << fs::current_path().string() << RESET << ORANGE << "$ " << RESET;

                                    if(h_ind < (int)history.size()){
                                        user_ip_word = history[h_ind];
                                        cur_pos = user_ip_word.size();                                        cout << user_ip_word;
                                    }
                                    else user_ip_word = "";
                                    cout.flush();
                                }

                                break;

                            case 'C':
                                if(cur_pos < (int)user_ip_word.size()){
                                    cur_pos++;
                                    cout << "\033[C";
                                    cout.flush();
                                }

                                break;
                            case 'D':
                                if(cur_pos > 0){
                                    cur_pos--;
                                    cout << "\033[D";
                                    cout.flush();
                                }
                                break;

                        }
                    }
                }

                else if (c == 127){
                    if (!user_ip_word.empty() && cur_pos > 0) {
                        user_ip_word.erase(cur_pos-1,1);
                        cur_pos--;

                        cout << "\033[D";
                        cout << user_ip_word.substr(cur_pos);
                        cout << " ";

                        int steps = (int)user_ip_word.size() - cur_pos+1;

                        for(int i =0; i< steps; i++){
                            cout << "\033[D";
                        }
                        cout.flush();
                    }

                }
                else if(c >= 32 && c < 127){
                    user_ip_word.insert(cur_pos,1,c);
                    cout << user_ip_word.substr(cur_pos);
                    cur_pos++;
                    int steps = (int)user_ip_word.size() - cur_pos;

                    for(int i =0; i< steps; i++){
                        cout << "\033[D";
                    }

                    cout.flush();
                }

                else if (c==4){
                    cout << RED << "Exiting Execra.....\n" << RESET;
                    disableRM();
                    saveHistory();
                    return;
                }
            }

            if(user_ip_word.empty()) continue;

            disableRM();
            cout.flush();

            vector<string> tokens;
            vector<pair<Job, Connector>> jobs;
            try {
                tokens = tokenize(user_ip_word);
                if (tokens.empty()) continue;
                jobs = parseLine(tokens);
            } catch (const exception& e) {
                cerr << RED << "execra: " << e.what() << RESET << "\n";
                continue;
            }

            bool exitRequested = false;
            int exitCode = 0;
            Connector pendingConnector = Connector::NONE;

            for (auto& entry : jobs) {
                Job& job = entry.first;

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
                                           job.pipeline[0].appendFile.empty());
                    bool handled = isBuiltinShape && runBuiltin(job.pipeline[0].args, status);
                    if (!handled) status = runPipeline(job, job.background);
                    lastStatus = status;
                }

                pendingConnector = entry.second;
            }

            if (exitRequested) {
                saveHistory();
                disableRM();
                exit(exitCode);
            }
        }
    }
