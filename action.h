#pragma once

#include <functional>
#include <list>
#include <map>
#include <memory>
#include <vector>

#include "components.h"
#include "game.h"
#include "timer.h"

using Path = std::vector<glm::vec2>;

class ActionManager;

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
  virtual void impl(Game& game, std::chrono::milliseconds, ActionManager&) = 0;

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
  void run(Game& game, std::chrono::milliseconds dt, ActionManager& manager) {
    stop_watch_.consume(dt);
    impl(game, dt, manager);
  }
};

template<typename F>
class GenericAction : public Action {
  F f_;

  static_assert(std::is_same_v<
      std::result_of_t<F(Game&, float, ActionManager&)>,
      void>);

public:
  template<typename Rep, typename Period>
  GenericAction(std::chrono::duration<Rep, Period> duration, F f)
      : Action(StopWatch(duration)), f_(std::move(f)) { }

  void impl(Game& game, std::chrono::milliseconds dt, ActionManager& manager) {
    f_(game, ratio_finished(), manager);
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

// The Action Manager stores independent and ordered actions to organize events
// that happen. Independent actions are for largely graphical or aesthetic
// events like text spawning and being destroyed, while ordered actions modify
// the game's state in some way and therefor can only be done one after another
// to avoid race conditions.
//
// Ordered actions are held in a stack and new actions can be added at any
// time, even while processing the previous action.
class ActionManager {
  // Each independent action has an ID so that it can be referred to by other
  // events. For example, consider an event that waits until another has
  // finished.
  unsigned int previous_action_id_ = 0;

  // An ordered map is used so we can efficiently assign the next action ID in
  // the case of integer overflow.
  std::map<unsigned int, std::unique_ptr<Action>> independent_actions_;
  // In case an action needs to modify the independent actions list, while
  // we're processing it, this needs to be done later. The deferred list allows
  // functions to schedule new actions to be added.
  std::vector<std::function<void(ActionManager&)>> deferred_events_;

  // Ordered events are stored in reverse order they are executed and only one
  // may be executed at a time.
  std::list<std::unique_ptr<Action>> ordered_actions_;

 public:
  // Adds an action to the independent set. Note that this may be unsafe to do
  // while executing actions so use add_deferred_action() instead.
  unsigned int add_independent_action(std::unique_ptr<Action> action);
  void add_deferred_action(std::function<void(ActionManager&)> f);
  void add_ordered_action(std::unique_ptr<Action> action);

  template<typename...ActionPtr>
  void add_ordered_sequence(std::unique_ptr<Action> a, ActionPtr...actions) {
    add_ordered_sequence(std::move(actions)...);
    add_ordered_action(std::move(a));
  }

  void add_ordered_sequence() { }

  void move_ordered_sequence(std::vector<std::unique_ptr<Action>> v) {
    for (auto& a : v) add_ordered_sequence(std::move(a));
  }

  void process_independent_actions(Game& game, std::chrono::milliseconds dt);
  unsigned int process_ordered_actions(Game& game,
                                       std::chrono::milliseconds dt);

  bool have_ordered_actions() { return ordered_actions_.size(); }
};
