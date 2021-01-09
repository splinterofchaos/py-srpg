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

ScriptEngine::ScriptEngine(unsigned int id, Script s) {
  script_id_ = id;
  reset(std::move(s));
}

bool ScriptEngine::active() {
  return instruction_pointer_ < script_.size() &&
    (last_result_.code == ScriptResult::START ||
     last_result_.code == ScriptResult::WAIT_ADVANCE);
}

void ScriptEngine::reset(Script script) {
  script_ = std::move(script);
  instruction_pointer_ = 0;
  last_result_ = ScriptResult::START;
}

ScriptResult ScriptEngine::run_impl(Game& game) {
  while (instruction_pointer_ < script_.size()) {
    ScriptResult r = script_.get(instruction_pointer_)(game);
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
  script.push([label=std::move(label)](Game& game) {
      return ScriptResult(ScriptResult::RETRY, label);
  });
}

void push_jump_ptr(Script& script, std::string* label) {
  script.push([label=label](Game& game) {
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
      (Game& game) {
        game.popup_box().reset(new DialogueBox(game, DIALOGUE_WIDTH));
        game.popup_box()->add_text(std::move(text));
        for (auto& [response, label] : response_labels) {
          game.popup_box()->add_text_with_onclick(
              std::move(response),
              [jump_label, label] { *jump_label = label; });
        }

        // We might want a slightly more intelligent way of determining this...
        EntityId speaker = game.decision().target;
        const glm::vec2& speaker_pos =
            game.ecs().read_or_panic<Transform>(speaker).pos;

        game.popup_box()->build_text_box_at(speaker_pos +
                                            glm::vec2(2.f, 2.5f));
        
        // Set the camera such that the speaker and far edge of the dialogue
        // window should be on screen and the dialogue box is centered on the
        // speaker.
        game.set_camera_target(
            glm::vec2((speaker_pos.x + DIALOGUE_WIDTH) / 1.8f,
                      game.popup_box()->center().y));

        return ScriptResult::WAIT_ADVANCE;
      }
  );

  if (jump_on_response) push_jump_ptr(script, jump_label);
}

void push_end_dialogue(Script& script) {
  script.push([](Game& game) {
      game.set_camera_target(
          game.ecs().read_or_panic<Transform>(game.turn().actor).pos);
      return ScriptResult::CONTINUE;
  });
}

static float path_distance(const Path& path) {
  if (path.size() < 2) return 0.0f;

  float dist = 0.f;
  for (std::size_t i = 0; i + 1 < path.size(); ++i)
    dist += glm::length(path[i + 1] - path[i]);
  return dist;
}

void push_set_camera_target(Script& script, glm::vec2 pos) {
  script.push([pos](Game& game) {
      game.set_camera_target(pos);
      return ScriptResult::CONTINUE;
  });
}


void push_move_along_path(Script& script, EntityId id, Path _path,
                          float tiles_per_second) {
  using std::chrono::duration;
  using std::chrono::seconds;

  Path* path = new Path(std::move(_path));
  // TODO: This math is wrong. FIXME
  StopWatch* watch = new StopWatch(duration<float, seconds::period>(
          path_distance(*path) / tiles_per_second));
  watch->start();

  script.push([=](Game& game) {
      Transform* transform = nullptr;
      if (game.ecs().read(id, &transform) != EcsError::OK) {
        std::cerr << "Move script interrupted: entity stopped existing."
                  << std::endl;
        return ScriptResult::CONTINUE;
      }

      watch->consume(game.dt());

      auto to_float = [](glm::vec2 v) { return v; };
      transform->pos = mix_vector_by_ratio(*path, watch->ratio_consumed(),
                                           to_float);

      // Clean up if finished.
      if (!path->empty() && watch->finished()) {
        GridPos* gpos = nullptr;
        if (game.ecs().read(id, &gpos) == EcsError::OK) {
          gpos->pos = glm::round(path->back());
        }

        delete watch;
        delete path;
        return ScriptResult::CONTINUE;
      }

      return ScriptResult::WAIT;
  });
}

void push_hp_change(Script& script, EntityId id, int change,
                    StatusEffect effect) {
  script.push([=](Game& game) {
      Actor* actor;
      GridPos* pos;
      if (game.ecs().read(id, &actor, &pos) != EcsError::OK) {
        std::cerr << "Could not read entity's stats or position!" << std::endl;
        return ScriptResult::CONTINUE;
      }

      actor->add_status(effect);

      int change_ = std::min(change, int(actor->hp));
      actor->hp -= change_;
      actor->hp = std::min(actor->hp, actor->stats.max_hp);

      // Spawn a "-X" to appear over the entity.
      GridPos x_pos{pos->pos + glm::ivec2(0, 1)};
      Transform x_transform{x_pos.pos, Transform::POPUP_TEXT};
      glm::vec4 color = change > 0 ? glm::vec4(0.8f, 0.0f, 0.0f, 1.f)
                                   : glm::vec4(0.0f, 0.8f, 0.1f, 1.f);
      GlyphRenderConfig rc(game.font_map().get('0' + change), color);
      rc.center();
      EntityId damage_text = game.ecs().write_new_entity(x_pos, x_transform,
                                                         std::vector{rc});

      Script move_up_and_delete;
      push_move_along_path(move_up_and_delete, damage_text,
                           {x_pos.pos, x_pos.pos + glm::ivec2(0, 1)});
      move_up_and_delete.push([=](Game& game) {
          game.ecs().mark_to_delete(damage_text);
          return ScriptResult::CONTINUE;
      });
      game.add_independent_script(std::move(move_up_and_delete));

      return ScriptResult::CONTINUE;
  });
}

void push_attack(Script& script, const Game& game, EntityId attacker,
                 EntityId defender) {
  glm::vec2 attacker_pos = game.ecs().read_or_panic<GridPos>(attacker).pos;
  glm::vec2 defender_pos = game.ecs().read_or_panic<GridPos>(defender).pos;
  // Where the attacker will thrust towards.
  glm::vec2 thrust_pos = glm::normalize(defender_pos - attacker_pos) * 0.3f +
                         attacker_pos;
  // Where to center the camera
  glm::vec2 cam_focus = glm::mix(attacker_pos, defender_pos, 0.5f);

  // Calculate damage.
  const Actor& defender_actor = game.ecs().read_or_panic<Actor>(defender);
  const Actor& attacker_actor = game.ecs().read_or_panic<Actor>(attacker);
  int damage = std::min(attacker_actor.stats.strength -
                        defender_actor.stats.defense,
                        defender_actor.hp);
  damage = std::max(damage, 1);

  push_set_camera_target(script, cam_focus);
  push_move_along_path(script, attacker, {attacker_pos, thrust_pos}, 5.f);
  push_hp_change(script, defender, damage, attacker_actor.embue);

  if (const Script* s = attacker_actor.triggers.get_or_null("on_hit_enemy")) {
    script.push([s](Game& game) {
        game.add_ordered_script(*s);
        return ScriptResult::CONTINUE;
    });
  }

  if (attacker_actor.lifesteal) {
    push_hp_change(script, attacker, -damage, StatusEffect());
  }

  push_move_along_path(script, attacker, {thrust_pos, attacker_pos}, 5.f);
  push_set_camera_target(script, attacker_pos);
}

void push_convert_to_team(Script& script, Team team) {
  script.push([team](Game& game) {
      std::vector<GlyphRenderConfig>* rcs;
      Agent* agent;
      if (game.ecs().read(game.decision().target, &rcs, &agent) !=
          EcsError::OK) {
        std::cout << "Can't convert an entity that doesn't exist."
                  << std::endl;
        return ScriptResult::CONTINUE;
      }

    agent->team = team;
    for (GlyphRenderConfig& rc : *rcs) {
      rc.fg_color = team == Team::PLAYER ? PLAYER_COLOR : CPU_COLOR;
    }
    return ScriptResult::CONTINUE;
  });
}
