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

class Shell{
private:
    const string GREEN = "\033[1;32m";
    const string ORANGE = "\033[38;5;208m";
    const string RESET = "\033[0m";

    struct termios orig;

    void disableRM() {
        tcsetattr(STDIN_FILENO,TCSAFLUSH, &orig);
    }

    void enableRM(){
        tcgetattr(STDIN_FILENO, &orig);
        struct termios raw = orig;
        
        raw.c_lflag &= ~(ECHO | ICANON | ISIG);
        tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
    }

    vector<char*> getArgs(vector<string>& args){
        vector<char*> c_args;
        for(auto& arg: args){
            c_args.push_back(arg.data());
        }
        c_args.push_back(nullptr);
        return c_args;
    }

    void execCommand(vector<string>& user_ip, string opFile = ""){
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

    void execPipe(vector<string>& cmd1, vector<string>& cmd2){
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

public:

    ~Shell() {
        disableRM();
    }

    void run(){

        enableRM();

        string user_ip_word = "";        
        while(true){

            cout << GREEN << fs::current_path().string() << RESET << ORANGE << "$ " << RESET;
            sig_int = 0;
            cout.flush();

            char c;
            while(true){
                read(STDIN_FILENO , &c, 1);
                if (c == '\n'){
                    cout << '\n';
                    break;
                }

                else if(c == 3){
                    cout << "^C\n";
                    user_ip_word = "";
                    break;
                }

                else if(c==27){
                    char arr[2];

                    if(read(STDIN_FILENO, &arr, 2) == 2 && arr[0] == '['){
                        switch(arr[1]){
                            case 'A':
                                cout << "A";
                                cout.flush();
                                break;

                            case 'B':
                                cout << "B";
                                cout.flush();
                                break;
                            case 'C':
                                cout << "C";
                                cout.flush();
                                break;
                            case 'D':
                                cout << "D";
                                cout.flush();
                                break;  
                              
                        }
                    }
                }

                else if (c == 127){
                    if (!user_ip_word.empty()) {
                        user_ip_word.pop_back();
                        cout << "\b \b";
                        cout.flush();
                    }

                }
                else if(c >= 32 && c < 127){
                    user_ip_word.push_back(c);
                    cout << c;
                    cout.flush();
                }

                else if (c==4){
                    cout << "Exiting Execra\n";
                    return;
                }
            }

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
            else execCommand(tokens, opFile);   
        }

        disableRM();
    }
};

int main(){
    struct sigaction sa;
    sa.sa_handler = handleSignal;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(SIGINT, &sa, nullptr);

    Shell execra;
    execra.run();

    return 0;
}