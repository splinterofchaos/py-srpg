import pygame
import typing
import random
import copy

import actor
import graphics
from item_compendium import item_compendium


class TileType:
  def __init__(self, walkable, surface_handle, desc):
    self.walkable_ = walkable
    self.surface_handle_ = surface_handle
    self.desc_ = desc

  def walkable(self): return self.walkable_

  def surface_handle(self): return self.surface_handle_

  def desc(self): return self.desc_

# Represents the map players walk on. Holds a dict of `tile_types_` mapping a
# type handle to its `TileType`, and the map itself.
class TileGrid:
  def __init__(self):
    self.tile_types_ = dict()
    self.grid_ = dict()
    self.min_x_ = None
    self.max_x_ = None
    self.min_y_ = None
    self.max_y_ = None

  def Get(self, xy, y=None):
    return self.grid_[xy if y is None else (xy, y)]

  def Iterate(self):
    for (x, y), type in self.grid_.items():
      yield (x, y), type

  def Set(self, x, y, tile_type):
    self.min_x_ = x if self.min_x_ is None else min(self.min_x_, x)
    self.min_y_ = y if self.min_y_ is None else min(self.min_y_, y)
    self.max_x_ = x if self.max_x_ is None else max(self.max_x_, x)
    self.max_y_ = y if self.max_y_ is None else max(self.max_y_, y)
    self.grid_[(x, y)] = tile_type

  def RegisterTileType(self, handle, type):
    # TODO: make sure an entry doesn't already exist
    self.tile_types_[handle] = type

  def HasTile(self, xy, y=None):
    return (xy if y is None else (x,y)) in self.grid_

SILLY_STARTING_MAP_FOR_TESTING = """
  ##############
  #............#
  #............#
  #............#
  #............#
  #............#
  #............######
  ######............#
       #............#
       #............#
       #............#
       #............#
       #............#
       #............#
       ##############
  """

def TileGridFromString(tile_types, string_map):
  grid = TileGrid()
  for row, line in enumerate(string_map.split('\n')):
    for col, c in enumerate(line):
      if c == ' ': continue
      grid.Set(col, row, tile_types[c])
  return grid

def ValidTiles(tile_grid, entities):
  taken_positions = {e[actor.Properties.POS] for e in entities}
  res = []
  for pos, type in tile_grid.Iterate():
    if type.walkable() and pos not in taken_positions: res.append(pos)

  return res

# TODO: The image should be assigned by the compendium, but we need to move
# graphics handling to an importable library, first.
def SpawnItem(entity_template, id, position):
  # TODO: Not all things are safe to copy like this. Consider making a Copy()
  # function within Modifier.
  e = copy.deepcopy(entity_template)
  e.id = id
  e[actor.Properties.POS] = position
  return e

# Some standard components
POS = "pos"
IMAGE = "image"
DESC = "description"

WINDOW_SIZE = (1000, 1000)
LOOK_DESCRIPTION_START = (600, 100)

# Represents all state held by the game like the list of entities, the map,
# etc.. Useful for passing around to functions that may want to modify the
# state of the world.
class Game:
  def __init__(self):
    self.tile_grid = None
    # TODO: Might use a dict in the future.
    self.entities = []

    # State such as whose turn it currently is will eventually go here, too.

def main():
  random.seed()
  pygame.init()
  pygame.display.set_caption("hello world")
  pygame.font.init()

  surface_reg = graphics.SurfaceRegistry()

  game = Game()

  tile_types = {
      '#': TileType(False, '#', 'A hard stone wall.'),
      '.': TileType(True, '.', 'A simple stone floor.'),
  }
  game.tile_grid = TileGridFromString(
      tile_types, SILLY_STARTING_MAP_FOR_TESTING)

  PLAYER = 0
  player = actor.Actor(PLAYER, 'player')
  player.UpdatePairs((
    (actor.Properties.POS, (5,5)),
    (actor.Properties.IMAGE, '@'),
    (actor.Properties.DESC, "A very simple dood.")),
  )
  game.entities.append(player)

  # Spawn a random item to play with.
  valid_tiles = ValidTiles(game.tile_grid, game.entities)
  valid_pos = random.choice(valid_tiles)
  random_item = random.choice(item_compendium)
  game.entities.append(SpawnItem(random_item, 1, valid_pos))

  screen = pygame.display.set_mode((graphics.TILE_SIZE*52, graphics.TILE_SIZE*45))

  map_surface = screen.subsurface(
    (0, 0, graphics.TILE_SIZE*40, graphics.TILE_SIZE*40))
  info_surface = screen.subsurface(
    (graphics.TILE_SIZE*41, graphics.TILE_SIZE*2,
     graphics.TILE_SIZE*10, graphics.TILE_SIZE*10))

  running = True

  camera_offset = (0, 0)

  selector_surface = pygame.Surface((graphics.TILE_SIZE, graphics.TILE_SIZE))
  selector_surface.fill([0,100,100,10])
  selector = surface_reg.RegisterSurface(selector_surface)

  previous_mouse_grid_pos = None
  desc = None
  while running:
    for event in pygame.event.get():
      if event.type == pygame.QUIT:
        running = False

    for (x,y), type in game.tile_grid.Iterate():
      graphics.GridBlit(surface_reg[game.tile_grid.Get(x, y).surface_handle()],
                        map_surface, camera_offset, (x, y))

    for e in game.entities:
      image = e.Get(actor.Properties.IMAGE)
      pos = e.Get(actor.Properties.POS)
      if not (image and pos): continue
      graphics.GridBlit(surface_reg[image], map_surface, camera_offset, pos)

    mouse_pos = graphics.GridPos(camera_offset, pygame.mouse.get_pos())
    graphics.GridBlit(surface_reg[selector], map_surface, camera_offset,
                      mouse_pos, special_flags=pygame.BLEND_RGBA_ADD)

    if (previous_mouse_grid_pos is not None and
        previous_mouse_grid_pos != mouse_pos):
      if not game.tile_grid.HasTile(mouse_pos):
        desc = None
      else:
        desc = f'{mouse_pos}:\n{game.tile_grid.Get(mouse_pos).desc()}'
        for e in game.entities:
          if e.Get(actor.Properties.POS) == mouse_pos:
            desc = f'{mouse_pos}:\n{str(e)}'
            break

    previous_mouse_grid_pos = mouse_pos

    if desc:
      graphics.BlitText(surface_reg, info_surface, desc)

    pygame.display.flip()
    screen.fill((0,0,0))

if __name__ == "__main__":
  main()
