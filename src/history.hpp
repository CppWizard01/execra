#ifndef HISTORY_HPP
#define HISTORY_HPP

#include <string>
#include <vector>

class History {
public:
    std::vector<std::string> entries;
    int nav_index = 0;

    void load();
    void save() const;

    void push(const std::string& line);
};

#endif
