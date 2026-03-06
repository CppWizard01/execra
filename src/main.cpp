#include "shell.hpp"
#include <csignal>

int main(){
    signal(SIGINT, SIG_IGN);
    signal(SIGTSTP, SIG_IGN);
    signal(SIGTTIN, SIG_IGN);
    signal(SIGTTOU, SIG_IGN);
    signal(SIGQUIT, SIG_IGN);

    pid_t shell_pgid = getpid();
    if (setpgid(shell_pgid, shell_pgid) < 0) {
        perror("Could not put shell in its process group");
        return 1;
    }

    tcsetpgrp(STDIN_FILENO, shell_pgid);

    Shell execra;
    execra.run();

    return 0;
}