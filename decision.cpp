#include "decision.h"

#include "dijkstra.h"
#include "user_input.h"

static std::vector<EntityId> enemies_in_range(
    const Ecs& ecs, Team team, glm::ivec2 pos, unsigned int range) {
  std::vector<EntityId> enemies;
  for (const auto& [id, grid_pos, agent] : ecs.read_all<GridPos, Agent>()) {
    if (agent.team != team && manh_dist(pos, grid_pos.pos) <= range)
      enemies.push_back(id);
  }
  return enemies;
}

bool can_attack(const Game& game, glm::ivec2 from_pos, unsigned int attack_range,
                EntityId target) {
  return !game.turn().did_action &&
         diamond_dist(game.ecs().read_or_panic<GridPos>(target).pos, from_pos)
         <= attack_range;
}

bool can_talk(const Game& game, glm::ivec2 from_pos, EntityId target) {
  return can_attack(game, from_pos, 3, target);
}

Decision cpu_decision(const Game& game, const DijkstraGrid& dijkstra,
                      EntityId id) {
  Decision decision;

  const Agent& agent = game.ecs().read_or_panic<Agent>(id);
  const Actor& actor = game.ecs().read_or_panic<Actor>(id);

  if (!game.turn().did_action) {
    std::vector<EntityId> enemies =
      enemies_in_range(game.ecs(), agent.team,
                       game.ecs().read_or_panic<GridPos>(id).pos,
                       actor.stats.range);
    if (enemies.size()) {
      decision.type = Decision::ATTACK_ENTITY;
      decision.target = enemies.front();
    }
  }

  if (decision.type == Decision::DECIDING && !game.turn().did_action &&
      !game.turn().did_move) {
    auto [enemy_loc, enemy_pos] = nearest_enemy_location(game, dijkstra, id,
                                                         agent.team);
    if (enemy_loc) {
      decision.type = Decision::MOVE_TO;
      decision.move_to = rewind_until(
          dijkstra, enemy_pos,
          [&](glm::ivec2 pos, const DijkstraNode& node) {
          return pos != enemy_pos &&
          node.dist <= actor.stats.move;
          });
    }
  }

  if (decision.type == Decision::DECIDING) decision.type = Decision::PASS;

  return decision;
}

Decision player_decision(const Game& game, EntityId id, const UserInput& input) {
  Decision decision;
  if (!input.left_click) return decision;

  glm::ivec2 pos = game.ecs().read_or_panic<GridPos>(id).pos;
  auto [enemy, exists] = actor_at(game.ecs(), input.mouse_pos);

  const Actor& actor = game.ecs().read_or_panic<Actor>(id);
  if (pos == input.mouse_pos) {
    decision.type = Decision::PASS;
  } else if (exists && can_attack(game, pos, actor.stats.range, enemy)) {
    decision.type = Decision::ATTACK_ENTITY;
    decision.target = enemy;
  } else if (!exists && manh_dist(pos, input.mouse_pos) <= actor.stats.move) {
    decision.type = Decision::MOVE_TO;
    decision.move_to = input.mouse_pos;
  }

  return decision;
}

