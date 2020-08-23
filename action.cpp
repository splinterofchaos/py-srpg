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
  template<typename...Actions>
  SequnceAction(Actions&&...actions) {
    (sequence_.push_back(std::forward<Actions>(actions)), ...);
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

template<typename...Actions>
std::unique_ptr<Action> sequance_action(Actions&&...actions) {
  return std::make_unique<SequnceAction>(std::forward<Actions>(actions)...);
}

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

// TODO: This can just be a sequence.
class MeleAction : public Action {
  std::unique_ptr<Action> mover_;
  std::unique_ptr<Action> attack_;
  std::unique_ptr<Action> recoil_;

  // Landing position after mover_.
  glm::vec2 final_pos_;
  EntityId attacker_;
  EntityId defender_;

  enum State { MOVING_TO_ATTACK_POSITION,
               ATTACKING,
               RETREATING } state_;

public:
  MeleAction(const Ecs& ecs, EntityId attacker, EntityId defender, Path path) {
    // Move to target first. Otherwise, initiate the attack.
    if (!path.empty()) {
      final_pos_ = path.back();
      mover_ = move_action(attacker, std::move(path));
      state_ = MOVING_TO_ATTACK_POSITION;
    } else {
      state_ = ATTACKING;
      final_pos_ = ecs.read_or_panic<GridPos>(attacker).pos;
    }

    attacker_ = attacker;
    defender_ = defender;

    glm::vec2 defender_pos = ecs.read_or_panic<GridPos>(defender).pos;
    // Where the attacker will thrust towards.
    glm::vec2 attack_pos = (defender_pos - final_pos_) * 0.3f + final_pos_;
    attack_ = move_action(attacker, {final_pos_, attack_pos},
                          false  /* change_final_position */);
    recoil_ = move_action(attacker, {attack_pos, final_pos_},
                          false  /* change_final_position */);
  }

  void impl(Game& game, std::chrono::milliseconds dt,
            std::vector<std::function<void()>>& deferred_events) override {
    if (state_ == MOVING_TO_ATTACK_POSITION) {
      mover_->run(game, dt, deferred_events);
      if (mover_->finished()) state_ = ATTACKING;
    } else if (state_ == ATTACKING) {
      attack_->run(game, dt, deferred_events);
      if (attack_->finished()) {
        state_ = RETREATING;

        Actor* defender_actor;
        GridPos* defender_pos;
        if (game.ecs().read(defender_, &defender_actor, &defender_pos) !=
            EcsError::OK) {
          std::cerr << "Could not read defender's stats or position!"
                    << std::endl;
          return;
        }

        Actor* attacker_actor;
        if (game.ecs().read(attacker_, &attacker_actor) != EcsError::OK) {
          std::cerr << "Could not read defender's stats!" << std::endl;
          return;
        }

        int damage =
          defender_actor->stats.hp < attacker_actor->stats.strength ?
          defender_actor->stats.hp : attacker_actor->stats.strength;

        // We could kill the entity off here as a deferred event, but we may
        // have to check that entities don't die of non-combat related causes
        // anyway so let's not bother.
        defender_actor->stats.hp -= damage;

        // Spawn a "-X" to appear over the defender.
        deferred_events.push_back([damage, defender_pos, &game] {
            GridPos x_pos{defender_pos->pos + glm::ivec2(0, 1)};
            Transform x_transform{glm::vec2(x_pos.pos), 1};
            // TODO: Obviously, we want to support multi-digit damage.
            GlyphRenderConfig rc(game.font_map().get('0' + damage),
                                 glm::vec4(8.f, 0.f, 0.f, 1.f));
            rc.center();
            EntityId damage_text =
                game.ecs().write_new_entity(x_pos, x_transform, rc);
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
    } else if (state_ == RETREATING) {
      recoil_->run(game, dt, deferred_events);
      if (recoil_->finished()) stop_short();
    }

    if (finished()) {
      GridPos* grid_pos = nullptr;
      Transform* transform = nullptr;
      if (game.ecs().read(attacker_, &grid_pos, &transform) != EcsError::OK) {
        std::cerr << "Could not read attacker's grid pos and transform!"
                  << std::endl;
        return;
      }
      grid_pos->pos = glm::round(final_pos_);
      transform->pos = final_pos_;
    }
  }
};

std::unique_ptr<Action> mele_action(const Ecs& ecs, EntityId attacker,
                                    EntityId defender, Path path) {
  return std::make_unique<MeleAction>(ecs, attacker, defender,
                                      std::move(path));
}
