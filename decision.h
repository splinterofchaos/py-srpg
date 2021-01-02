#pragma once

// The Decision stores as data what the entity whose turn it currently is wants
// to do. This can then be referenced by scripts and interfaces globally.

#include <glm/vec2.hpp>

#include "include/ecs.h"

// Forward decls due to mutual dependency.
class Game;
class DijkstraGrid;
class UserInput;

// Decides what actions to take for this turn.
struct Decision {
  enum Type {
    DECIDING,       // No decision made yet.
    PASS,           // End the turn early.
    MOVE_TO,        // Walk to `move_to` below.
    ATTACK_ENTITY,  // Attack `target` below.
    LOOK_AT,        // Look at `target` below.
    TALK,           // Talk to `target` below.
    N_TYPES
  } type = DECIDING;

  union {
    glm::ivec2 move_to;
    // Attacking, talking, various actions may target an entity.
    EntityId target;
  };

  Decision() : type(DECIDING) { }
};

bool can_attack(const Game& game, glm::ivec2 from_pos,
                unsigned int attack_range,
                EntityId target);

bool can_talk(const Game& game, glm::ivec2 from_pos, EntityId target);

void cpu_decision(Game& game, const DijkstraGrid& dijkstra, EntityId id);
void player_decision(Game& game, EntityId id, const UserInput& input);
