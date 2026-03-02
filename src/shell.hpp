#ifndef SHELL_HPP
#define SHELL_HPP

#include "input.hpp"

class Shell {
    private:
        Input input;
        void p_logo();
        
    public:
        void run();
};

#endif