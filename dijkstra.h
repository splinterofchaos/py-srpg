#pragma once

#include <limits>
#include <unordered_map>

#include <glm/vec2.hpp>

#include "game.h"

struct DijkstraNode {
  glm::ivec2 prev;
  unsigned int dist;
  EntityId entity;

  DijkstraNode(EntityId entity)
      : entity(entity) {
    prev = {0, 0};
    dist = std::numeric_limits<int>::max();
  }

  DijkstraNode() : DijkstraNode(EntityId()) { }
};

class DijkstraGrid {
  std::unordered_map<glm::ivec2, DijkstraNode> nodes_;
  glm::ivec2 source_;

public:
  void generate(const Game& game, glm::ivec2 source);

  const glm::ivec2& source() const { return source_; }

  DijkstraNode& at(glm::ivec2 pos) { return nodes_.at(pos); }
  const DijkstraNode& at(glm::ivec2 pos) const { return nodes_.at(pos); }

  bool contains(glm::ivec2 p) { return nodes_.contains(p); }

  auto begin() { return nodes_.begin(); }
  auto begin() const { return nodes_.begin(); }
  auto end() { return nodes_.end(); }
  auto end() const { return nodes_.end(); }
};

// Returns the path to a position. Use ipath_to() for the format most native to
// the dijkstra graph, integers, and path_to() for the type most native to
// rendering: floats.
std::vector<glm::ivec2> ipath_to(const DijkstraGrid& dijkstra, glm::ivec2 pos);
std::vector<glm::vec2> path_to(const DijkstraGrid& dijkstra, glm::ivec2 pos);

// Roll down the graph n times. If n is larger that the distance from pos to
// the source(), source() is returned.
glm::ivec2 rewind(const DijkstraGrid& dijkstra, glm::ivec2 pos,
                  unsigned int n);

// Roll down the dijkstra graph to get a point no further from source() than n.
glm::ivec2 rewind_until(const DijkstraGrid& dijkstra, glm::ivec2 pos,
                        unsigned int n);

// Returns the nearest attacking target for AI.
std::pair<glm::ivec2, EntityId> nearest_player(const DijkstraGrid& dijkstra);
