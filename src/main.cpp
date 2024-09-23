
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <ncurses.h>
#include <pwd.h>
#include <sstream>
#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <sys/types.h>
#include <unistd.h>
#include <vector>

using namespace std;

bool home = true;
bool is_root = (getuid() == 0);

void draw_text(WINDOW *win, const vector<string> &text, int cursor_y,
               int cursor_x, const string &message = "",
               bool new_file = false) {
  wclear(win);
  int max_y, max_x;
  getmaxyx(win, max_y, max_x);

  // Draw top and bottom bars
  string top_bar = "Luced v.2.2 - Terminal Text Editor";
  string bottom_bar =
      "Ctrl + Shift + V: Paste Clipboard Content  Ctrl + S: Save File  Ctrl + "
      "Q: Exit  Ctrl + Shift + C: Copy to Clipboard";

  // Center the top bar
  int top_bar_x = (max_x - top_bar.length()) / 2;
  wattron(win, A_BOLD | A_REVERSE);
  mvwaddnstr(win, 0, top_bar_x, top_bar.c_str(), max_x - top_bar_x);
  wattroff(win, A_BOLD | A_REVERSE);

  // Center the bottom bar with highlighting
  int bottom_bar_x = (max_x - bottom_bar.length()) / 2;
  wattron(win, A_BOLD | A_REVERSE);
  mvwaddnstr(win, max_y - 1, bottom_bar_x, bottom_bar.c_str(),
             max_x - bottom_bar_x);
  wattroff(win, A_BOLD | A_REVERSE);

  // Display "New File" message if applicable with highlighting
  if (new_file) {
    string new_file_msg = "New File";
    wattron(win, A_BOLD | A_REVERSE);
    mvwaddnstr(win, max_y - 2, (max_x - new_file_msg.length()) / 2,
               new_file_msg.c_str(),
               max_x - (max_x - new_file_msg.length()) / 2);
    wattroff(win, A_BOLD | A_REVERSE);
  }

  // Display a message if needed with highlighting
  if (!message.empty()) {
    wattron(win, A_BOLD | A_REVERSE);
    mvwaddnstr(win, max_y / 2, (max_x - message.length()) / 2, message.c_str(),
               max_x - (max_x - message.length()) / 2);
    wattroff(win, A_BOLD | A_REVERSE);
  }

  // Display text within the window
  int start_y = 1; // Start displaying text below the top bar
  int end_y = min(
      (int)text.size(),
      max_y - 2); // Display text up to window height minus top and bottom bars

  for (int i = 0; i < end_y; ++i) {
    string line = text[i];
    size_t start_pos = 0;
    size_t length = (size_t)max_x;
    while (start_pos < line.size()) {
      mvwaddnstr(win, i + 1, 0, line.substr(start_pos, length).c_str(),
                 (int)length);
      start_pos += length;
      length = min((size_t)max_x, line.size() - start_pos);
    }
  }

  // Move the cursor to the necessary position
  wmove(win, cursor_y - start_y + 1, cursor_x);
  wrefresh(win);
}

pair<int, int> move_cursor(int key, int cursor_y, int cursor_x,
                           const vector<string> &text, int max_y, int max_x) {
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

void insert_text(vector<string> &text, int &cursor_y, int &cursor_x, char key,
                 int max_x) {
  if (cursor_y > (int)text.size()) {
    text.push_back(string(1, key));
    cursor_y = text.size();
    cursor_x = 1;
  } else {
    string &current_line = text[cursor_y - 1];
    current_line.insert(cursor_x, 1, key);
    cursor_x++;

    // Perform line wrapping based on the max_x (window width) rather than
    // cursor position
    if (current_line.length() > (size_t)max_x) {
      // Find the position where the line should wrap
      size_t wrap_position = max_x;
      string new_line = current_line.substr(wrap_position);
      current_line = current_line.substr(0, wrap_position);

      // Insert the overflow into a new line in the text
      text.insert(text.begin() + cursor_y, new_line);
      cursor_y++;   // Move the cursor to the next line
      cursor_x = 0; // Reset cursor position to the start of the new line
    }
  }
}

void handle_key_input(vector<string> &text, int &cursor_y, int &cursor_x,
                      int key, int max_x, int max_y) {
  if (key >= 32 && key <= 126) { // Printable characters
    insert_text(text, cursor_y, cursor_x, static_cast<char>(key), max_x);
  } else if (key == KEY_BACKSPACE || key == 127) { // Backspace key
    if (cursor_x > 0 || cursor_y > 1) {
      if (cursor_x > 0) {
        string &current_line = text[cursor_y - 1];
        current_line.erase(cursor_x - 1, 1);
        cursor_x--;
      } else if (cursor_y > 1) {
        cursor_y--;
        cursor_x = text[cursor_y - 1].length();
        text[cursor_y - 1] += text[cursor_y];
        text.erase(text.begin() + cursor_y);
      }
    }
  } else if (key == KEY_DC) { // Delete key
    if (cursor_x < (int)text[cursor_y - 1].length()) {
      text[cursor_y - 1].erase(cursor_x, 1);
    } else if (cursor_y < (int)text.size()) {
      text[cursor_y - 1] += text[cursor_y];
      text.erase(text.begin() + cursor_y);
    }
  } else if (key == KEY_ENTER || key == 10) { // Enter key
    if (cursor_y == (int)text.size()) {
      text.push_back("");
    } else {
      string &current_line = text[cursor_y - 1];
      text[cursor_y - 1] = current_line.substr(0, cursor_x);
      text.insert(text.begin() + cursor_y, current_line.substr(cursor_x));
    }
    cursor_y++;
    cursor_x = 0;
  } else {
    tie(cursor_y, cursor_x) =
        move_cursor(key, cursor_y, cursor_x, text, max_y, max_x);
  }
}

void clipboard_copy(const string &text) {
  string command = "wl-copy <<< \"" + text + "\"";
  system(command.c_str());
}

string clipboard_paste() {
  char buffer[128];
  string result;
  FILE *pipe = popen("wl-paste", "r");
  if (pipe) {
    while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
      result += buffer;
    }
    pclose(pipe);
  }
  return result;
}

bool save_file(const string &filename, const vector<string> &text) {
  ofstream file(filename);
  if (!file.is_open()) {
    cerr << "Error saving file: " << filename << endl;
    return false;
  }
  for (const auto &line : text) {
    file << line << endl;
  }
  return true;
}

vector<string> load_file(const string &filename) {
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

void main_loop(WINDOW *win, const string &filename) {
  raw();             // Disable line buffering
  keypad(win, TRUE); // Enable special keys
  noecho();          // Don't echo input
  curs_set(1);       // Show cursor

  vector<string> text = load_file(filename);
  int cursor_x = 0, cursor_y = 1;
  bool running = true;

  bool new_file = text.size() == 1 && text[0].empty();
  bool file_saved = !new_file;

  // Check if running as root and display warning
  if (is_root) {
    draw_text(win, text, cursor_y, cursor_x, "Warning: Running as root!",
              new_file);
    usleep(2000000); // Show the message for 2 seconds
  }

  while (running) {
    draw_text(win, text, cursor_y, cursor_x, "", new_file && !file_saved);

    int ch = wgetch(win);
    switch (ch) {
    case 24: // Ctrl + X (Exit)
    case 17: // Ctrl + Q (Quit)
      running = false;
      break;

    case 19: { // Ctrl + S (Save)
      if (save_file(filename, text)) {
        file_saved = true;
        draw_text(win, text, cursor_y, cursor_x, "File Saved!", new_file);
        usleep(500000); // Show message for 0.5 seconds
      }
      break;
    }

    case 3: { // Ctrl + C (Copy to clipboard)
      if (cursor_y > 0 && cursor_y <= (int)text.size()) {
        clipboard_copy(text[cursor_y - 1]);
        draw_text(win, text, cursor_y, cursor_x, "Copied to Clipboard",
                  new_file);
        usleep(500000); // Show message for 0.5 seconds
      }
      break;
    }

    case 22: { // Ctrl + V (Paste from clipboard)
      string clipboard_content = clipboard_paste();
      if (!clipboard_content.empty()) {
        insert_text(text, cursor_y, cursor_x, clipboard_content[0],
                    COLS); // Insert the first character only
        draw_text(win, text, cursor_y, cursor_x, "Pasted Clipboard Content",
                  new_file);
        usleep(500000); // Show message for 0.5 seconds
      }
      break;
    }

    default:
      handle_key_input(text, cursor_y, cursor_x, ch, COLS, LINES);
      file_saved = false;
      break;
    }
  }

  endwin();
}

int main(int argc, char **argv) {
  const char *home_dir = getenv("HOME");
  if (!home_dir) {
    home_dir = getpwuid(getuid())->pw_dir;
  }

  initscr(); // Initialize ncurses
  const char *filename = (argc > 1) ? argv[1] : "untitled.txt";
  WINDOW *win = newwin(LINES, COLS, 0, 0);
  main_loop(win, filename);
  delwin(win);
  endwin();

  return 0;
}
