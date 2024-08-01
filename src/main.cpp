#include <ncurses.h>
#include <unistd.h>
#include <string>
#include <vector>
#include <fstream>
#include <sstream>
#include <iostream>
#include <cstdlib>
#include <sys/types.h>
#include <pwd.h>
#include <stdlib.h>
#include <stdio.h>

using namespace std;

bool home = true;
bool is_root = (getuid() == 0);

void draw_text(WINDOW* win, const vector<string>& text, int cursor_y, int cursor_x, const string& message = "", bool new_file = false) {
    wclear(win);
    int max_y, max_x;
    getmaxyx(win, max_y, max_x);

    // Draw top and bottom bars
    string top_bar = "Luced v.2.2 - Terminal Text Editor";
    string bottom_bar = "Ctrl + Shift + V: Paste Clipboard Content  Ctrl + S: Save File  Ctrl + Q: Exit  Ctrl + Shift + C: Copy to Clipboard";

    // Center the top bar
    int top_bar_x = (max_x - top_bar.length()) / 2;
    wattron(win, A_BOLD | A_REVERSE);
    mvwaddnstr(win, 0, top_bar_x, top_bar.c_str(), max_x - top_bar_x);
    wattroff(win, A_BOLD | A_REVERSE);

    // Center the bottom bar with highlighting
    int bottom_bar_x = (max_x - bottom_bar.length()) / 2;
    wattron(win, A_BOLD | A_REVERSE);
    mvwaddnstr(win, max_y - 1, bottom_bar_x, bottom_bar.c_str(), max_x - bottom_bar_x);
    wattroff(win, A_BOLD | A_REVERSE);

    // Display "New File" message if applicable with highlighting
    if (new_file) {
        string new_file_msg = "New File";
        wattron(win, A_BOLD | A_REVERSE);
        mvwaddnstr(win, max_y - 2, (max_x - new_file_msg.length()) / 2, new_file_msg.c_str(), max_x - (max_x - new_file_msg.length()) / 2);
        wattroff(win, A_BOLD | A_REVERSE);
    }

    // Display a message if needed with highlighting
    if (!message.empty()) {
        wattron(win, A_BOLD | A_REVERSE);
        mvwaddnstr(win, max_y / 2, (max_x - message.length()) / 2, message.c_str(), max_x - (max_x - message.length()) / 2);
        wattroff(win, A_BOLD | A_REVERSE);
    }

    // Display text within the window
    int start_y = 1; // Start displaying text below the top bar
    int end_y = min((int)text.size(), max_y - 2); // Display text up to window height minus top and bottom bars

    for (int i = 0; i < end_y; ++i) {
        string line = text[i];
        size_t start_pos = 0;
        size_t length = (size_t)max_x;
        while (start_pos < line.size()) {
            mvwaddnstr(win, i + 1, 0, line.substr(start_pos, length).c_str(), (int)length);
            start_pos += length;
            length = min((size_t)max_x, line.size() - start_pos);
        }
    }

    // Move the cursor to the necessary position
    wmove(win, cursor_y - start_y + 1, cursor_x);
    wrefresh(win);
}

pair<int, int> move_cursor(int key, int cursor_y, int cursor_x, const vector<string>& text, int max_y, int max_x) {
    int num_lines = text.size();

    if (key == KEY_LEFT) {
        if (cursor_x > 0) {
            cursor_x--;
        } else if (cursor_y > 1) {
            cursor_y--;
            cursor_x = min(cursor_x, (int)text[cursor_y - 2].length());
        }
    } else if (key == KEY_RIGHT) {
        if (cursor_x < (int)text[cursor_y - 1].length()) {
            cursor_x++;
        } else if (cursor_y < num_lines) {
            cursor_y++;
            cursor_x = min(cursor_x, (int)text[cursor_y - 1].length());
        }
    } else if (key == KEY_UP) {
        if (cursor_y > 1) {
            cursor_y--;
            cursor_x = min(cursor_x, (int)text[cursor_y - 1].length());
        }
    } else if (key == KEY_DOWN) {
        if (cursor_y < num_lines) {
            cursor_y++;
            cursor_x = min(cursor_x, (int)text[cursor_y - 1].length());
        }
    }

    cursor_x = min(cursor_x, max_x - 1);
    return {cursor_y, cursor_x};
}

void clipboard_copy(const string& text) {
    string command = "wl-copy <<< \"" + text + "\"";
    system(command.c_str());
}

string clipboard_paste() {
    char buffer[128];
    string result;
    FILE* pipe = popen("wl-paste", "r");
    if (pipe) {
        while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
            result += buffer;
        }
        pclose(pipe);
    }
    return result;
}

bool save_file(const string& filename, const vector<string>& text) {
    ofstream file(filename);
    if (!file.is_open()) {
        cerr << "Error saving file: " << filename << endl;
        return false;
    }
    for (const auto& line : text) {
        file << line << endl;
    }
    return true;
}

vector<string> load_file(const string& filename) {
    vector<string> text;
    ifstream file(filename);
    if (!file.is_open()) {
        // Do not create an empty file here
        text.push_back("");
    } else {
        string line;
        while (getline(file, line)) {
            text.push_back(line);
        }
        if (text.empty()) {
            text.push_back("");
        }
    }
    return text;
}

void main_loop(WINDOW* win, const string& filename) {
    raw();           // Disable line buffering
    keypad(win, TRUE); // Enable special keys
    noecho();        // Don't echo input
    curs_set(1);     // Show cursor

    // Load file content
    vector<string> text = load_file(filename);

    // Initial values
    int cursor_x = 0;
    int cursor_y = 1; // Start cursor at line 1 (below the top bar)
    int max_y, max_x;
    getmaxyx(win, max_y, max_x);

    bool new_file = filename == "untitled.txt" && text.size() == 1 && text[0].empty();
    bool file_saved = !new_file;

    if (is_root) {
        draw_text(win, text, cursor_y, cursor_x, "You are running as root!");
        wgetch(win); // Wait for a key press before continuing
    }

    while (true) {
        draw_text(win, text, cursor_y, cursor_x, "", new_file && !file_saved);

        int key = wgetch(win);

        if (key == 3) { // Ctrl-C to terminate
            break;
        } else if (key == 19) { // Ctrl-S to save
            if (save_file(filename, text)) {
                draw_text(win, text, cursor_y, cursor_x, "File saved successfully.");
                file_saved = true;
            } else {
                draw_text(win, text, cursor_y, cursor_x, "Error saving file.");
            }
            wgetch(win); // Wait for a key press before continuing
        } else if (key == 17) { // Ctrl-Q to quit
            draw_text(win, text, cursor_y, cursor_x, "Goodbye!");
            wgetch(win); // Wait for a key press before exiting
            break;
        } else if (key == 127 || key == KEY_BACKSPACE) { // Handle backspace
            if (cursor_x > 0) {
                text[cursor_y - 1].erase(cursor_x - 1, 1);
                cursor_x--;
            } else if (cursor_y > 1) {
                cursor_x = text[cursor_y - 2].length();
                text[cursor_y - 2] += text[cursor_y - 1];
                text.erase(text.begin() + cursor_y - 1);
                cursor_y--;
            }
        } else if (key == '\n' || key == KEY_ENTER) { // Handle new line
            if (cursor_y > (int)text.size()) {
                text.push_back("");
            } else {
                text.insert(text.begin() + cursor_y, text[cursor_y - 1].substr(cursor_x));
                text[cursor_y - 1] = text[cursor_y - 1].substr(0, cursor_x);
            }
            cursor_y++;
            cursor_x = 0;
            if (new_file && !file_saved) {
                draw_text(win, text, cursor_y, cursor_x, "New File", true);
                wgetch(win); // Wait for a key press before continuing
            }
        } else if (key == 67) { // Ctrl-Shift-C (uppercase C)
            if (home) {
                if (cursor_y <= (int)text.size()) {
                    clipboard_copy(text[cursor_y - 1]);
                    draw_text(win, text, cursor_y, cursor_x, "Line copied to clipboard.");
                    wgetch(win); // Wait for a key press before continuing
                }
            } else {
                draw_text(win, text, cursor_y, cursor_x, "Clipboard access denied. Relaunch with sudo -E.");
                wgetch(win); // Wait for a key press before continuing
            }
        } else if (key == 86) { // Ctrl-Shift-V (uppercase V)
            if (home) {
                string clipboard_text = clipboard_paste();
                if (!clipboard_text.empty()) {
                    istringstream ss(clipboard_text);
                    string line;
                    while (getline(ss, line)) {
                        if (cursor_y > (int)text.size()) {
                            text.push_back(line);
                        } else {
                            text[cursor_y - 1].insert(cursor_x, line);
                        }
                        cursor_y++;
                    }
                    cursor_y = min(cursor_y - 1, (int)text.size());
                    cursor_x = text[cursor_y - 1].length();
                }
            } else {
                draw_text(win, text, cursor_y, cursor_x, "Clipboard access denied. Relaunch with sudo -E.");
                wgetch(win); // Wait for a key press before continuing
            }
        } else {
            tie(cursor_y, cursor_x) = move_cursor(key, cursor_y, cursor_x, text, max_y, max_x);
            if (32 <= key && key <= 126) {
                if (cursor_y <= (int)text.size()) {
                    text[cursor_y - 1].insert(cursor_x, 1, (char)key);
                } else {
                    text.push_back(string(1, (char)key));
                }
                cursor_x++;
            }
        }
    }

    // Clean up ncurses
    noraw();
    keypad(win, FALSE);
    echo();
    curs_set(0);

    // Print goodbye message to the terminal and wait before exiting
    endwin(); // Ensure ncurses cleanup is complete
    cout << "Goodbye!" << endl;

}

int main(int argc, char* argv[]) {
    const char* home_env = getenv("HOME");
    home = home_env && string(home_env).find("/home/") == 0;

    string filename = "untitled.txt";
    if (argc > 1) {
        filename = argv[1];
    }

    initscr();
    WINDOW* win = newwin(LINES, COLS, 0, 0);
    main_loop(win, filename);
    delwin(win);
    endwin();

    return 0;
}


