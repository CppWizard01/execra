#ifndef INPUT_HPP
#define INPUT_HPP

#include <string>
#include <vector>
#include <termios.h>

class Input {
private:
    std::string renderLine(const std::string& user_ip_word, int cur_pos, bool is_sugg);

public:
    std::vector<std::string> history;
    int h_ind;
    struct termios orig;

    Input();
    ~Input();
    
    void disableRM();
    void enableRM();
    std::string read_line();
};

#endif