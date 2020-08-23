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

  void impl(Ecs& ecs, std::chrono::milliseconds) override {
    GridPos* grid_pos = nullptr;
    Transform* transform = nullptr;
    if (EcsError e = ecs.read(actor_, &transform, &grid_pos);
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

class MeleAction : public Action {
  std::unique_ptr<Action> mover_;
  std::unique_ptr<Action> attack_;
  std::unique_ptr<Action> recoil_;

  // Landing position after mover_.
  glm::vec2 final_pos_;
  EntityId attacker_;

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

    glm::vec2 defender_pos = ecs.read_or_panic<GridPos>(defender).pos;
    // Where the attacker will thrust towards.
    glm::vec2 attack_pos = (defender_pos - final_pos_) * 0.3f + final_pos_;
    attack_ = move_action(attacker, {final_pos_, attack_pos},
                          false  /* change_final_position */);
    recoil_ = move_action(attacker, {attack_pos, final_pos_},
                          false  /* change_final_position */);
  }

  void impl(Ecs& ecs, std::chrono::milliseconds dt) override {
    if (state_ == MOVING_TO_ATTACK_POSITION) {
      mover_->run(ecs, dt);
      if (mover_->finished()) state_ = ATTACKING;
    } else if (state_ == ATTACKING) {
      attack_->run(ecs, dt);
      if (attack_->finished()) state_ = RETREATING;
    } else if (state_ == RETREATING) {
      recoil_->run(ecs, dt);
      if (recoil_->finished()) stop_short();
    }

    if (finished()) {
      GridPos* grid_pos = nullptr;
      Transform* transform = nullptr;
      if (ecs.read(attacker_, &grid_pos, &transform) != EcsError::OK) {
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
