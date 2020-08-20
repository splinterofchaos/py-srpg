#include "action.h"

#include "util.h"

class MoveAction : public Action {
  EntityId actor_;
  Path path_;

  static constexpr std::chrono::milliseconds MOVE_TILE_TIME =
    std::chrono::milliseconds(200);

public:
  MoveAction(EntityId actor, Path path)
      : Action(StopWatch(MOVE_TILE_TIME * path.size())), actor_(actor),
        path_(std::move(path)) { }

  void impl(Ecs& ecs) override {
    GridPos* grid_pos = nullptr;
    Transform* transform = nullptr;
    if (EcsError e = ecs.read(actor_, &transform, &grid_pos);
        e != EcsError::OK) {
      std::cerr << "MoveAction(): got error from ECS: " << int(e);
      return;
    }

    auto to_float = [](glm::ivec2 v) -> glm::vec2 { return v; };
    transform->pos = mix_vector_by_ratio(path_, ratio_finished(), to_float);

    if (finished()) {
      grid_pos->pos = path_.back();
      transform->pos = path_.back();
    }
  }
};


std::unique_ptr<Action> move_action(EntityId actor, Path path) {
  return std::make_unique<MoveAction>(actor, path);
}

