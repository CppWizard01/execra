#include "shell.hpp"
#include <csignal>

int main(){
    signal(SIGINT, SIG_IGN);

    Shell execra;
    execra.run();

    return 0;
}