#ifndef INPUT_HPP
#define INPUT_HPP

#include <string>
#include <optional>
#include <termios.h>
#include "history.hpp"

class Input {
public:
    struct termios orig;

    Input();
    ~Input();

    void disableRM();
    void enableRM();

    std::optional<std::string> read_line(const std::string& prompt, History& history);

private:
    std::string renderLine(const std::string& prompt, const std::string& line,
                            int cur_pos, bool is_sugg, History& history);
};

#endif
