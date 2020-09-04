#pragma once

#include <memory>
#include <vector>

#include "ecs.h"
#include "font.h"
#include "shaders.h"

#include <glm/vec2.hpp>

// The logical position of an entity on the grid.
struct GridPos {
  glm::ivec2 pos;
};

// The graphical position of an entity in 2D/3D space. NOT RELATIVE TO THE
// CAMERA POSITION. The integer value of pos' coordinates map to the same
// location for a GridPos.
struct Transform {
  glm::vec2 pos;
  int z;
};

struct Stats {
  unsigned int hp = 10, max_hp = 10;
  unsigned int move = 5;
  unsigned int range = 1;
  unsigned int defense = 3;
  unsigned int strength = 5;
  unsigned int speed = 5;
};

// Actions depend on the ECS so forward declare it so it can be a component.
class Action;
using ActionPtr = std::unique_ptr<Action>;

enum ActorState {
  SETUP, DECIDING, TAKING_TURN, WAITING,
  N_ACTOR_STATES
};

struct StatusEffect {
  // The number of game ticks for this effect to be around. If in Actor::embue,
  // this relates to how many ticks it will take to expire.
  int ticks_left = 0;
  bool slowed = false;
};

// Identifies that an entity is an actor.
struct Actor {
  std::string name;
  Stats stats;

  std::vector<StatusEffect> statuses;
  StatusEffect embue;
  bool lifesteal;

  Actor(std::string name, Stats stats)
      : name(std::move(name)), stats(stats), lifesteal(false) {
  }
};

enum class Team { PLAYER, CPU };

// The agent controls when an actor gets to take its turn and how.
struct Agent {
  // TODO: When an agent is attacked, it should lose energy, but it should also
  // build "breaking" which, when full, would allow that unit to act
  // immediately.
  int energy = 0;
  int breaking = 0;

  Team team;

  Agent(Team team) {
    this->team = team;
  }
};

using Ecs = EntityComponentSystem<
  GridPos,
  Transform,
  std::vector<GlyphRenderConfig>,
  Actor,
  ActionPtr,
  ActorState,
  Agent>;

