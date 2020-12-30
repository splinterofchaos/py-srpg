#include "script.h"

#include "game.h"

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

void push_jump(Script& script, std::string label) {
  script.push([label=std::move(label)]
              (Game& game, ActionManager& action_manager) {
      return ScriptResult(ScriptResult::RETRY, label);
  });
}

void push_jump_ptr(Script& script, std::string* label) {
  script.push([label=label](Game& game, ActionManager& action_manager) {
      auto r = ScriptResult(ScriptResult::RETRY, *label);
      label->clear();
      return r;
  });
}

void push_dialogue_block(
    Script& script,
    std::string* jump_label,
    std::string_view label,
    std::string_view text,
    std::vector<std::pair<std::string, std::string>>
        response_labels) {
  // Later, we're going to push a command to jump to the next part of the
  // conversation based on the dialogue choice, but we're moving the
  // response_labels into the first command so we have to check this now:
  bool jump_on_response = !response_labels.empty();

  script.push_label(std::string(label));
  script.push(
      [jump_label, text=std::string(text),
      response_labels=std::move(response_labels)]
      (Game& game, ActionManager& action_manager) {
        game.popup_box().reset(new DialogueBox(game, DIALOGUE_WIDTH));
        game.popup_box()->add_text(std::move(text));
        for (auto& [response, label] : response_labels) {
          game.popup_box()->add_text_with_onclick(
              std::move(response),
              [jump_label, label] { *jump_label = label; });
        }

        game.popup_box()->build_text_box_at(
            game.camera_offset() / TILE_SIZE +
            glm::vec2(-DIALOGUE_WIDTH/2, -2));

        return ScriptResult::WAIT_ADVANCE;
      }
  );

  if (jump_on_response) push_jump_ptr(script, jump_label);
}
