#include "pomodoro.h"

#include <ncurses.h>

#include <atomic>
#include <chrono>
#include <string>
#include <thread>
#include <vector>

using namespace std::chrono;

// Magic numbers and UI constants
constexpr int kMenuPromptRow = 1;
constexpr int kMenuOptionStartRow = 3;
constexpr int kMenuHelpRowOffset = 1;
constexpr int kBreakPromptRow = 3;
constexpr int kBreakHelpRow = 5;
constexpr int kStatusRow = 7;
constexpr int kControlRow = 5;
constexpr int kTimeRow = 3;
constexpr int kSecondsPerMinute = 60;
constexpr int kMicrosecondsPerSecond = 1'000'000;
constexpr int kMicrosecondsPerMinute = 60'000'000;
constexpr int kIntervalUs = 10'000;
constexpr int kShortStudyMinutes = 25;
constexpr int kLongStudyMinutes = 50;
constexpr int kShortBreakMinutes = 5;
constexpr int kLongBreakMinutes = 10;
constexpr int kDebugStudySeconds = 10;
constexpr int kDebugBreakSeconds = 5;
constexpr int kEnterKey = 10;

namespace {
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables) //
// Required for ncurses TUI state
std::atomic<bool> running{false};
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables) //
// Required for ncurses TUI state
std::atomic<bool> paused{false};
}  // namespace

// Presents a menu for the user to select an option using arrow keys and enter
// Returns -1 if the user selects the quit option
int prompt_selection(const std::string& prompt,
                     const std::vector<std::string>& options, bool allow_quit) {
  int choice = 0;
  int key_code = 0;
  int num_options = static_cast<int>(options.size());
  int max_choice = allow_quit ? num_options : num_options - 1;
  while (true) {
    draw_menu(prompt, options, choice, allow_quit);
    key_code = getch();
    if (key_code == KEY_UP) {
      choice = (choice - 1 + (max_choice + 1)) % (max_choice + 1);
    } else if (key_code == KEY_DOWN) {
      choice = (choice + 1) % (max_choice + 1);
    } else if (key_code == '\n' || key_code == '\r' || key_code == kEnterKey) {
      if (allow_quit && choice == num_options) {
        return -1;
      }
      return choice;
    }
  }
}

// Prompts the user after a session, showing session durations and allowing exit
bool prompt_continue(const char* msg, const SessionTime& study,
                     const SessionTime& brk, bool is_break) {
  clear();
  // NOLINTNEXTLINE(cppcoreguidelines-pro-type-vararg) // ncurses API
  mvprintw(kBreakPromptRow, 2, "%s", msg);
  if (is_break) {
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-vararg) // ncurses API
    mvprintw(kBreakHelpRow, 2, "Break time: %02d:%02d", brk.minutes,
             brk.seconds);
  } else {
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-vararg) // ncurses API
    mvprintw(kBreakHelpRow, 2, "Study time: %02d:%02d", study.minutes,
             study.seconds);
  }
  // NOLINTNEXTLINE(cppcoreguidelines-pro-type-vararg) // ncurses API
  mvprintw(kStatusRow, 2, "Press any key to continue, or 'q' to exit...");
  refresh();
  nodelay(stdscr, FALSE);
  int key_code = getch();
  nodelay(stdscr, TRUE);
  return key_code != 'q' && key_code != 'Q';
}

// Returns true if the timer has finished, false otherwise
bool timer_tick(TimerTickState& state, int interval_us) {
  state.elapsed_us += interval_us;
  if (state.elapsed_us >= kMicrosecondsPerSecond) {
    state.elapsed_us = 0;
    if (state.total_seconds > 0) {
      --state.total_seconds;
    } else {
      return true;
    }
  }
  return false;
}

// Handles the transition between study and break sessions
bool handle_session_transition(bool& on_break, SessionTime& current,
                               TimerTickState& tick_state,
                               const SessionTime& pomodoro,
                               const SessionTime& brk, std::string& status) {
  if (!on_break) {
    on_break = true;
    current = brk;
    tick_state = {current.minutes * kSecondsPerMinute + current.seconds, 0};
    status = "Break Ready";
    if (!prompt_continue("Study session complete! Time for a break.", pomodoro,
                         brk, true)) {
      return false;
    }
    status = "Break Running";
    running = true;
  } else {
    if (!prompt_continue(
            "Break complete! Press any key to start a new study session.",
            pomodoro, brk, false)) {
      return false;
    }
    on_break = false;
    current = pomodoro;
    tick_state = {current.minutes * kSecondsPerMinute + current.seconds, 0};
    status = "Running";
    running = true;
  }
  return true;
}

void pomodoro_event_loop(const SessionTime& pomodoro, const SessionTime& brk) {
  SessionTime current = pomodoro;
  TimerTickState tick_state{
      current.minutes * kSecondsPerMinute + current.seconds, 0};
  const int initial_total_seconds = tick_state.total_seconds;
  std::string status = "Stopped";
  bool on_break = false;
  draw(current.minutes, current.seconds, status, initial_total_seconds,
       tick_state.total_seconds);
  while (true) {
    int key_code = getch();
    if (key_code == 'q') {
      break;
    }
    if (key_code == 's') {
      if (!running) {
        running = true;
        paused = false;
        status = on_break ? "Break Running" : "Running";
      } else {
        paused = !paused;
        if (paused) {
          status = on_break ? "Break Paused" : "Paused";
        } else {
          status = on_break ? "Break Running" : "Running";
        }
      }
    }
    if (key_code == 'r') {
      running = false;
      paused = false;
      if (on_break) {
        current = brk;
      } else {
        current = pomodoro;
      }
      tick_state = {current.minutes * kSecondsPerMinute + current.seconds, 0};
      status = on_break ? "Break Stopped" : "Stopped";
    }
    if (running && !paused) {
      if (timer_tick(tick_state, kIntervalUs)) {
        running = false;
        if (!handle_session_transition(on_break, current, tick_state, pomodoro,
                                       brk, status)) {
          break;
        }
      }
      int remaining_us = tick_state.total_seconds * kMicrosecondsPerSecond -
                         tick_state.elapsed_us;
      if (remaining_us < 0) {
        remaining_us = 0;
      }
      int display_total_us = remaining_us;
      int display_minutes = display_total_us / kMicrosecondsPerMinute;
      int display_seconds =
          (display_total_us / kMicrosecondsPerSecond) % kSecondsPerMinute;
      draw(display_minutes, display_seconds, status, initial_total_seconds,
           tick_state.total_seconds);
      std::this_thread::sleep_for(std::chrono::microseconds(kIntervalUs));
    } else {
      std::this_thread::sleep_for(std::chrono::microseconds(kIntervalUs));
      int display_total_us = tick_state.total_seconds * kMicrosecondsPerSecond;
      int display_minutes = display_total_us / kMicrosecondsPerMinute;
      int display_seconds =
          (display_total_us / kMicrosecondsPerSecond) % kSecondsPerMinute;
      draw(display_minutes, display_seconds, status, initial_total_seconds,
           tick_state.total_seconds);
    }
  }
}

// Helper to draw the menu options (reduces cognitive complexity)
void draw_menu(const std::string& prompt,
               const std::vector<std::string>& options, int choice,
               bool allow_quit) {
  clear();
  // NOLINTNEXTLINE(cppcoreguidelines-pro-type-vararg) // ncurses API
  mvprintw(kMenuPromptRow, 2, "%s", prompt.c_str());
  int num_options = static_cast<int>(options.size());
  for (int i = 0; i < num_options; ++i) {
    if (i == choice) {
      attron(A_REVERSE);
    }
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-vararg) // ncurses API
    mvprintw(kMenuOptionStartRow + i, 4, "%s", options[i].c_str());
    if (i == choice) {
      attroff(A_REVERSE);
    }
  }
  int quit_row = kMenuOptionStartRow + num_options;
  if (allow_quit) {
    if (choice == num_options) {
      attron(A_REVERSE);
    }
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-vararg) // ncurses API
    mvprintw(quit_row, 4, "Quit");
    if (choice == num_options) {
      attroff(A_REVERSE);
    }
  }
  // NOLINTNEXTLINE(cppcoreguidelines-pro-type-vararg) // ncurses API
  mvprintw(quit_row + kMenuHelpRowOffset, 2,
           "Use UP/DOWN to select, ENTER to confirm");
  refresh();
}

// Prompts the user to start a break, blocking until a key is pressed
void prompt_break(const char* break_msg) {
  clear();
  // NOLINTNEXTLINE(cppcoreguidelines-pro-type-vararg) // ncurses API
  mvprintw(kBreakPromptRow, 2, "%s", break_msg);
  // NOLINTNEXTLINE(cppcoreguidelines-pro-type-vararg) // ncurses API
  mvprintw(kBreakHelpRow, 2, "Press any key to start break timer...");
  refresh();
  nodelay(stdscr, FALSE);
  getch();
  nodelay(stdscr, TRUE);
}

// Draws the main timer UI with a progress bar
void draw(int minutes, int seconds, const std::string& status,
          int total_seconds, int remaining_seconds) {
  constexpr int kBarWidth = 40;
  clear();
  // NOLINTNEXTLINE(cppcoreguidelines-pro-type-vararg) // ncurses API
  mvprintw(kMenuPromptRow, 2, "Pomodoro Timer");
  // NOLINTNEXTLINE(cppcoreguidelines-pro-type-vararg) // ncurses API
  mvprintw(kTimeRow, 2, "Time: %02d:%02d", minutes, seconds);
  // Draw progress bar
  int elapsed = total_seconds - remaining_seconds;
  int fill = 0;
  if (total_seconds > 0) {
    if (remaining_seconds == 0 && elapsed > 0) {
      fill = kBarWidth;
    } else {
      fill = (elapsed * kBarWidth) / total_seconds;
    }
  }
  std::string bar =
      "[" + std::string(fill, '#') + std::string(kBarWidth - fill, ' ') + "]";
  // NOLINTNEXTLINE(cppcoreguidelines-pro-type-vararg) // ncurses API
  mvprintw(kTimeRow + 1, 2, "%s", bar.c_str());
  // NOLINTNEXTLINE(cppcoreguidelines-pro-type-vararg) // ncurses API
  mvprintw(kControlRow, 2, "[s] Start/Pause  [r] Reset  [q] Quit");
  // NOLINTNEXTLINE(cppcoreguidelines-pro-type-vararg) // ncurses API
  mvprintw(kStatusRow, 2, "Status: %s", status.c_str());
  refresh();
}
