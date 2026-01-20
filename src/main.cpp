#include <iostream>
#include <vector>
#include <string>
#include <unistd.h>
#include <sys/wait.h>
#include <sstream>
#include <filesystem>
#include <fcntl.h>

using namespace std;
namespace fs = filesystem;

int main(){
    string user_ip_word = "";

    const string GREEN = "\033[1;32m";
    const string ORANGE = "\033[38;5;208m";
    const string RESET = "\033[0m";

    while(true){

        cout << GREEN << fs::current_path().string() << RESET << ORANGE << "$ " << RESET;
        getline(cin, user_ip_word);

        if(cin.eof()) break;

        vector<string> user_ip;

        stringstream ss(user_ip_word);
        string token;

        while (ss >> token) user_ip.push_back(token);
        int ip_size = user_ip.size();
        string opFile = "";

        bool isPipe = false;
        vector<string> cmd1, cmd2;

        for(int i = 0; i< ip_size; i++){
            if (user_ip[i] == "|"){
                isPipe = true;
                for(int j = 0; j < i; j++) {
                    cmd1.push_back(user_ip[j]);
                }

                for(int j = i+1; j < ip_size; j++) {
                    cmd2.push_back(user_ip[j]);
                }
            }
        }

        if(isPipe){

            vector<char*> args1;
            vector<char*> args2;

            for(string& s: cmd1){
                args1.push_back(s.data());
            }

            for(string& s: cmd2){
                args2.push_back(s.data());
            }
            
            args1.push_back(nullptr);
            args2.push_back(nullptr); 

            int fd[2];

            if(pipe(fd) == -1){
                perror("Pipe Failed.");
                continue;
            }

            pid_t pid1 = fork();

            if(pid1 == 0){
                dup2(fd[1], STDOUT_FILENO);
                close(fd[0]);
                close(fd[1]);

                execvp(args1[0], args1.data());
                perror("exec cmd1 failed");
                exit(1);
            }

            pid_t pid2 = fork();

            if(pid2 == 0){
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

        else{
            for (int i =0; i < ip_size; i++){
                if (user_ip[i] == ">" && i+1 < ip_size){
                    opFile = user_ip[i+1];
                    user_ip.erase(user_ip.begin() + i, user_ip.begin() + i+2 );
                    break;
                }
            }
            // ----------------------------

            vector<char*> args;

            for(string& s: user_ip){
                args.push_back(s.data());
            }
            
            args.push_back(nullptr);  
            
            if (ip_size > 0){
                if (user_ip[0] == "cd"){
                    if (ip_size > 1) {
                        const char* path = user_ip[1].c_str();
                        chdir(path);                
                    }
                    else perror("No path given");        
                }
                else if(user_ip[0] == "exit"){
                    break;
                }
                else{
                    pid_t pid = fork();

                    if (pid == 0){

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

                        execvp(args[0], args.data());
                        perror("exec failed");
                        exit(1);
                    }
                    else if (pid > 0){
                        waitpid(pid, nullptr, 0);
                    }
                    else{
                        perror("Error");
                    }
                }
            }
        }       
    }
}