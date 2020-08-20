#pragma once

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

// Identifies that an entity is an actor.
struct Actor { };

using Ecs = EntityComponentSystem<
  GridPos,
  Transform,
  GlyphRenderConfig,
  Actor>;

