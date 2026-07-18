#include "input.hpp"
#include "colors.hpp"
#include <iostream>
#include <unistd.h>
#include <fcntl.h>

using namespace std;

constexpr char KEY_CTRL_C = 3;
constexpr char KEY_CTRL_D = 4;
constexpr char KEY_TAB    = 9;
constexpr char KEY_ENTER  = 10;
constexpr char KEY_ESC    = 27;
constexpr char KEY_BACKSPACE     = 127;
constexpr char KEY_BACKSPACE_ALT = 8;

Input::Input() {
    if (tcgetattr(STDIN_FILENO, &orig) == -1) {
        perror("tcgetattr");
    }
}

Input::~Input() {
    disableRM();
}

void Input::disableRM() {
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig);
}

void Input::enableRM() {
    struct termios raw = orig;
    raw.c_lflag &= ~(ECHO | ICANON | ISIG);
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
}

string Input::renderLine(const string& prompt, const string& line, int cur_pos,
                          bool is_sugg, History& history) {
    string sugg;
    if (is_sugg && !line.empty()) {
        for (int i = (int)history.entries.size() - 1; i >= 0; i--) {
            if (history.entries[i].find(line) == 0 && history.entries[i].size() > line.size()) {
                sugg = history.entries[i].substr(line.size());
                break;
            }
        }
    }

    cout << "\33[2K\r" << prompt;
    cout << line;
    if (!sugg.empty()) cout << GREY << sugg << RESET;

    int diff = (int)sugg.size() + ((int)line.size() - cur_pos);
    for (int i = 0; i < diff; i++) cout << "\033[D";

    cout.flush();
    return sugg;
}

optional<string> Input::read_line(const string& prompt, History& history) {
    string line;
    int cur_pos = 0;
    char c;

    while (true) {
        if (read(STDIN_FILENO, &c, 1) <= 0) break;

        switch (c) {
            case KEY_TAB: {
                string sugg = renderLine(prompt, line, cur_pos, true, history);
                if (!sugg.empty()) {
                    line += sugg;
                    cur_pos = (int)line.size();
                    renderLine(prompt, line, cur_pos, true, history);
                }
                break;
            }

            case KEY_ENTER:
                renderLine(prompt, line, cur_pos, false, history);
                cout << '\n';
                if (!line.empty()) history.push(line);
                return line;

            case KEY_CTRL_C:
                renderLine(prompt, line, cur_pos, false, history);
                cout << "^C\n";
                cout.flush();
                return string("");

            case KEY_CTRL_D:
                cout << RED << "Exiting Execra.....\n" << RESET;
                return nullopt;

            case KEY_BACKSPACE:
            case KEY_BACKSPACE_ALT:
                if (!line.empty() && cur_pos > 0) {
                    line.erase(cur_pos - 1, 1);
                    cur_pos--;
                    renderLine(prompt, line, cur_pos, true, history);
                }
                break;

            case KEY_ESC: {
                int flags = fcntl(STDIN_FILENO, F_GETFL, 0);
                fcntl(STDIN_FILENO, F_SETFL, flags | O_NONBLOCK);

                char arr[2];
                int bytes_read = read(STDIN_FILENO, &arr, 2);

                fcntl(STDIN_FILENO, F_SETFL, flags);

                if (bytes_read == 2 && arr[0] == '[') {
                    switch (arr[1]) {
                        case 'A': // up
                            if (history.nav_index > 0) {
                                history.nav_index--;
                                line = history.entries[history.nav_index];
                                cur_pos = (int)line.size();
                                renderLine(prompt, line, cur_pos, false, history);
                            }
                            break;
                        case 'B': // down
                            if (history.nav_index < (int)history.entries.size()) {
                                history.nav_index++;
                                if (history.nav_index < (int)history.entries.size()) {
                                    line = history.entries[history.nav_index];
                                    cur_pos = (int)line.size();
                                } else {
                                    line.clear();
                                    cur_pos = 0;
                                }
                                renderLine(prompt, line, cur_pos, false, history);
                            }
                            break;
                        case 'C': // right
                            if (cur_pos < (int)line.size()) {
                                cur_pos++;
                                renderLine(prompt, line, cur_pos, true, history);
                            }
                            break;
                        case 'D': // left
                            if (cur_pos > 0) {
                                cur_pos--;
                                renderLine(prompt, line, cur_pos, true, history);
                            }
                            break;
                        case '3': { // delete
                            char tilde;
                            if (read(STDIN_FILENO, &tilde, 1) == 1 && tilde == '~') {
                                if (cur_pos < (int)line.size()) {
                                    line.erase(cur_pos, 1);
                                    renderLine(prompt, line, cur_pos, true, history);
                                }
                            }
                            break;
                        }
                    }
                }
                break;
            }

            default:
                if (c >= 32 && c < 127) {
                    line.insert(cur_pos, 1, c);
                    cur_pos++;
                    renderLine(prompt, line, cur_pos, true, history);
                }
                break;
        }
    }
    return line;
}
