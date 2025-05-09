#include <ncurses.h>  // ncurses library for terminal UI

#include <atomic>  // For atomic variables (thread-safe booleans)
#include <chrono>  // C++ time utilities (like Python's time module)
#include <string>  // For using std::string (like Python's str)
#include <thread>  // For sleeping (like time.sleep in Python)
#include <vector>  // For using std::vector (like Python's list)

using namespace std::chrono;  // Use chrono types without std::chrono:: prefix

// Atomic booleans for timer state
std::atomic<bool> running(false);
std::atomic<bool> paused(false);

// Struct to represent a timer option (label, minutes, seconds)
struct TimerOption {
  std::string label;
  int minutes;
  int seconds;
};

// Presents a menu for the user to select an option using arrow keys and enter
// Returns -1 if the user selects the quit option
int prompt_selection(const std::string& prompt,
                     const std::vector<std::string>& options,
                     bool allow_quit = false) {
  int choice = 0;
  int ch;
  int num_options = static_cast<int>(options.size());
  while (1) {
    clear();
    mvprintw(1, 2, "%s", prompt.c_str());
    for (int i = 0; i < num_options; ++i) {
      if (i == choice) attron(A_REVERSE);
      mvprintw(3 + i, 4, "%s", options[i].c_str());
      if (i == choice) attroff(A_REVERSE);
    }
    int quit_row = 3 + num_options;
    if (allow_quit) {
      if (choice == num_options) attron(A_REVERSE);
      mvprintw(quit_row, 4, "Quit");
      if (choice == num_options) attroff(A_REVERSE);
    }
    mvprintw(quit_row + 1, 2, "Use UP/DOWN to select, ENTER to confirm");
    refresh();
    int max_choice = allow_quit ? num_options : num_options - 1;
    ch = getch();
    if (ch == KEY_UP) {
      choice = (choice - 1 + (max_choice + 1)) % (max_choice + 1);
    } else if (ch == KEY_DOWN) {
      choice = (choice + 1) % (max_choice + 1);
    } else if (ch == '\n' || ch == '\r' || ch == 10) {
      if (allow_quit && choice == num_options) return -1;
      return choice;
    }
  }
}

// Prompts the user to start a break, blocking until a key is pressed
void prompt_break(const char* break_msg) {
  clear();
  mvprintw(3, 2, "%s", break_msg);
  mvprintw(5, 2, "Press any key to start break timer...");
  refresh();
  nodelay(stdscr, FALSE);
  getch();
  nodelay(stdscr, TRUE);
}

// Prompts the user after a session, showing session durations and allowing exit
bool prompt_continue(const char* msg, int study_min, int study_sec,
                     int break_min, int break_sec, bool is_break) {
  clear();
  mvprintw(3, 2, "%s", msg);
  if (is_break) {
    mvprintw(5, 2, "Break time: %02d:%02d", break_min, break_sec);
  } else {
    mvprintw(5, 2, "Study time: %02d:%02d", study_min, study_sec);
  }
  mvprintw(7, 2, "Press any key to continue, or 'q' to exit...");
  refresh();
  nodelay(stdscr, FALSE);
  int ch = getch();
  nodelay(stdscr, TRUE);
  return ch != 'q' && ch != 'Q';
}

// Draws the main timer UI
void draw(int minutes, int seconds, const std::string& status) {
  clear();
  mvprintw(1, 2, "Pomodoro Timer");
  mvprintw(3, 2, "Time: %02d:%02d", minutes, seconds);
  mvprintw(5, 2, "[s] Start/Pause  [r] Reset  [q] Quit");
  mvprintw(7, 2, "Status: %s", status.c_str());
  refresh();
}

// Returns true if the timer has finished, false otherwise
bool timer_tick(int& total_seconds, int& elapsed_us, int interval_us) {
  elapsed_us += interval_us;
  if (elapsed_us >= 1'000'000) {
    elapsed_us = 0;
    if (total_seconds > 0) {
      --total_seconds;
    } else {
      return true;
    }
  }
  return false;
}

// Handles the transition between study and break sessions
bool handle_session_transition(bool& on_break, int& minutes, int& seconds,
                               int& elapsed_us, int& total_seconds,
                               int pomodoro_minutes, int pomodoro_seconds,
                               int break_minutes, int break_seconds,
                               std::string& status) {
  if (!on_break) {
    on_break = true;
    minutes = break_minutes;
    seconds = break_seconds;
    elapsed_us = 0;
    total_seconds = minutes * 60 + seconds;
    status = "Break Ready";
    if (!prompt_continue("Study session complete! Time for a break.",
                         pomodoro_minutes, pomodoro_seconds, break_minutes,
                         break_seconds, true))
      return false;
    status = "Break Running";
    running = true;
  } else {
    if (!prompt_continue(
            "Break complete! Press any key to start a new study session.",
            pomodoro_minutes, pomodoro_seconds, break_minutes, break_seconds,
            false))
      return false;
    on_break = false;
    minutes = pomodoro_minutes;
    seconds = pomodoro_seconds;
    elapsed_us = 0;
    total_seconds = minutes * 60 + seconds;
    status = "Running";
    running = true;
  }
  return true;
}

// Main application loop for the Pomodoro timer
int main(int argc, char* argv[]) {
  initscr();
  cbreak();
  noecho();
  nodelay(stdscr, FALSE);
  keypad(stdscr, TRUE);

  // Check for debug flag
  bool debug_mode = false;
  for (int i = 1; i < argc; ++i) {
    if (std::string(argv[i]) == "--debug") {
      debug_mode = true;
      break;
    }
  }

  // Menu options for study and break durations using struct-based vectors
  std::vector<TimerOption> study_options = {{"25:00 (Short Study)", 25, 0},
                                            {"50:00 (Long Study)", 50, 0}};
  std::vector<TimerOption> break_options = {{"5:00 (Short Break)", 5, 0},
                                            {"10:00 (Long Break)", 10, 0}};
  if (debug_mode) {
    study_options.push_back({"0:10 (Debug Study)", 0, 10});
    break_options.push_back({"0:05 (Debug Break)", 0, 5});
  }

  // Extract labels for menu display
  std::vector<std::string> study_labels, break_labels;
  for (const auto& opt : study_options) study_labels.push_back(opt.label);
  for (const auto& opt : break_options) break_labels.push_back(opt.label);

  int study_choice = prompt_selection("Select Study Time:", study_labels, true);
  if (study_choice == -1) {
    endwin();
    return 0;
  }
  int break_choice = prompt_selection("Select Break Time:", break_labels, true);
  if (break_choice == -1) {
    endwin();
    return 0;
  }

  nodelay(stdscr, TRUE);

  int pomodoro_minutes = study_options[study_choice].minutes;
  int pomodoro_seconds = study_options[study_choice].seconds;
  int break_minutes = break_options[break_choice].minutes;
  int break_seconds = break_options[break_choice].seconds;
  int minutes = pomodoro_minutes;
  int seconds = pomodoro_seconds;
  int elapsed_us = 0;
  constexpr int interval_us = 10'000;
  int total_seconds = minutes * 60 + seconds;
  std::string status = "Stopped";
  bool on_break = false;

  draw(minutes, seconds, status);

  while (true) {
    int ch = getch();
    if (ch == 'q') break;
    if (ch == 's') {
      if (!running) {
        running = true;
        paused = false;
        status = on_break ? "Break Running" : "Running";
      } else {
        paused = !paused;
        status = paused ? (on_break ? "Break Paused" : "Paused")
                        : (on_break ? "Break Running" : "Running");
      }
    }
    if (ch == 'r') {
      running = false;
      paused = false;
      if (on_break) {
        minutes = break_minutes;
        seconds = break_seconds;
      } else {
        minutes = pomodoro_minutes;
        seconds = pomodoro_seconds;
      }
      elapsed_us = 0;
      total_seconds = minutes * 60 + seconds;
      status = on_break ? "Break Stopped" : "Stopped";
    }

    if (running && !paused) {
      if (timer_tick(total_seconds, elapsed_us, interval_us)) {
        running = false;
        if (!handle_session_transition(on_break, minutes, seconds, elapsed_us,
                                       total_seconds, pomodoro_minutes,
                                       pomodoro_seconds, break_minutes,
                                       break_seconds, status))
          break;
      }
      int remaining_us = total_seconds * 1'000'000 - elapsed_us;
      if (remaining_us < 0) remaining_us = 0;
      int display_total_us = remaining_us;
      int display_minutes = static_cast<int>(display_total_us / 60'000'000);
      int display_seconds =
          (static_cast<int>(display_total_us / 1'000'000)) % 60;
      draw(display_minutes, display_seconds, status);
      std::this_thread::sleep_for(std::chrono::microseconds(interval_us));
    } else {
      std::this_thread::sleep_for(std::chrono::microseconds(interval_us));
      int display_total_us = total_seconds * 1'000'000;
      int display_minutes = static_cast<int>(display_total_us / 60'000'000);
      int display_seconds =
          (static_cast<int>(display_total_us / 1'000'000)) % 60;
      draw(display_minutes, display_seconds, status);
    }
  }

  endwin();
  return 0;
}
