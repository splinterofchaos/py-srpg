#pragma once

#include <functional>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "ui.h"

// Forward decl because of mutual references between Script and Game.
class Game;

// For movement scripts from point A to B with any number of intermediate
// points.
using Path = std::vector<glm::vec2>;

struct ScriptResult {
  enum Code {
    START,     // The default value of a script that hasn't run yet.
    EXIT,      // Exit the script (no more instruction or early exit).
    WAIT,      // Return early, don't advance. (Used by some goto
               // instructions.)
    WAIT_ADVANCE,  // Return early, advance.
    RETRY,     // Run the same line again (Used by goto instructions).
    CONTINUE,  // Run the next line.
    ERROR,     // Stop due to errors.
    N_RESULTS
  } code;

  int goto_line = -1;
  std::string goto_label;

  ScriptResult(Code code) : code(code) { }
  ScriptResult(Code code, int goto_line) : code(code), goto_line(goto_line) { }
  ScriptResult(Code code, std::string label)
    : code(code), goto_label(std::move(label)) { }
};

using ScriptFn = std::function<ScriptResult(Game&)>;

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

struct Vars {
  std::unordered_map<std::string, int> int_vars;
  std::unordered_map<std::string, std::string> string_vars;
  std::unordered_map<std::string, EntityId> entity_id_vars;
};

class ScriptEngine {
 private:
  unsigned int script_id_ = 0;
  Script script_;
  unsigned int instruction_pointer_ = 0;
  ScriptResult last_result_ = ScriptResult::EXIT;

  ScriptResult run_impl(Game& game);

 public:
  ScriptEngine(unsigned int id) : script_id_(id) { }
  explicit ScriptEngine(unsigned int id, Script s);

  unsigned int id() const { return script_id_; }

  bool finished() const { return last_result_.code == ScriptResult::EXIT; }

  bool active(); // Checks if we are currently executing a script.
  void clear() { instruction_pointer_ = 0; script_.clear(); }
  void reset(Script script);

  ScriptResult run(Game& game) {
    last_result_ = run_impl(game);
    return last_result_;
  }
};

// Pushes an instruction to the script which jumps to a label.
void push_jump(Script& script, std::string label);

// Scripts may allocate a string on the heap to coordinate where to jump to
// next in the script between GUI dialogue boxes and the script itself. Adds
// an instruction that jumps to the value set in the label.
void push_jump_ptr(Script& script, std::string* label);

// If a script allocated storage, such as for a jump label, put this at the
// end of the script to clean up the memory.
template<typename X>
void push_delete(Script& script, X* x) {
  script.push([x](Game&) {
      delete x;
      return ScriptResult::CONTINUE;
  });
}

// Dialogue-based scripts follow a typical formula of call and response.
// This function pushes a very simple call-response command onto the script.
// Responses then jump to later parts of the script allowing for branching
// options.
void push_dialogue_block(
    Script& script,
    std::string* jump_label,
    std::string_view label,
    std::string_view text,
    std::vector<std::pair<std::string, std::string>>
        response_labels = {});

void push_end_dialogue(Script& script);

void push_set_camera_target(Script& script, glm::vec2 pos);

void push_move_along_path(Script& script, EntityId id, Path path,
                          float tiles_per_second = 5.0f);

// Changes an entities health, making a nice "-X" appear on the screen to
// inform the player.
struct StatusEffect;
void push_hp_change(Script& script, EntityId id, int change,
                    StatusEffect effect);

void push_attack(Script& script, const Game& game, EntityId attacker,
                 EntityId defender);

enum class Team;
void push_convert_to_team(Script& script, Team team);
