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

void cpu_decision(Game& game, const DijkstraGrid& dijkstra, EntityId id) {
  Decision decision;

  const Agent& agent = game.ecs().read_or_panic<Agent>(id);
  const Actor& actor = game.ecs().read_or_panic<Actor>(id);

  if (!game.turn().did_action) {
    std::vector<EntityId> enemies =
      enemies_in_range(game.ecs(), agent.team,
                       game.ecs().read_or_panic<GridPos>(id).pos,
                       actor.stats.range);
    if (enemies.size()) {
      game.decision().type = Decision::ATTACK_ENTITY;
      game.decision().target = enemies.front();
    }
  }

  if (game.decision().type == Decision::DECIDING && !game.turn().did_action &&
      !game.turn().did_move) {
    auto [enemy_loc, enemy_pos] = nearest_enemy_location(game, dijkstra, id,
                                                         agent.team);
    if (enemy_loc) {
      game.decision().type = Decision::MOVE_TO;
      game.decision().move_to = rewind_until(
          dijkstra, enemy_pos,
          [&](glm::ivec2 pos, const DijkstraNode& node) {
          return pos != enemy_pos &&
          node.dist <= actor.stats.move;
          });
    }
  }

  if (game.decision().type == Decision::DECIDING) {
    game.decision().type = Decision::PASS;
  }
}

// Used by player_decision(); handles the creation of the menu where the player
// selects from a list of actions when they right click on a tile in the world.
static void spawn_selection_box(Game& game, glm::ivec2 pos, EntityId player_id) {
  auto [id, exists] = actor_at(game.ecs(), pos);
  if (!exists) return;

  game.popup_box().reset(new SelectionBox(game));

  auto look_at = [&game, id=id] {
    game.decision().type = Decision::LOOK_AT;
    game.decision().target = id;
  };
  game.popup_box()->add_text_with_onclick("look", look_at);

  glm::ivec2 player_pos = game.ecs().read_or_panic<GridPos>(player_id).pos;
  const unsigned int range =
    game.ecs().read_or_panic<Actor>(player_id).stats.range;
  if (can_attack(game, player_pos, range, id)) {
    auto attack = [&game, id=id] {
      game.decision().type = Decision::ATTACK_ENTITY;
      game.decision().target = id;
    };
    game.popup_box()->add_text_with_onclick("normal attack", attack);
  }

  if (can_talk(game, player_pos, id)) {
    auto attack = [&game, id=id] {
      game.decision().type = Decision::TALK;
      game.decision().target = id;
    };
    // TODO: Do we want a generic talk or should it always be recruit?
    game.popup_box()->add_text_with_onclick("recruit", attack);
  }

  game.popup_box()->build_text_box_next_to(player_pos);
}

void player_decision(Game& game, EntityId id, const UserInput& input) {
  if (input.right_click) {
    spawn_selection_box(game, input.mouse_pos, id);
    return;
  }

  if (!input.left_click) return;

  glm::ivec2 pos = game.ecs().read_or_panic<GridPos>(id).pos;
  auto [enemy, exists] = actor_at(game.ecs(), input.mouse_pos);

  const Actor& actor = game.ecs().read_or_panic<Actor>(id);
  if (pos == input.mouse_pos) {
    game.decision().type = Decision::PASS;
  } else if (exists && can_attack(game, pos, actor.stats.range, enemy)) {
    game.decision().type = Decision::ATTACK_ENTITY;
    game.decision().target = enemy;
  } else if (!exists && manh_dist(pos, input.mouse_pos) <= actor.stats.move) {
    game.decision().type = Decision::MOVE_TO;
    game.decision().move_to = input.mouse_pos;
  }
}

