#pragma once

#include <functional>
#include <unordered_map>
#include <vector>

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
  std::string goto_label;

  ScriptResult(Code code) : code(code) { }
  ScriptResult(Code code, int goto_line) : code(code), goto_line(goto_line) { }
};

using ScriptFn = std::function<ScriptResult(Game&, ActionManager&)>;

class Script {
  std::vector<ScriptFn> instructions_;
  std::unordered_map<std::string, unsigned int> labels_;

 public:
  // Adds one instruction to the instruction list.
  void push(ScriptFn fn);

  // Put a label at the end of the current instruction set.
  void push_label(std::string name);

  unsigned int size() const { return instructions_.size(); }
  bool empty() const { return size() == 0; }
  ScriptFn& get(unsigned int i) { return instructions_[i]; }
  const ScriptFn& get(unsigned int i) const { return instructions_[i]; }
  int get_label(const std::string& label);

  void clear();
};

class ScriptEngine {
 private:
  Script script_;
  unsigned int instruction_pointer_ = 0;
  ScriptResult last_result_ = ScriptResult::EXIT;

  ScriptResult run_impl(Game& game, ActionManager& manager);

 public:
  ScriptEngine() { }

  // Checks if we are currently executing a script.
  bool active();

  void clear() { instruction_pointer_ = 0; script_.clear(); }

  void reset(Script script) {
    script_ = std::move(script);
    instruction_pointer_ = 0;
    last_result_ = ScriptResult::START;
  }

  ScriptResult run(Game& game, ActionManager& manager) {
    last_result_ = run_impl(game, manager);
    return last_result_;
  }
};
