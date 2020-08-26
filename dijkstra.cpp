#include "dijkstra.h"

#include <unordered_set>

static glm::ivec2 min_node(
    const std::unordered_map<glm::ivec2, DijkstraNode>& nodes,
    const std::unordered_set<glm::ivec2>& Q) {
  glm::ivec2 min_pos;
  unsigned int min_dist = std::numeric_limits<int>::max();
  for (const glm::ivec2& pos : Q) {
    if (nodes.at(pos).dist < min_dist) {
      min_pos = pos;
      min_dist = nodes.at(pos).dist;
    }
  }

  return min_pos;
}

void DijkstraGrid::generate(const Game& game, glm::ivec2 source) {
  nodes_.clear();

  source_ = source;

  std::unordered_set<glm::ivec2> Q;
  for (const auto& [pos, tile] : game.grid()) {
    if (!tile.walkable) continue;
    auto [entity, entity_exists] = actor_at(game.ecs(), pos);
    // We want paths to nodes with entities on them, but we do not want to
    // expand them or paths would go right through!
    if (!entity) Q.insert(pos);

    nodes_.emplace(pos, DijkstraNode(entity));
  }
  nodes_[source].dist = 0;
  Q.insert(source);

  while (!Q.empty()) {
    glm::ivec2 pos = min_node(nodes_, Q);
    Q.erase(pos);

    DijkstraNode& node = nodes_.at(pos);

    for (glm::ivec2 next_pos : adjacent_positions(pos)) {
      auto next_node = nodes_.find(next_pos);
      if (next_node == nodes_.end()) continue;

      unsigned int alt = node.dist + 1;
      if (alt < next_node->second.dist) {
        next_node->second.dist = alt;
        next_node->second.prev = pos;
      }
    }
  }
}

std::vector<glm::ivec2> ipath_to(const DijkstraGrid& dijkstra,
                                 glm::ivec2 pos) {
  const DijkstraNode* node = &dijkstra.at(pos);

  std::vector<glm::ivec2> path;
  path.resize(node->dist + 1);
  path[node->dist] = pos;
  while (node->dist) {
    pos = node->prev;
    node = &dijkstra.at(pos);
    path[node->dist] = pos;
  }

  return path;
}

std::vector<glm::vec2> path_to(const DijkstraGrid& dijkstra, glm::ivec2 pos) {
  auto ipath = ipath_to(dijkstra, pos);
  std::vector<glm::vec2> path(ipath.begin(), ipath.end());
  return path;
}

glm::ivec2 rewind(const DijkstraGrid& dijkstra, glm::ivec2 pos,
                  unsigned int n) {
  const DijkstraNode* node = &dijkstra.at(pos);
  while (n && node->dist > 0) {
    pos = node->prev;
    node = &dijkstra.at(pos);
  }
  return pos;
}

glm::ivec2 rewind_until(const DijkstraGrid& dijkstra, glm::ivec2 pos,
                        unsigned int n) {
  const DijkstraNode* node = &dijkstra.at(pos);
  while (node->dist > n) {
    pos = node->prev;
    node = &dijkstra.at(pos);
  }
  return pos;
}

std::pair<glm::ivec2, EntityId> nearest_player(const DijkstraGrid& dijkstra) {
  const DijkstraNode* min_node = nullptr;
  glm::ivec2 min_pos;
  for (const auto& [pos, node] : dijkstra) {
    if (node.entity && pos != dijkstra.source() &&
        (!min_node || node.dist < min_node->dist)) {
      min_node = &node;
      min_pos = pos;
    }
  }
  return {min_pos, min_node ? min_node->entity : EntityId()};
}

