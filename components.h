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

  // The z-axis is stored as an int, but must be converted to a value between
  // zero and -1 objects at lower (closer to zero) layers are drawn first.
  enum ZLayer {
    BACKGROUND,
    GRID,
    ACTORS,
    OVERLAY,
    POPUP_TEXT,
    WINDOW_BACKGROUND,
    WINDOW_TEXT,
    N_Z_LAYERS
  };

  static constexpr float OFFSET_PER_LAYER = -1.9f / N_Z_LAYERS;

  ZLayer z;
};

struct Stats {
  unsigned int max_hp = 10;
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
  Stats base_stats;
  unsigned int hp;

  std::vector<StatusEffect> statuses;
  StatusEffect embue;
  bool lifesteal;

  void recalculate_stats() {
    stats.max_hp = base_stats.max_hp;
    stats.move = base_stats.move;
    stats.range = base_stats.range;
    stats.defense = base_stats.defense;
    stats.strength = base_stats.strength;
    stats.speed = base_stats.speed;

    for (const StatusEffect& eff : statuses) {
      if (eff.slowed) stats.speed /= 2;
    }

    hp = std::min(hp, stats.max_hp);
  }

  void add_status(const StatusEffect& effect) {
    statuses.push_back(effect);
    recalculate_stats();
  }

  void expire_statuses() {
    for (StatusEffect& eff : statuses) --eff.ticks_left;
    std::size_t n_erased = std::erase_if(
        statuses,
        [](const StatusEffect& eff) { return eff.ticks_left <= 0; });
    if (n_erased) recalculate_stats();
  }

  Actor(std::string name, Stats stats)
      : name(std::move(name)), base_stats(stats), lifesteal(false) {
    recalculate_stats();
  }
};

struct Marker {
  glm::vec4 color;
  glm::vec2 stretch = glm::vec2(1.0f, 1.0f);

  Marker(glm::vec4 color_) { color = color_; }
  Marker(glm::vec4 color, glm::vec2 stretch)
    : color(color), stretch(stretch) { }
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
  Marker,
  Actor,
  ActionPtr,
  ActorState,
  Agent>;

