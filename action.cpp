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

  auto deal_damage = generic_action(
      [attacker, defender]
      (Game& game, float,
       std::vector<std::function<void()>>& deferred_events) {
        Actor* defender_actor;
        GridPos* defender_pos;
        if (game.ecs().read(defender, &defender_actor, &defender_pos) !=
            EcsError::OK) {
          std::cerr << "Could not read defender's stats or position!"
                    << std::endl;
          return;
        }

        Actor* attacker_actor;
        if (game.ecs().read(attacker, &attacker_actor) != EcsError::OK) {
          std::cerr << "Could not read defender's stats!" << std::endl;
          return;
        }

        defender_actor->status = attacker_actor->embue;

        int damage = std::min(attacker_actor->stats.strength -
                              defender_actor->stats.defense,
                              defender_actor->stats.hp);
        damage = std::max(damage, 1);

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

  SequnceAction s;
  if (move_to_position) s.push_back(std::move(move_to_position));
  s.push_back(std::move(thrust));
  s.push_back(std::move(deal_damage));
  s.push_back(std::move(recoil));
  return std::make_unique<SequnceAction>(std::move(s));
}

std::unique_ptr<Action> sequance_action(
    std::vector<std::unique_ptr<Action>> s) {
  return std::make_unique<SequnceAction>(std::move(s));
}
