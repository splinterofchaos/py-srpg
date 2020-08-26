#include "dijkstra.h"

#include <deque>

void DijkstraGrid::generate(const Game& game, glm::ivec2 source) {
  nodes_.clear();

  source_ = source;

  // Note that while we're creating a data structure that resembles the result
  // of Dijkstra's algorithm, we're actually going to use flood fill because
  // it's faster on simple 2D grids like this where all edge weights are the
  // same.
  struct QueNode { glm::ivec2 pos; glm::ivec2 prev; unsigned int dist; };
  std::deque<QueNode> Q;
  Q.push_back({source, {0, 0}, 0});

  while (!Q.empty()) {
    auto [pos, prev, dist] = Q.front();
    Q.pop_front();

    auto [tile, exists] = game.grid().get(pos);
    if (!exists || !tile.walkable || nodes_.contains(pos)) continue;

    nodes_[pos] = DijkstraNode{prev, dist, actor_at(game.ecs(), pos).first};

    for (glm::ivec2 next_pos : adjacent_positions(pos))
      Q.push_back({next_pos, pos, dist + 1});
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

std::pair<const DijkstraNode*, glm::ivec2>
nearest_enemy_location(const Game& game, const DijkstraGrid& dijkstra,
                       EntityId my_id, Team my_team) {
  const DijkstraNode* min_node = nullptr;
  glm::ivec2 min_pos;
  for (const auto& [id, gpos, agent] : game.ecs().read_all<GridPos, Agent>()) {
    const glm::ivec2& pos = gpos.pos;
    if (id != my_id && agent.team != my_team && pos != dijkstra.source()) {
      const DijkstraNode& node = dijkstra.at(pos);
      if (!min_node || node.dist < min_node->dist) {
        min_node = &node;
        min_pos = pos;
      }
    }
  }
  return {min_node, min_pos};
}

