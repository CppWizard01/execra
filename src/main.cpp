#include <iostream>
#include <vector>
#include <string>
#include <unistd.h>
#include <sys/wait.h>

using namespace std;

int main(){
    string user_ip_word = "";

    const string GREEN = "\033[1;32m";
    const string ORANGE = "\033[38;5;208m";
    const string RESET = "\033[0m";

    while(true){
        char cwd[1024];
        getcwd(cwd, sizeof(cwd));

        cout << GREEN << cwd << RESET << ORANGE << "$ " << RESET;
        getline(cin, user_ip_word);

        if(cin.eof()) break;

        vector<string> user_ip;

        string s = "";
        for(char ch:user_ip_word){
            if(ch == ' '){
                user_ip.push_back(s);
                s= "";
            }
            else s+= ch;
        }
        if (s != "") user_ip.push_back(s);

        // ----------------------------

        vector<char*> args;

        for(string& s: user_ip){
            args.push_back( const_cast<char*>(s.c_str()));
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