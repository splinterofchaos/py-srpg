#pragma once

#include <functional>
#include <memory>
#include <vector>

#include "components.h"
#include "game.h"
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

  bool stop_short_ = false;

protected:
  // see run() documentation
  virtual void impl(Game& game, std::chrono::milliseconds,
                    std::vector<std::function<void()>>&) = 0;

public:
  Action(StopWatch stop_watch) : stop_watch_(stop_watch) {
    stop_watch_.start();
    stop_short_ = false;
  }

  Action() { }

  float ratio_finished() const { return stop_watch_.ratio_consumed(); }

  void stop_short() { stop_short_ = true; }
  bool finished() const { return stop_short_ || stop_watch_.finished(); }

  // Executes the action. This is intended to be called while iterating through
  // the ECS, therefore mutating operations, like creating a new entity or
  // destroying one, will be "deferred" in order to be run after execution.
  void run(Game& game, std::chrono::milliseconds dt,
           std::vector<std::function<void()>>& deferred_events) {
    stop_watch_.consume(dt);
    impl(game, dt, deferred_events);
  }
};

template<typename F>
class GenericAction : public Action {
  F f_;

  static_assert(std::is_same_v<
      std::result_of_t<F(Game&, float, std::vector<std::function<void()>>&)>,
      void>);

public:
  template<typename Rep, typename Period>
  GenericAction(std::chrono::duration<Rep, Period> duration, F f)
      : Action(StopWatch(duration)), f_(std::move(f)) { }

  void impl(Game& game, std::chrono::milliseconds dt,
            std::vector<std::function<void()>>& deferred_events) {
    f_(game, ratio_finished(), deferred_events);
  }
};

template<typename Rep, typename Period, typename F>
auto generic_action(std::chrono::duration<Rep, Period> duration, F f) {
  return std::make_unique<GenericAction<F>>(duration, std::move(f));
}

template<typename F>
auto generic_action(F f) {
  return generic_action(std::chrono::milliseconds(0), std::move(f));
}

// If change_final_position is true, sets the actor's grid position to
// path.back() at the end. Otherwise, the effect is merely graphical.
std::unique_ptr<Action> move_action(EntityId actor, Path path,
                                    bool change_final_position = true);
std::unique_ptr<Action> mele_action(const Ecs& ecs, EntityId attacker,
                                    EntityId defender, Path path);
std::unique_ptr<Action> expire_action(EntityId id,
                                      std::chrono::milliseconds expiry);

std::unique_ptr<Action> sequance_action(
    std::vector<std::unique_ptr<Action>> s);

template<typename...Actions>
std::unique_ptr<Action> sequance_action(Actions&&...actions) {
  std::vector<std::unique_ptr<Action>> s;
  (s.push_back(std::forward<Actions>(actions)), ...);
  return sequance_action(std::move(s));
}

