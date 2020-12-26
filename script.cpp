#include "script.h"

void Script::push(ScriptFn fn) { instructions_.push_back(std::move(fn)); }

void Script::push_label(std::string name) {
  labels_.emplace(std::move(name), size());
}

void Script::clear() {
  instructions_.clear();
  labels_.clear();
}

int Script::get_label(const std::string& label) {
  auto it = labels_.find(label);
  return it == std::end(labels_) ? -1 : it->second;
}

bool ScriptEngine::active() {
  return instruction_pointer_ < script_.size() &&
    (last_result_.code == ScriptResult::START ||
     last_result_.code == ScriptResult::WAIT_ADVANCE);
}

ScriptResult ScriptEngine::run_impl(Game& game, ActionManager& manager) {
  while (instruction_pointer_ < script_.size()) {
    ScriptResult r = script_.get(instruction_pointer_)(game, manager);
    if (r.goto_line != -1) instruction_pointer_ = r.goto_line;
    if (int line = script_.get_label(r.goto_label); line != -1) {
      instruction_pointer_ = line;
    }

    if (r.code == ScriptResult::CONTINUE ||
        r.code == ScriptResult::WAIT_ADVANCE) {
      instruction_pointer_++;
    }

    if (r.code == ScriptResult::WAIT_ADVANCE ||
        r.code == ScriptResult::WAIT) {
      return r;
    }

    if (r.code == ScriptResult::ERROR) {
      clear();
      return r;
    }

    if (instruction_pointer_ == script_.size()) {
      r.code = ScriptResult::EXIT;
    }
    if (r.code == ScriptResult::EXIT) break;

    // r == retry is a noop.
  }

  return ScriptResult::EXIT;
}
