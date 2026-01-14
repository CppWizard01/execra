#include <iostream>
#include <vector>
#include <string>
#include <unistd.h>
#include <sys/wait.h>
#include <sstream>
#include <filesystem>

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

        // ----------------------------

        vector<char*> args;

        for(string& s: user_ip){
            args.push_back(s.data());
        }
        
        args.push_back(nullptr);  
        
        if (user_ip.size() > 0){
            if (user_ip[0] == "cd"){
                if (user_ip.size() > 1) {
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