#include "shell.hpp"
#include "exec.hpp"
#include "colors.hpp"
#include <iostream>
#include <sstream>
#include <filesystem>
#include <algorithm>
#include <csignal>
#include <unistd.h>
#include <sys/wait.h>
#include <stdexcept>

using namespace std;
namespace fs = filesystem;

volatile sig_atomic_t sig_int = 0;

void handleSignal(int signal) {
    if (signal == SIGINT) {
        sig_int = 1;
        cout << '\n';
    }
}

void Shell::p_logo() {
    cout << CYAN << R"(
  #######  ##   ##  #######  ######   ######     ###
  ##        ## ##   ##      ##    ##  ##   ##   ## ##
  #####      ###    #####   ##        ######   #######
  ##        ## ##   ##      ##    ##  ##   ##  ##   ##
  #######  ##   ##  #######  ######   ##   ##  ##   ##
)" << RESET << endl;
    cout << ORANGE << "v1.0.0" << RESET << "\n\n";
}

void Shell::run() {
    p_logo();
    signal(SIGINT, handleSignal);

    while(true) {
        input.enableRM();

        int status;
        pid_t pid;
        while ((pid = waitpid(-1, &status, WNOHANG | WUNTRACED | WCONTINUED)) > 0) {
            auto it = std::find_if(jobs_list.begin(), jobs_list.end(), [pid](const Job& j) { return j.pid == pid; });

            if (it != jobs_list.end()) {
                if (WIFEXITED(status)) {
                    cout << "\n[" << std::distance(jobs_list.begin(), it) + 1 << "]+  Done\t\t" << it->command << "\n";
                    jobs_list.erase(it);
                } 

                else if (WIFSIGNALED(status)) {
                    cout << "\n[" << std::distance(jobs_list.begin(), it) + 1 << "]+  Terminated\t" << it->command << "\n";
                    jobs_list.erase(it);
                } 

                else if (WIFSTOPPED(status)) {
                    it->is_stopped = true;
                } 

                else if (WIFCONTINUED(status)) {
                    it->is_stopped = false;
                }
            }
        }

        cout << GREEN << fs::current_path().string() << RESET << ORANGE << "$ " << RESET;
        cout.flush();

        sig_int = 0;
        string user_ip_word = input.read_line();

        if(user_ip_word == "_EOF") break;
        if(user_ip_word.empty()) continue;

        stringstream ss(user_ip_word);
        string token;
        vector<string> tokens;
        while (ss >> token) tokens.push_back(token);

        if (tokens.empty()) continue;

        bool run_in_bg = false;
        if (tokens.back() == "&") {
            run_in_bg = true;
            tokens.pop_back();
        }

        vector<Command> cmds;
        Command current_cmd;
        bool syntax_error = false;

        if (!tokens.empty() && tokens.back() == "|") {
            cerr << "Execra: syntax error near unexpected token `|`\n";
            continue;
        }

        for (size_t i = 0; i < tokens.size(); ++i) {
            if (tokens[i] == "|") {
                if (current_cmd.args.empty()) {
                    cerr << "Execra: syntax error near unexpected token `|`\n";
                    syntax_error = true; break;
                }
                cmds.push_back(current_cmd);
                current_cmd = Command(); 
            } 

            else if (tokens[i] == ">") {
                if (i + 1 < tokens.size()) {
                    current_cmd.outFile = tokens[++i];
                    current_cmd.appendOut = false;
                } 

                else {
                    cerr << "Execra: syntax error near unexpected token `newline`\n";
                    syntax_error = true;
                }
            } 

            else if (tokens[i] == ">>") {
                if (i + 1 < tokens.size()) {
                    current_cmd.outFile = tokens[++i];
                    current_cmd.appendOut = true;
                } 
                
                else {
                    cerr << "Execra: syntax error near unexpected token `newline`\n";
                    syntax_error = true;
                }
            } 
            else if (tokens[i] == "<") {
                if (i + 1 < tokens.size()) {
                    current_cmd.inFile = tokens[++i];
                } 

                else {
                    cerr << "Execra: syntax error near unexpected token `newline`\n";
                    syntax_error = true;
                }
            } 
            else if (tokens[i] == "<<") {
                if (i + 1 < tokens.size()) {
                    string delimiter = tokens[++i];
                    string heredoc = "";
                    string line;
                    
                    input.disableRM();

                    while (true) {
                        cout << "> ";
                        getline(cin, line);

                        if (line == delimiter) break;
                        heredoc += line + "\n";
                    }

                    input.enableRM(); 
                    current_cmd.heredocContent = heredoc;
                } 
                
                else {
                    cerr << "Execra: syntax error near unexpected token `newline`\n";
                    syntax_error = true;
                }
            } 

            else {
                current_cmd.args.push_back(tokens[i]);
            }
        }
        
        if (syntax_error) continue;
        
        if (!current_cmd.args.empty()) {
            cmds.push_back(current_cmd);
        }

        if (cmds.empty() || cmds[0].args.empty()) continue;

        if (cmds.size() == 1) {
            string base_cmd = cmds[0].args[0];
            
            if (base_cmd == "exit") break;

            else if (base_cmd == "cd") {
                if (cmds[0].args.size() > 1) {
                    if (chdir(cmds[0].args[1].c_str()) != 0) perror("cd failed");
                } 
                
                else {
                    if(getenv("HOME")) chdir(getenv("HOME"));
                    else cerr << "No HOME variable found\n";
                }
                continue;
            }

            else if (base_cmd == "history") {
                for(size_t i = 0; i < input.history.size(); i++) {
                    cout << " " << input.history[i] << '\n'; 
                }

                continue;
            }

            else if (base_cmd == "help") {
                cout << CYAN << "\nExecra Shell - v1.0.0" << RESET << '\n';
                continue;
            }
            
            else if (base_cmd == "jobs") {
                for (size_t i = 0; i < jobs_list.size(); ++i) {
                    cout << "[" << i + 1 << "] " 
                         << (jobs_list[i].is_stopped ? "Stopped" : "Running") 
                         << "\t" << jobs_list[i].command << '\n';
                }
                continue;
            }

            else if (base_cmd == "bg") {
                if (jobs_list.empty()) {
                    cerr << "Execra: bg: current: no such job\n";
                    continue;
                }
                
                Job& last_job = jobs_list.back();
                if (last_job.is_stopped) {
                    last_job.is_stopped = false;
                    kill(-last_job.pid, SIGCONT);
                }
                cout << "[" << jobs_list.size() << "] " << last_job.command << " &\n";
                continue;
            }

            else if (base_cmd == "fg") {
                if (jobs_list.empty()) {
                    cerr << "Execra: fg: current: no such job\n";
                    continue;
                }
                
                input.disableRM();
                
                Job job_to_fg = jobs_list.back();
                jobs_list.pop_back();

                cout << job_to_fg.command << '\n';
                
                tcsetpgrp(STDIN_FILENO, job_to_fg.pid);
                kill(-job_to_fg.pid, SIGCONT);

                int fg_status;
                waitpid(job_to_fg.pid, &fg_status, WUNTRACED);

                if (WIFSTOPPED(fg_status)) {
                    cout << "\n[Stopped] PID: " << job_to_fg.pid << '\n';
                    job_to_fg.is_stopped = true;
                    jobs_list.push_back(job_to_fg);
                }
                
                tcsetpgrp(STDIN_FILENO, getpid());
                continue;
            }
            
            else if (base_cmd == "kill") {
                if (cmds[0].args.size() < 2) {
                    cerr << "kill: usage: kill <pid> | %<job_id>\n";
                    continue;
                }
                string target = cmds[0].args[1];
                pid_t target_pid = -1;
                try {
                    if (target[0] == '%') {
                        int job_idx = stoi(target.substr(1)) - 1;
                        if (job_idx < 0 || (size_t)job_idx >= jobs_list.size()) {
                            cerr << "Execra: kill: " << target << ": no such job\n";
                            continue;
                        }
                        target_pid = -jobs_list[job_idx].pid; 
                    } 
                    
                    else {
                        target_pid = stoi(target);
                    }
                    if (kill(target_pid, SIGTERM) < 0) perror("kill failed");
                } 
                
                catch (const exception& e) {
                    cerr << "Execra: kill: " << target << ": invalid argument\n";
                }
                continue;
            }

            input.disableRM();
            Exec::execSingle(cmds[0], input.orig, jobs_list, run_in_bg);
        } 
        else {
            input.disableRM();
            Exec::execPipe(cmds, input.orig, jobs_list, run_in_bg);
        }
    }   

    input.disableRM();
    cout << '\n';
}