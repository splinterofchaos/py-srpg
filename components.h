#pragma once

#include <memory>

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
  unsigned int hp = 0, max_hp = 0;
  unsigned int strength = 0;
  unsigned int speed = 0;
};

// Actions depend on the ECS so forward declare it so it can be a component.
class Action;
using ActionPtr = std::unique_ptr<Action>;

enum ActorState {
  SETUP, DECIDING, TAKING_TURN, WAITING,
  N_ACTOR_STATES
};

// Identifies that an entity is an actor.
struct Actor {
  std::string name;
  Stats stats;
};

enum class Team { PLAYER, CPU };

// The agent controls when an actor gets to take its turn and how.
struct Agent {
  int energy = 0;

  Team team;
};

using Ecs = EntityComponentSystem<
  GridPos,
  Transform,
  GlyphRenderConfig,
  Actor,
  ActionPtr,
  ActorState,
  Agent>;

