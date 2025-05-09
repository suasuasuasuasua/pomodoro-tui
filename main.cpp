#include <ncurses.h>  // ncurses library for terminal UI

#include <atomic>  // For atomic variables (thread-safe booleans)
#include <chrono>  // C++ time utilities (like Python's time module)
#include <string>  // For using std::string (like Python's str)
#include <thread>  // For sleeping (like time.sleep in Python)
#include <vector>  // For using std::vector (like Python's list)

using namespace std::chrono;  // Use chrono types without std::chrono:: prefix

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

// Atomic booleans for timer state
namespace {
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables) //
// Required for ncurses TUI state
std::atomic<bool> running{false};
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables) //
// Required for ncurses TUI state
std::atomic<bool> paused{false};
}  // namespace

// Struct to represent a timer option (label, minutes, seconds)
struct TimerOption {
  std::string label;
  int minutes;
  int seconds;
};

// Structs to group related parameters and avoid swappable-parameter warnings
struct SessionTime {
  int minutes;
  int seconds;
};

struct TimerTickState {
  int total_seconds;
  int elapsed_us;
};

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

// Presents a menu for the user to select an option using arrow keys and enter
// Returns -1 if the user selects the quit option
auto prompt_selection(const std::string& prompt,
                      const std::vector<std::string>& options,
                      bool allow_quit = false) -> int {
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

// Prompts the user after a session, showing session durations and allowing exit
auto prompt_continue(const char* msg, const SessionTime& study,
                     const SessionTime& brk, bool is_break) -> bool {
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

// Draws the main timer UI
void draw(int minutes, int seconds, const std::string& status) {
  clear();
  // NOLINTNEXTLINE(cppcoreguidelines-pro-type-vararg) // ncurses API
  mvprintw(kMenuPromptRow, 2, "Pomodoro Timer");
  // NOLINTNEXTLINE(cppcoreguidelines-pro-type-vararg) // ncurses API
  mvprintw(kTimeRow, 2, "Time: %02d:%02d", minutes, seconds);
  // NOLINTNEXTLINE(cppcoreguidelines-pro-type-vararg) // ncurses API
  mvprintw(kControlRow, 2, "[s] Start/Pause  [r] Reset  [q] Quit");
  // NOLINTNEXTLINE(cppcoreguidelines-pro-type-vararg) // ncurses API
  mvprintw(kStatusRow, 2, "Status: %s", status.c_str());
  refresh();
}

// Returns true if the timer has finished, false otherwise
auto timer_tick(TimerTickState& state, int interval_us) -> bool {
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
auto handle_session_transition(bool& on_break, SessionTime& current,
                               TimerTickState& tick_state,
                               const SessionTime& pomodoro,
                               const SessionTime& brk,
                               std::string& status) -> bool {
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

// Extracted event loop to reduce main() complexity
void pomodoro_event_loop(
    const SessionTime& pomodoro,
    const SessionTime&
        brk)  // NOLINT(readability-function-cognitive-complexity)
              // TUI event loop is inherently complex
{
  SessionTime current = pomodoro;
  TimerTickState tick_state{
      current.minutes * kSecondsPerMinute + current.seconds, 0};
  std::string status = "Stopped";
  bool on_break = false;
  draw(current.minutes, current.seconds, status);
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
      draw(display_minutes, display_seconds, status);
      std::this_thread::sleep_for(std::chrono::microseconds(kIntervalUs));
    } else {
      std::this_thread::sleep_for(std::chrono::microseconds(kIntervalUs));
      int display_total_us = tick_state.total_seconds * kMicrosecondsPerSecond;
      int display_minutes = display_total_us / kMicrosecondsPerMinute;
      int display_seconds =
          (display_total_us / kMicrosecondsPerSecond) % kSecondsPerMinute;
      draw(display_minutes, display_seconds, status);
    }
  }
}

// Main application loop for the Pomodoro timer
auto main(int argc, char* argv[]) -> int {
  initscr();
  cbreak();
  noecho();
  nodelay(stdscr, FALSE);
  keypad(stdscr, TRUE);

  // Check for debug flag
  bool debug_mode = false;
  for (int arg_index = 1; arg_index < argc; ++arg_index) {
    // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic) //
    // Standard C++ argv usage
    if (std::string(argv[arg_index]) == "--debug") {
      debug_mode = true;
      break;
    }
  }

  // Menu options for study and break durations using struct-based vectors
  std::vector<TimerOption> study_options = {
      {"25:00 (Short Study)", kShortStudyMinutes, 0},
      {"50:00 (Long Study)", kLongStudyMinutes, 0}};
  std::vector<TimerOption> break_options = {
      {"5:00 (Short Break)", kShortBreakMinutes, 0},
      {"10:00 (Long Break)", kLongBreakMinutes, 0}};
  if (debug_mode) {
    study_options.push_back({"0:10 (Debug Study)", 0, kDebugStudySeconds});
    break_options.push_back({"0:05 (Debug Break)", 0, kDebugBreakSeconds});
  }

  std::vector<std::string> study_labels;
  std::vector<std::string> break_labels;
  study_labels.reserve(study_options.size());
  break_labels.reserve(break_options.size());
  for (const auto& opt : study_options) {
    study_labels.push_back(opt.label);
  }
  for (const auto& opt : break_options) {
    break_labels.push_back(opt.label);
  }

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

  SessionTime pomodoro{study_options[study_choice].minutes,
                       study_options[study_choice].seconds};
  SessionTime brk{break_options[break_choice].minutes,
                  break_options[break_choice].seconds};
  pomodoro_event_loop(pomodoro, brk);
  endwin();
  return 0;
}
