#include "action.h"

#include "util.h"

static float path_distance(const Path& path) {
  if (path.empty()) return 0.f;

  float dist = 0;
  glm::vec pos = path.front();
  for (std::size_t i = 1; i < path.size(); ++i) {
    dist += glm::length(path[i] - pos);
    pos = path[i];
  }

  return dist;
}

class SequnceAction : public Action {
  std::vector<std::unique_ptr<Action>> sequence_;
  unsigned int current_ = 0;
  
public:
  SequnceAction() { }

  SequnceAction(std::vector<std::unique_ptr<Action>> s)
      : sequence_(std::move(s)) { }

  void push_back(std::unique_ptr<Action> a) {
    sequence_.push_back(std::move(a));
  }

  void impl(Game& game, std::chrono::milliseconds dt,
            std::vector<std::function<void()>>& deferred_events) {
    if (current_ >= sequence_.size()) {
      stop_short();
      return;
    }

    sequence_[current_]->run(game, dt, deferred_events);
    if (sequence_[current_]->finished()) ++current_;
    if (current_ >= sequence_.size()) stop_short();
  }
};

class MoveAction : public Action {
  EntityId actor_;
  Path path_;
  bool change_final_position_;

  static constexpr std::chrono::milliseconds MOVE_TILE_TIME =
    std::chrono::milliseconds(200);

public:
  MoveAction(EntityId actor, Path path, bool change_final_position)
      : Action(StopWatch(MOVE_TILE_TIME * path_distance(path))), actor_(actor),
        path_(std::move(path)),
        change_final_position_(change_final_position) { }

  void impl(Game& game, std::chrono::milliseconds,
            std::vector<std::function<void()>>&) override {
    GridPos* grid_pos = nullptr;
    Transform* transform = nullptr;
    if (EcsError e = game.ecs().read(actor_, &transform, &grid_pos);
        e != EcsError::OK) {
      std::cerr << "MoveAction(): got error from ECS: " << int(e);
      return;
    }

    auto to_float = [](glm::vec2 v) -> glm::vec2 { return v; };
    transform->pos = mix_vector_by_ratio(path_, ratio_finished(), to_float);

    if (!path_.empty() && change_final_position_ && finished()) {
      grid_pos->pos = glm::round(path_.back());
    }
  }
};

std::unique_ptr<Action> move_action(EntityId actor, Path path,
                                    bool change_final_position) {
  return std::make_unique<MoveAction>(actor, path, change_final_position);
}

std::unique_ptr<Action> hp_change_action(
    EntityId id, int change, StatusEffect effect = StatusEffect()) {
  return generic_action(
      [id, change, effect]
      (Game& game, float,
       std::vector<std::function<void()>>& deferred_events) {
        Actor* actor;
        GridPos* pos;
        if (game.ecs().read(id, &actor, &pos) !=
            EcsError::OK) {
          std::cerr << "Could not read entity's stats or position!"
                    << std::endl;
          return;
        }

        actor->add_status(effect);

        int change_ = std::min(change, int(actor->hp));
        actor->hp -= change_;
        actor->hp = std::min(actor->hp, actor->stats.max_hp);

        // Spawn a "-X" to appear over the entity
        deferred_events.push_back([change, pos, &game] {
            GridPos x_pos{pos->pos + glm::ivec2(0, 1)};
            Transform x_transform{glm::vec2(x_pos.pos), Transform::POPUP_TEXT};
            glm::vec4 color = change > 0 ? glm::vec4(0.8f, 0.0f, 0.0f, 1.f)
                                         : glm::vec4(0.0f, 0.8f, 0.1f, 1.f);
            // TODO: Obviously, we want to support multi-digit change.
            GlyphRenderConfig rc(game.font_map().get('0' + change), color);
            rc.center();
            EntityId damage_text =
                game.ecs().write_new_entity(x_pos, x_transform,
                                            std::vector{rc});
            auto float_up = move_action(
                damage_text, {x_pos.pos, x_pos.pos + glm::ivec2(0, 1)});
            auto expire = generic_action(
                [damage_text]
                (Game& game, float, std::vector<std::function<void()>>&) {
                  game.ecs().mark_to_delete(damage_text);
                });
            auto seq = sequance_action(std::move(float_up), std::move(expire));
            game.ecs().write(damage_text, std::move(seq), Ecs::CREATE_ENTRY);
        });
      }
  );
}

std::unique_ptr<Action> mele_action(const Ecs& ecs, EntityId attacker,
                                    EntityId defender, Path path) {
  std::unique_ptr<Action> move_to_position;
  glm::vec2 final_pos;
  if (!path.empty()) {
    final_pos = path.back();
    move_to_position = move_action(attacker, std::move(path));
  } else {
    final_pos = ecs.read_or_panic<GridPos>(attacker).pos;
  }

  glm::vec2 defender_pos = ecs.read_or_panic<GridPos>(defender).pos;
  // Where the attacker will thrust towards.
  glm::vec2 thrust_pos = glm::normalize(defender_pos - final_pos) * 0.3f +
                         final_pos;

  auto thrust = move_action(attacker, {final_pos, thrust_pos});
  auto recoil = move_action(attacker, {thrust_pos, final_pos});

  const Actor& defender_actor = ecs.read_or_panic<Actor>(defender);
  const Actor& attacker_actor = ecs.read_or_panic<Actor>(attacker);

  int damage = std::min(attacker_actor.stats.strength -
                        defender_actor.stats.defense,
                        defender_actor.hp);
  damage = std::max(damage, 1);

  auto deal_damage = hp_change_action(defender, damage, attacker_actor.embue);

  std::unique_ptr<Action> heal;
  if (attacker_actor.lifesteal)
    heal = hp_change_action(attacker, -damage);

  SequnceAction s;
  if (move_to_position) s.push_back(std::move(move_to_position));
  s.push_back(std::move(thrust));
  s.push_back(std::move(deal_damage));
  if (heal) s.push_back(std::move(heal));
  s.push_back(std::move(recoil));
  return std::make_unique<SequnceAction>(std::move(s));
}

std::unique_ptr<Action> sequance_action(
    std::vector<std::unique_ptr<Action>> s) {
  return std::make_unique<SequnceAction>(std::move(s));
}
