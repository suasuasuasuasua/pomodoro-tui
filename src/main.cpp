#include <ncurses.h>

#include <string>
#include <vector>

#include "pomodoro.h"

int main(int argc, char* argv[]) {
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
  std::vector<TimerOption> study_options = {{"25:00 (Short Study)", 25, 0},
                                            {"50:00 (Long Study)", 50, 0}};
  std::vector<TimerOption> break_options = {{"5:00 (Short Break)", 5, 0},
                                            {"10:00 (Long Break)", 10, 0}};
  if (debug_mode) {
    study_options.push_back({"0:10 (Debug Study)", 0, 10});
    break_options.push_back({"0:05 (Debug Break)", 0, 5});
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
