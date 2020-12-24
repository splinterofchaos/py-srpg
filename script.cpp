#include "script.h"

bool ScriptEngine::active() {
  return instruction_pointer_ < instructions_.size() &&
    (last_result_.code == ScriptResult::START ||
     last_result_.code == ScriptResult::WAIT);
}


void ScriptEngine::add(ScriptFn f) {
  instructions_.push_back(std::move(f));
}

ScriptResult ScriptEngine::run_impl(Game& game, ActionManager& manager) {
  while (instruction_pointer_ < instructions_.size()) {
    ScriptResult r = instructions_[instruction_pointer_](game, manager);
    if (r.goto_line != -1) instruction_pointer_ = r.goto_line;

    if (r.code == ScriptResult::WAIT) return r;

    if (r.code == ScriptResult::ERROR) {
      clear();
      return r;
    }

    if (r.code == ScriptResult::CONTINUE) instruction_pointer_++;

    if (instruction_pointer_ == instructions_.size()) {
      r.code = ScriptResult::EXIT;
    }
    if (r.code == ScriptResult::EXIT) break;

    // r == retry is a noop.
  }

  return ScriptResult::EXIT;
}
