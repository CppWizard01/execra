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

using namespace std;
namespace fs = filesystem;

volatile sig_atomic_t sig_int = 0;

void handleSignal(int signal){
    if (signal == SIGINT) {
        sig_int = 1;
        cout << '\n';
    }
}

Shell::Shell(){
    h_ind = 0;
    if (tcgetattr(STDIN_FILENO, &orig) == -1) {
        perror("tcgetattr");
    }
}

Shell::~Shell(){
    disableRM();
}

void Shell::disableRM(){
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig);
}

void Shell::enableRM(){
        // tcgetattr(STDIN_FILENO, &orig);
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
        cout << ORANGE << "v1.0.0" << RESET << "\n\n";
}

vector<char*> Shell::getArgs(vector<string>& args){
        vector<char*> c_args;
        for(auto& arg: args){
            c_args.push_back(arg.data());
        }
        c_args.push_back(nullptr);
        return c_args;
}

void Shell::execCommand(vector<string>& user_ip, string opFile){
    pid_t pid = fork();

    if (pid == 0){

        tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig);

        signal(SIGINT, SIG_DFL);
        if(!opFile.empty()){
            int fd = open(
                opFile.c_str(), 
                O_WRONLY | O_CREAT | O_TRUNC,
                0644);

            if (fd < 0){
                perror("Cannot open the file");
                exit(1);
            }

            dup2(fd, STDOUT_FILENO);
            close(fd);
        }

        vector<char*> args = getArgs(user_ip);
        execvp(args[0], args.data());
        perror("Execution failed");
        exit(1);
    }

    else if (pid > 0){
        waitpid(pid, nullptr, 0);
    }

    else{
        perror("Error");
    }
}

void Shell::execPipe(vector<string>& cmd1, vector<string>& cmd2){
    vector<char*> args1 = getArgs(cmd1);
    vector<char*> args2 = getArgs(cmd2);

    int fd[2];

    if(pipe(fd) == -1){
        perror("Pipe Failed.");
        return;
    }

    pid_t pid1 = fork();

    if(pid1 == 0){
        tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig);
        signal(SIGINT, SIG_DFL);
        dup2(fd[1], STDOUT_FILENO);
        close(fd[0]);
        close(fd[1]);

        execvp(args1[0], args1.data());
        perror("exec cmd1 failed");
        exit(1);
    }

    pid_t pid2 = fork();

    if(pid2 == 0){
        tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig);
        signal(SIGINT, SIG_DFL);
        dup2(fd[0], STDIN_FILENO);  
        close(fd[0]);
        close(fd[1]);

        execvp(args2[0], args2.data());
        perror("exec cmd2 failed");
        exit(1);
    }

    close(fd[0]);
    close(fd[1]);

    waitpid(pid1, nullptr, 0);
    waitpid(pid2, nullptr, 0);
}

string Shell::read_line(){
    string user_ip_word = "";
    int cur_pos = 0; 
    char c;

    auto text = [&](bool is_sugg = true){
        string sugg = "";
        if(is_sugg && !user_ip_word.empty()){
            for(int i = history.size()-1; i>=0; i--){
                if(history[i].find(user_ip_word) == 0 && history[i].size() > user_ip_word.size()){
                    sugg = history[i].substr(user_ip_word.size());
                    break;
                }
            }
        }

        cout << "\33[2K\r" << GREEN << fs::current_path().string() << RESET << ORANGE << "$ " << RESET;

        cout << user_ip_word;
        if(!sugg.empty()) cout << GREY << sugg << RESET;

        int diff = (int)sugg.size() + ((int)user_ip_word.size() - cur_pos);
        for(int i = 0; i < diff; i++) cout << "\033[D";

        cout.flush();
        return sugg;
    };

    
    while(true){
        if (read(STDIN_FILENO , &c, 1) <= 0) break;

        if(c == 9){
            string sugg = text(true);
            if(!sugg.empty()){
                user_ip_word+= sugg;
                cur_pos = user_ip_word.size();
                text(true);
            }
        }

        else if (c == '\n'){
            if(!user_ip_word.empty()){
                history.push_back(user_ip_word);
                h_ind = history.size();
            }
            cout << '\n';
            return user_ip_word;
        }

        else if(c == 3){
            cout << "^C\n";
            user_ip_word = "";
            return "";
        }

        else if(c==27){
            char arr[2];

            if(read(STDIN_FILENO, &arr, 2) == 2 && arr[0] == '['){
                switch(arr[1]){
                    case 'A':
                        if(h_ind > 0){
                            h_ind--;
                            // cout << "\33[2K\r";
                            // cout << GREEN << fs::current_path().string() << RESET << ORANGE << "$ " << RESET;

                            user_ip_word = history[h_ind];
                            cur_pos = user_ip_word.size();
                            // cout << user_ip_word;
                            // cout.flush();
                            text(false);
                        }
                        break;

                    case 'B':
                        if(h_ind < (int)history.size()){
                            h_ind++;
                            // cout << "\33[2K\r";
                            // cout << GREEN << fs::current_path().string() << RESET << ORANGE << "$ " << RESET;
                                    
                            if(h_ind < (int)history.size()){
                                user_ip_word = history[h_ind];
                                cur_pos = user_ip_word.size();                                        
                                // cout << user_ip_word;
                            }
                            else {
                                user_ip_word = "";
                                cur_pos = 0;
                            }
                            // cout.flush();
                            text(false);
                        }
                        break;
                        
                    case 'C':
                        if(cur_pos < (int)user_ip_word.size()){
                            cur_pos++;
                            // cout << "\033[C";
                            // cout.flush();
                            text(true);
                        }
                        break;

                    case 'D':
                        if(cur_pos > 0){
                            cur_pos--;
                            // cout << "\033[D";
                            // cout.flush();
                            text(true);
                        }
                        break;  
                              
                }
            }
        }

        else if (c == 127){
            if (!user_ip_word.empty() && cur_pos > 0) {
                user_ip_word.erase(cur_pos-1,1);
                cur_pos--;
                text(true);

                // cout << "\033[D";
                // cout << user_ip_word.substr(cur_pos);
                // cout << " ";

                // int steps = (int)user_ip_word.size() - cur_pos+1;

                // for(int i =0; i< steps; i++){
                //     cout << "\033[D";
                // }
                // cout.flush();
            }
        }

        else if(c >= 32 && c < 127){
            user_ip_word.insert(cur_pos,1,c);
            // cout << user_ip_word.substr(cur_pos);
            cur_pos++;
            // int steps = (int)user_ip_word.size() - cur_pos;

            // for(int i =0; i< steps; i++){
            //     cout << "\033[D";
            // }

            // cout.flush();
            text(true);
        }

        else if (c==4){
            cout << RED << "Exiting Execra.....\n" << RESET;
            return "_EOF";
        }
    }

    return "";

}
 
void Shell::run(){
    p_logo();
                
    while(true){
        enableRM();

        cout << GREEN << fs::current_path().string() << RESET << ORANGE << "$ " << RESET;
        cout.flush();

        sig_int = 0;
        string user_ip_word = read_line();

        if(user_ip_word == "_EOF") break;
        if(user_ip_word.empty()) continue;

        stringstream ss(user_ip_word);
        string token;
        vector<string> tokens;
        while (ss >> token) tokens.push_back(token);

        if (tokens.empty()) continue;

        auto chPipe = find(tokens.begin(), tokens.end(), "|");
        if (chPipe != tokens.end()){
            vector<string> cmd1(tokens.begin(), chPipe);
            vector<string> cmd2(chPipe+1, tokens.end());
            execPipe(cmd1, cmd2);
            continue;
        }

        string opFile = "";
        auto chRedir = find(tokens.begin(), tokens.end(), ">");

        if (chRedir != tokens.end()){
            if(chRedir +1 != tokens.end()){
                opFile = *(chRedir+1);
                tokens.erase(chRedir, chRedir+2);
            }
            else {
                cerr << "Expected a file after '>'";
                continue;
            }
        }
            
        if (tokens[0] == "exit") break;

        else if (tokens[0] == "cd"){
            if (tokens.size() > 1){
                chdir( tokens[1].c_str() ); 
            }
            else{
                if(getenv("HOME")) chdir(getenv("HOME"));
                else cerr << "No HOME variable found";
            }
        }

        else if(tokens[0] == "history"){
            for(size_t i =0; i < history.size();i++){
                cout << i+1 << " " << history[i] << '\n'; 
            }
        }

        else if(tokens[0] == "help"){
            cout << CYAN << "\nExecra Shell - v1.0.0" << RESET << '\n';
        }
            
        else execCommand(tokens, opFile);   
    }

    disableRM();
    cout << '\n';
}

