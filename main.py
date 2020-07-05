import pygame
import typing
import random
import copy
import collections

import actor
import graphics
from item_compendium import *
from vec import *
from priority_queue import PriorityQueue


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
  taken_positions = {e.pos for e in entities}
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
  e.pos = position
  return e

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
    self.keyboard = {}  # Maps key names to True if pressed else False.

    # TODO: What this should really do is track the position of when the left
    # mouse button was pressed so that actions are committed when it's unpressed
    # IFF its position hasn't moved much.
    self.left_mouse = False  # True when the left mouse button is down.

    self.mouse_pos = None  # The position of the mouse in pixels.

    # State such as whose turn it currently is will eventually go here, too.

  def Walkable(self, pos):
    return (self.tile_grid.HasTile(pos) and
            self.tile_grid.Get(pos).walkable() and
            not pos in {e.GetOr('pos', (-1,-1)) for e in self.entities})

  def EntityAt(self, pos):
    for e in self.entities:
      p = e.GetOr('pos')
      if p and p == pos: return e


# Represents the action of moving from one place to the other.
class MoveAction(actor.Action):
  MARKER_SURFACE = None
  def __init__(self, to_pos): self.to_pos = to_pos
  def Run(self, game, a): a.pos = self.to_pos
  def Pos(self): return self.to_pos

  def Markers(self):
    return [actor.Marker(self.to_pos, MoveAction.MARKER_SURFACE)]


class GetAction(actor.Action):
  MARKER_SURFACE = None

  def __init__(self, item, move_action=None):
    self.item = item
    self.move_action = move_action

  def Run(self, game, a):
    if self.move_action:
      self.move_action.Run(game, a)
    a.PickUp(self.item)
    del self.item.pos

  def Pos(self): return self.item.pos

  def Markers(self):
    markers = [actor.Marker(self.item.pos, GetAction.MARKER_SURFACE)]
    if self.move_action:
      markers.extend(self.move_action.Markers())
    return markers


def OrthogonalPositions(pos):
  for step_x, step_y in ((-1, 0), (1, 0), (0, -1), (0, 1)):
    yield pos[0] + step_x, pos[1] + step_y

def BreadthFirstExpand(game, start, max_steps):
  tiles = []
  edges = collections.deque()
  edges.append((start, max_steps))
  while edges:
    pos, steps_left = edges.popleft()
    if pos in tiles or (pos != start and not game.Walkable(pos)): continue
    tiles.append(pos)

    if steps_left > 0:
      for new_pos in OrthogonalPositions(pos):
        edges.append((new_pos, steps_left - 1))
  return tiles

def GenerateActions(game, a):
  start = a.pos
  sheet = a.Stats()
  max_steps = sheet.stats['MOV'].value
  visited = {a.pos}
  actions = []
  for pos in BreadthFirstExpand(game, start, max_steps):
    move_action = None
    if pos != start:
      visited.add(pos)
      move_action = MoveAction(pos)
      actions.append(move_action)
    actions.extend(ExpandGetItem(game, pos, start, move_action, visited))

    for attr in sheet.attributes:
        if attr.HasAction(ADD_ACTION):
            actions.extend(attr.OnAction(ADD_ACTION, game, sheet, pos,
                                         move_action, visited))

  return actions

def ExpandGetItem(game, pos, start_pos, move_action, visited):
  actions = []
  for new_pos in OrthogonalPositions(pos):
    if new_pos == start_pos or new_pos in visited: continue
    e = game.EntityAt(new_pos)
    if not e or 'has_stats' in e: continue
    yield GetAction(e, move_action)
    visited.add(new_pos)

def RegisterMarkerSurface(surface_reg, rgba_color):
  s = pygame.Surface((graphics.TILE_SIZE, graphics.TILE_SIZE))
  s.fill(rgba_color)
  return surface_reg.RegisterSurface(s)

SCREEN_WIDTH = graphics.TILE_SIZE * 52
SCREEN_HEIGHT = graphics.TILE_SIZE * 45

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
    ('pos', (5,5)),
    ('image', '@'),
    ('desc', "A very simple dood.")),
  )
  player.PickUp(Item('human feet', '', 'One right, one left.',
                     StatAddMod('MOV', 4)))
  game.entities.append(player)

  SPIDER = 1
  spider = actor.Actor(PLAYER, 'spider')
  spider.UpdatePairs((
    ('pos', (10,5)),
    ('image', 'S'),
    ('desc', "Creepy and crawly.")),
  )
  spider.PickUp(Item('small health potion', '!', 'Don\'t drop it!',
                     GagueAddMod('HP', 15, 'smells nice')))
  game.entities.append(spider)

  # Spawn a random item to play with.
  for _ in range(5):
    valid_tiles = ValidTiles(game.tile_grid, game.entities)
    valid_pos = random.choice(valid_tiles)
    random_item = random.choice(item_compendium)
    game.entities.append(SpawnItem(random_item, 1, valid_pos))

  screen = pygame.display.set_mode((SCREEN_WIDTH, SCREEN_HEIGHT))

  info_surface = screen.subsurface(
    (graphics.TILE_SIZE*41, graphics.TILE_SIZE*2,
     graphics.TILE_SIZE*10, graphics.TILE_SIZE*40))

  running = True

  map_frame = graphics.Frame(
    surface_reg, screen,
    (0, 0, graphics.TILE_SIZE*40, graphics.TILE_SIZE*40))

  selector = RegisterMarkerSurface(surface_reg, [0,100,100,10])
  MoveAction.MARKER_SURFACE = RegisterMarkerSurface(surface_reg, [100,0,100,10])
  GetAction.MARKER_SURFACE = RegisterMarkerSurface(surface_reg, [50,50,100,10])
  MeleAction.MARKER_SURFACE = RegisterMarkerSurface(surface_reg, [200,50,50,10])

  actions = GenerateActions(game, player)

  previous_mouse_grid_pos = None
  desc = None

  clock = pygame.time.Clock()

  while running:
    for event in pygame.event.get():
      if event.type == pygame.QUIT:
        running = False
      elif event.type == pygame.MOUSEMOTION:
        game.mouse_pos = pygame.mouse.get_pos()
      elif event.type == pygame.MOUSEBUTTONDOWN:
        game.left_mouse = True

    time_delta = clock.tick()

    screen_center = map_frame.Dimensions() / 2
    center_to_player = Vec2d(player.pos[0] * graphics.TILE_SIZE,
                             player.pos[1] * graphics.TILE_SIZE) - screen_center
    camera_offset_diff = center_to_player - map_frame.camera_offset
    if (Distance(map_frame.camera_offset, center_to_player) >
        graphics.TILE_SIZE * 0.5):
      map_frame.camera_offset = Lerp(map_frame.camera_offset, center_to_player,
                                     1.0 / 400 * time_delta)

    grid_mouse_pos = graphics.GridPos(map_frame.camera_offset, game.mouse_pos)

    if game.left_mouse:
      decided_action = None
      for a in actions or []:
        if a.Pos() == grid_mouse_pos:
          decided_action = a

      if decided_action:
        decided_action.Run(game, player)
        actions = GenerateActions(game, player)
      else:
        # TODO: Eventually, there should be an ingame log and this'll show up
        # there.
        print('Nothing to do, there.')

    game.left_mouse = False

    GRID = 0
    ACTOR = 1
    OVERLAY = 2

    # Render the screen: tiles, entities, UI elements.
    for pos, type in game.tile_grid.Iterate():
      map_frame.AddTask(graphics.TiledPos(pos), GRID, type.surface_handle())

    for e in game.entities:
      image = e.GetOr('image')
      pos = e.GetOr('pos')
      if not (image and pos): continue
      map_frame.AddTask(graphics.TiledPos(pos), ACTOR, image)

    map_frame.AddTask(graphics.TiledPos(grid_mouse_pos), OVERLAY, selector,
                      rgb_add=True)

    for a in actions:
      for m in a.Markers():
        if not (m.pos and m.surface): continue
        map_frame.AddTask(graphics.TiledPos(m.pos), OVERLAY, m.surface,
                          rgb_add=True)

    if not game.tile_grid.HasTile(grid_mouse_pos):
      desc = None
    else:
      desc = f'{grid_mouse_pos}:\n{game.tile_grid.Get(grid_mouse_pos).desc()}'
      for e in game.entities:
        pos = e.GetOr('pos')
        if pos and pos == grid_mouse_pos:
          desc = f'{grid_mouse_pos}:\n{str(e)}'
          break

    previous_mouse_grid_pos = grid_mouse_pos

    if desc:
      graphics.BlitText(surface_reg, info_surface, desc)

    map_frame.Render()

    pygame.display.flip()
    screen.fill((0,0,0))

if __name__ == "__main__":
  main()
