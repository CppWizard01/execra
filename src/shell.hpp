#ifndef SHELL_HPP
#define SHELL_HPP

#include "input.hpp"
#include "exec.hpp"
#include <vector>

class Shell {
    private:
        Input input;
        std::vector<Job> jobs_list;
        void p_logo(); 
        
    public:
        void run();
};

#endif