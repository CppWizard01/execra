#include "shell.hpp"
#include "exec.hpp"
#include "colors.hpp"
#include <iostream>
#include <sstream>
#include <filesystem>
#include <algorithm>
#include <csignal>
#include <unistd.h>

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

        vector<Command> cmds;
        Command current_cmd;
        bool syntax_error = false;

        if (tokens.back() == "|") {
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
                } else {
                    cerr << "Execra: syntax error near unexpected token `newline`\n";
                    syntax_error = true;
                }
            } 
            else if (tokens[i] == ">>") {
                if (i + 1 < tokens.size()) {
                    current_cmd.outFile = tokens[++i];
                    current_cmd.appendOut = true;
                } else {
                    cerr << "Execra: syntax error near unexpected token `newline`\n";
                    syntax_error = true;
                }
            } 
            else if (tokens[i] == "<") {
                if (i + 1 < tokens.size()) {
                    current_cmd.inFile = tokens[++i];
                } else {
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
                } else {
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
                } else {
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
            
            Exec::execSingle(cmds[0], input.orig);
        } 
        else {
            Exec::execPipe(cmds, input.orig);
        }
    }   

    input.disableRM();
    cout << '\n';
}