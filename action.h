#pragma once

#include <memory>

#include "components.h"
#include "timer.h"

using Path = std::vector<glm::vec2>;

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
  virtual void impl(Ecs& ecs, std::chrono::milliseconds) = 0;

public:
  Action(StopWatch stop_watch) : stop_watch_(stop_watch) {
    stop_watch_.start();
  }

  Action() { }

  float ratio_finished() const { return stop_watch_.ratio_consumed(); }

  virtual bool finished() const { return stop_watch_.finished(); }

  void run(Ecs& ecs, std::chrono::milliseconds dt) {
    stop_watch_.consume(dt);
    impl(ecs, dt);
  }
};

// If change_final_position is true, sets the actor's grid position to
// path.back() at the end. Otherwise, the effect is merely graphical.
std::unique_ptr<Action> move_action(EntityId actor, Path path,
                                    bool change_final_position = true);
std::unique_ptr<Action> mele_action(const Ecs& ecs, EntityId attacker,
                                    EntityId defender, Path path);
