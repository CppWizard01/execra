#include "history.hpp"
#include <cstdlib>
#include <fstream>

using namespace std;

static const size_t HISTORY_LIMIT = 1000;

static string historyFilePath() {
    const char* home = getenv("HOME");
    if (!home) return "";
    return string(home) + "/.execra_history";
}

void History::load() {
    string path = historyFilePath();
    if (path.empty()) return;
    ifstream in(path);
    if (!in.is_open()) return;
    string line;
    while (getline(in, line)) {
        if (!line.empty()) entries.push_back(line);
    }
    nav_index = (int)entries.size();
}

void History::save() const {
    string path = historyFilePath();
    if (path.empty()) return;
    ofstream out(path, ios::trunc);
    if (!out.is_open()) return;
    size_t start = (entries.size() > HISTORY_LIMIT) ? entries.size() - HISTORY_LIMIT : 0;
    for (size_t i = start; i < entries.size(); i++) out << entries[i] << "\n";
}

void History::push(const std::string& line) {
    entries.push_back(line);
    nav_index = (int)entries.size();
}
