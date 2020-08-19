#include "action.h"

class MoveAction : public Action {
  EntityId actor_;
  glm::ivec2 to_pos_;

  static constexpr std::chrono::milliseconds MOVE_TILE_TIME =
    std::chrono::milliseconds(200);

public:
  MoveAction(EntityId actor, glm::ivec2 to_pos)
      : Action(StopWatch(MOVE_TILE_TIME)), actor_(actor), to_pos_(to_pos) { }

  void impl(Ecs& ecs) override {
    GridPos* grid_pos = nullptr;
    Transform* transform = nullptr;
    if (EcsError e = ecs.read(actor_, &transform, &grid_pos);
        e != EcsError::OK) {
      std::cerr << "MoveAction(): got error from ECS: " << int(e);
      return;
    }

    transform->pos = glm::mix(glm::vec2(grid_pos->pos), glm::vec2(to_pos_),
                              ratio_finished());

    if (finished()) {
      grid_pos->pos = to_pos_;
      transform->pos = to_pos_;
    }
  }
};


std::unique_ptr<Action> move_action(EntityId actor, glm::ivec2 to_pos) {
  return std::make_unique<MoveAction>(actor, to_pos);
}

