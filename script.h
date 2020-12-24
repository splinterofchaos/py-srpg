#pragma once

#include <vector>
#include <functional>

#include "action.h"

struct ScriptResult {
  enum Code {
    START,     // The default value of a script that hasn't run yet.
    EXIT,      // Exit the script (no more instruction or early exit).
    WAIT,      // Return early; do not advance.
    RETRY,     // Run the same line again (might be used by goto instructions).
    CONTINUE,  // Run the next line.
    ERROR,     // Stop due to errors.
    N_RESULTS
  } code;

  int goto_line = -1;

  ScriptResult(Code code) : code(code) { }
  ScriptResult(Code code, int goto_line) : code(code), goto_line(goto_line) { }
};

using ScriptFn = std::function<ScriptResult(Game&, ActionManager&)>;

class ScriptEngine {
 public:

 private:
  std::vector<ScriptFn> instructions_;
  unsigned int instruction_pointer_ = 0;
  ScriptResult last_result_ = ScriptResult::EXIT;

  ScriptResult run_impl(Game& game, ActionManager& manager);

 public:
  ScriptEngine() { }

  // Checks if we are currently executing a script.
  bool active();

  void clear() { instruction_pointer_ = 0; instructions_.clear(); }

  void reset(std::vector<ScriptFn> script) {
    instructions_ = std::move(script);
    instruction_pointer_ = 0;
    last_result_ = ScriptResult::START;
  }

  // Appends an instruction to the script.
  void add(ScriptFn f);

  ScriptResult run(Game& game, ActionManager& manager) {
    last_result_ = run_impl(game, manager);
    return last_result_;
  }
};
