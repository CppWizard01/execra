#include "input.hpp"
#include "colors.hpp"
#include <iostream>
#include <unistd.h>
#include <filesystem>
#include <fcntl.h>

using namespace std;
namespace fs = filesystem;

constexpr char KEY_CTRL_C = 3;
constexpr char KEY_CTRL_D = 4;
constexpr char KEY_TAB    = 9;
constexpr char KEY_ENTER  = 10;
constexpr char KEY_ESC    = 27;
constexpr char KEY_BACKSPACE = 127;
constexpr char KEY_BACKSPACE_ALT = 8;

Input::Input() {
    h_ind = 0;
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

string Input::renderLine(const string& user_ip_word, int cur_pos, bool is_sugg) {
    string sugg = "";
    if(is_sugg && !user_ip_word.empty()) {
        for(int i = history.size()-1; i>=0; i--) {
            if(history[i].find(user_ip_word) == 0 && history[i].size() > user_ip_word.size()) {
                sugg = history[i].substr(user_ip_word.size());
                break;
            }
        }
    }

    cout << "\33[2K\r" << GREEN << fs::current_path().string() << RESET << ORANGE << "$ " << RESET;
    cout << user_ip_word;
    if(!sugg.empty()) cout << GREY << sugg << RESET;

    int diff = (int)sugg.size() + ((int)user_ip_word.size() - cur_pos);
    for(int i = 0; i < diff; i++) cout << "\033[D";

    cout.flush();
    return sugg;
}

string Input::read_line() {
    string user_ip_word = "";
    int cur_pos = 0; 
    char c;

    while(true) {
        if (read(STDIN_FILENO , &c, 1) <= 0) break;

        switch(c) {
            case KEY_TAB: {
                string sugg = renderLine(user_ip_word, cur_pos, true);
                if(!sugg.empty()) {
                    user_ip_word += sugg;
                    cur_pos = user_ip_word.size();
                    renderLine(user_ip_word, cur_pos, true);
                }
                break;
            }

            case KEY_ENTER:

                renderLine(user_ip_word, cur_pos, false);
                if(!user_ip_word.empty()) {
                    history.push_back(user_ip_word);
                    h_ind = history.size();
                }
                cout << '\n';
                return user_ip_word;

            case KEY_CTRL_C:
                renderLine(user_ip_word, cur_pos, false);
                cout << "^C\n";
                user_ip_word = "";
                return "";

            case KEY_CTRL_D:
                cout << RED << "Exiting Execra.....\n" << RESET;
                return "_EOF";

            case KEY_BACKSPACE:
            case KEY_BACKSPACE_ALT:
                if (!user_ip_word.empty() && cur_pos > 0) {
                    user_ip_word.erase(cur_pos-1, 1);
                    cur_pos--;
                    renderLine(user_ip_word, cur_pos, true);
                }
                break;

            case KEY_ESC: {
                int flags = fcntl(STDIN_FILENO, F_GETFL, 0);
                fcntl(STDIN_FILENO, F_SETFL, flags | O_NONBLOCK);

                char arr[2];
                int bytes_read = read(STDIN_FILENO, &arr, 2);

                fcntl(STDIN_FILENO, F_SETFL, flags);

                if(bytes_read == 2 && arr[0] == '[') {
                    switch(arr[1]) {
                        case 'A':
                            if(h_ind > 0) {
                                h_ind--;
                                user_ip_word = history[h_ind];
                                cur_pos = user_ip_word.size();
                                renderLine(user_ip_word, cur_pos, false);
                            }
                            break;
                        case 'B':
                            if(h_ind < (int)history.size()) {
                                h_ind++;
                                if(h_ind < (int)history.size()) {
                                    user_ip_word = history[h_ind];
                                    cur_pos = user_ip_word.size();
                                } else {
                                    user_ip_word = "";
                                    cur_pos = 0;
                                }
                                renderLine(user_ip_word, cur_pos, false);
                            }
                            break;
                        case 'C':
                            if(cur_pos < (int)user_ip_word.size()) {
                                cur_pos++;
                                renderLine(user_ip_word, cur_pos, true);
                            }
                            break;
                        case 'D':
                            if(cur_pos > 0) {
                                cur_pos--;
                                renderLine(user_ip_word, cur_pos, true);
                            }
                            break;  
                        case '3': {
                            char tilde;
                            if(read(STDIN_FILENO, &tilde, 1) == 1 && tilde == '~') {
                                if(cur_pos < (int)user_ip_word.size()) {
                                    user_ip_word.erase(cur_pos, 1);
                                    renderLine(user_ip_word, cur_pos, true);
                                }
                            }
                            break;
                        }
                    }
                }
                break;
            }

            default:
                if(c >= 32 && c < 127) {
                    user_ip_word.insert(cur_pos, 1, c);
                    cur_pos++;
                    renderLine(user_ip_word, cur_pos, true);
                }
                break;
        }
    }
    return "";
}