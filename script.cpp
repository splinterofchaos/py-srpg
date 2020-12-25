#include "script.h"

void Script::push(ScriptFn fn) { instructions_.push_back(std::move(fn)); }

void Script::push_label(std::string name) {
  labels_.emplace(std::move(name), size());
}

void Script::clear() {
  instructions_.clear();
  labels_.clear();
}

bool ScriptEngine::active() {
  return instruction_pointer_ < script_.size() &&
    (last_result_.code == ScriptResult::START ||
     last_result_.code == ScriptResult::WAIT);
}

ScriptResult ScriptEngine::run_impl(Game& game, ActionManager& manager) {
  while (instruction_pointer_ < script_.size()) {
    ScriptResult r = script_.get(instruction_pointer_)(game, manager);
    if (r.goto_line != -1) instruction_pointer_ = r.goto_line;

    if (r.code == ScriptResult::WAIT) return r;

    if (r.code == ScriptResult::ERROR) {
      clear();
      return r;
    }

    if (r.code == ScriptResult::CONTINUE) instruction_pointer_++;

    if (instruction_pointer_ == script_.size()) {
      r.code = ScriptResult::EXIT;
    }
    if (r.code == ScriptResult::EXIT) break;

    // r == retry is a noop.
  }

  return ScriptResult::EXIT;
}
