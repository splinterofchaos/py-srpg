#pragma once

#include <memory>

#include "components.h"
#include "timer.h"

// An "action" encompasses a change of state of the game. It handled both the
// animations and state effects of that action.
//
// Historical note: Previously, I had attempted to have an animation
// abstraction that would run and then call back into the game state in order
// to reflect changes, but this induced massive amounts of complexity. Handling
// it all in one place is much simpler.
class Action {
  StopWatch stop_watch_;  // Records the amount of time for this animation.

protected:
  virtual void impl(Ecs& ecs) = 0;

public:
  Action(StopWatch stop_watch) : stop_watch_(stop_watch) {
    stop_watch_.start();
  }

  float ratio_finished() { return stop_watch_.ratio_consumed(); }

  bool finished() { return stop_watch_.finished(); }

  void run(Ecs& ecs, std::chrono::milliseconds dt) {
    stop_watch_.consume(dt);
    impl(ecs);
  }
};

std::unique_ptr<Action> move_action(EntityId actor, glm::ivec2 to_pos);
