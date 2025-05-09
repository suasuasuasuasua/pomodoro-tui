#pragma once

#include <string>
#include <vector>

void draw(int minutes, int seconds, const std::string& status,
          int total_seconds, int remaining_seconds);

struct TimerOption {
  std::string label;
  int minutes;
  int seconds;
};

struct SessionTime {
  int minutes;
  int seconds;
};

struct TimerTickState {
  int total_seconds;
  int elapsed_us;
};

int prompt_selection(const std::string& prompt,
                     const std::vector<std::string>& options,
                     bool allow_quit = false);
void draw_menu(const std::string& prompt,
               const std::vector<std::string>& options, int choice,
               bool allow_quit);
void prompt_break(const char* break_msg);
bool prompt_continue(const char* msg, const SessionTime& study,
                     const SessionTime& brk, bool is_break);
bool timer_tick(TimerTickState& state, int interval_us);
bool handle_session_transition(bool& on_break, SessionTime& current,
                               TimerTickState& tick_state,
                               const SessionTime& pomodoro,
                               const SessionTime& brk, std::string& status);
void pomodoro_event_loop(const SessionTime& pomodoro, const SessionTime& brk);
