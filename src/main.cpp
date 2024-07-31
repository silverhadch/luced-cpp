#include <ncurses.h>
#include <unistd.h>
#include <string>
#include <vector>
#include <fstream>
#include <sstream>
#include <iostream>
#include <cstdlib>

bool home = true;

void draw_text(WINDOW* win, const std::vector<std::string>& text, int cursor_y, int cursor_x, const std::string& message) {
    int max_y, max_x;
    getmaxyx(win, max_y, max_x);
    wclear(win);

    // Draw top and bottom bars
    std::string top_bar = "Luced v.2.1 - Terminal Text Editor";
    std::string bottom_bar = "Ctrl + Shift + V: Paste Clipboard Content  Ctrl + S: Save File  Ctrl + Q: Exit  Ctrl + Shift + C: Copy to Clipboard";

    // Center the top bar
    int top_bar_x = (max_x - top_bar.length()) / 2;
    wattron(win, A_BOLD);
    mvwaddnstr(win, 0, top_bar_x, top_bar.c_str(), max_x - top_bar_x);
    wattroff(win, A_BOLD);

    // Center the bottom bar
    int bottom_bar_x = (max_x - bottom_bar.length()) / 2;
    mvwaddnstr(win, max_y - 1, bottom_bar_x, bottom_bar.c_str(), max_x - bottom_bar_x);

    // Display a message if needed
    if (!message.empty()) {
        mvwaddnstr(win, max_y / 2, (max_x - message.length()) / 2, message.c_str(), max_x - (max_x - message.length()) / 2);
    }

    // Ensure cursor_y and cursor_x don't go out of bounds
    cursor_y = std::min(std::max(cursor_y, 1), (int)text.size() + 1);
    cursor_x = std::min(std::max(cursor_x, 0), max_x - 1);

    // Display text within the window
    int start_y = std::max(1, cursor_y - (max_y - 2) / 2);
    int end_y = std::min((int)text.size() + 1, start_y + (max_y - 2));

    for (int i = start_y - 1; i < end_y - 1; ++i) {
        mvwaddnstr(win, i - start_y + 1, 0, text[i].c_str(), max_x);
    }

    // Move the cursor to the necessary position
    wmove(win, cursor_y - start_y + 1, cursor_x);
    wrefresh(win);
}

std::pair<int, int> move_cursor(int key, int cursor_y, int cursor_x, const std::vector<std::string>& text, int max_y, int max_x) {
    int num_lines = text.size();

    if (key == KEY_LEFT) {
        if (cursor_x > 0) {
            cursor_x--;
        } else if (cursor_y > 1) {
            cursor_y--;
            cursor_x = std::min(cursor_x, (int)text[cursor_y - 2].length());
        }
    } else if (key == KEY_RIGHT) {
        if (cursor_x < (int)text[cursor_y - 1].length()) {
            cursor_x++;
        } else if (cursor_y < num_lines) {
            cursor_y++;
            cursor_x = std::min(cursor_x, (int)text[cursor_y - 1].length());
        }
    } else if (key == KEY_UP) {
        if (cursor_y > 1) {
            cursor_y--;
            cursor_x = std::min(cursor_x, (int)text[cursor_y - 1].length());
        }
    } else if (key == KEY_DOWN) {
        if (cursor_y < num_lines) {
            cursor_y++;
            cursor_x = std::min(cursor_x, (int)text[cursor_y - 1].length());
        }
    }

    cursor_x = std::min(cursor_x, max_x - 1);
    return {cursor_y, cursor_x};
}

void clipboard_copy(const std::string& text) {
    std::string command = "echo -n \"" + text + "\" | xclip -selection clipboard";
    std::system(command.c_str());
}

std::string clipboard_paste() {
    char buffer[128];
    std::string result;
    FILE* pipe = popen("xclip -selection clipboard -o", "r");
    if (pipe) {
        while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
            result += buffer;
        }
        pclose(pipe);
    }
    return result;
}

bool save_file(const std::string& filename, const std::vector<std::string>& text) {
    std::ofstream file(filename);
    if (!file.is_open()) {
        std::cerr << "Error saving file: " << filename << std::endl;
        return false;
    }
    for (const auto& line : text) {
        file << line << std::endl;
    }
    return true;
}

std::vector<std::string> load_file(const std::string& filename) {
    std::vector<std::string> text;
    std::ifstream file(filename);
    if (!file.is_open()) {
        std::ofstream new_file(filename);  // Create the file if it doesn't exist
        text.push_back("");
    } else {
        std::string line;
        while (std::getline(file, line)) {
            text.push_back(line);
        }
        if (text.empty()) {
            text.push_back("");
        }
    }
    return text;
}

void main_loop(WINDOW* win, const std::string& filename) {
    // Initialize window settings
    raw();           // Disable line buffering
    keypad(win, TRUE); // Enable special keys
    noecho();        // Don't echo input
    curs_set(1);     // Show cursor

    // Load file content
    std::vector<std::string> text = load_file(filename);

    // Initial values
    int cursor_x = 0;
    int cursor_y = 1; // Start cursor at line 1 (below the top bar)
    int max_y, max_x;
    getmaxyx(win, max_y, max_x);

    while (true) {
        draw_text(win, text, cursor_y, cursor_x, "");

        int key = wgetch(win);

        if (key == 3) { // Ctrl-C to terminate
            break;
        } else if (key == 22) { // Ctrl-V to paste (adjust for actual implementation context)
            if (home) {
                std::string clipboard_text = clipboard_paste();
                if (!clipboard_text.empty()) {
                    std::istringstream ss(clipboard_text);
                    std::string line;
                    while (std::getline(ss, line)) {
                        if (cursor_y > (int)text.size()) {
                            text.push_back(line);
                        } else {
                            text[cursor_y - 1].insert(cursor_x, line);
                        }
                        cursor_y++;
                    }
                    cursor_y = std::min(cursor_y - 1, (int)text.size());
                    cursor_x = text[cursor_y - 1].length();
                }
            } else {
                draw_text(win, text, cursor_y, cursor_x, "Clipboard access denied. Relaunch with sudo -E.");
                wgetch(win); // Wait for a key press before continuing
            }
        } else if (key == 19) { // Ctrl-S to save
            if (save_file(filename, text)) {
                draw_text(win, text, cursor_y, cursor_x, "File saved successfully.");
            } else {
                draw_text(win, text, cursor_y, cursor_x, "Error saving file.");
            }
            wgetch(win); // Wait for a key press before continuing
        } else if (key == 17) { // Ctrl-Q to quit
            break;
        } else if (key == KEY_BACKSPACE || key == 127) {
            if (cursor_x > 0) {
                text[cursor_y - 1].erase(cursor_x - 1, 1);
                cursor_x--;
            } else if (cursor_y > 1) {
                cursor_x = text[cursor_y - 2].length();
                text[cursor_y - 2] += text[cursor_y - 1];
                text.erase(text.begin() + cursor_y - 1);
                cursor_y--;
            }
        } else if (key == '\n' || key == KEY_ENTER) {
            if (cursor_y > (int)text.size()) {
                text.push_back("");
            } else {
                text.insert(text.begin() + cursor_y, text[cursor_y - 1].substr(cursor_x));
                text[cursor_y - 1] = text[cursor_y - 1].substr(0, cursor_x);
            }
            cursor_y++;
            cursor_x = 0;
        } else if (key == 3) { // Ctrl-C (copy)
            if (home && cursor_y <= (int)text.size()) {
                clipboard_copy(text[cursor_y - 1]);
                draw_text(win, text, cursor_y, cursor_x, "Line copied to clipboard.");
                wgetch(win); // Wait for a key press before continuing
            } else {
                draw_text(win, text, cursor_y, cursor_x, "Clipboard access denied. Relaunch with sudo -E.");
                wgetch(win); // Wait for a key press before continuing
            }
        } else {
            auto [new_cursor_y, new_cursor_x] = move_cursor(key, cursor_y, cursor_x, text, max_y, max_x);
            cursor_y = new_cursor_y;
            cursor_x = new_cursor_x;

            if (isprint(key)) {
                if (cursor_y <= (int)text.size()) {
                    text[cursor_y - 1].insert(cursor_x, 1, (char)key);
                } else {
                    text.push_back(std::string(1, (char)key));
                }
                cursor_x++;
            }
        }
    }

    // Reset terminal settings
    noraw();
    keypad(win, FALSE);
}

int main(int argc, char* argv[]) {
    std::string filename = "Test.txt";
    if (argc > 1) {
        filename = argv[1];
    }

    // Check if running as root
    if (geteuid() == 0) {
        std::string warning_root = "You are root! Proceed with caution!";
        std::string clipboard_warning = "Clipboard access denied. Relaunch with sudo -E.";

        initscr();
        int max_y, max_x;
        getmaxyx(stdscr, max_y, max_x);
        mvaddstr(max_y / 2 - 1, (max_x - warning_root.length()) / 2, warning_root.c_str());
        mvaddstr(max_y / 2, (max_x - clipboard_warning.length()) / 2, clipboard_warning.c_str());
        refresh();
        getch();
        endwin();

        home = false;
    }

    initscr();
    noecho();
    cbreak();
    keypad(stdscr, TRUE);
    main_loop(stdscr, filename);
    endwin();

    return 0;
}
