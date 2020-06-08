import pygame
import typing
import random
import copy

import actor

from item_compendium import item_compendium

BACKGROUND_COLOR = [0,0,0]
WHITE = [255,255,255]

# TODO: In a language where every variable is a pointer to a reference counted
# value, what do we gain by using ID's instead of referencing the objects
# themselves?
class SurfaceRegistry:
  def __init__(self):
    self.next_id_ = 1
    self.registry_ = dict()
    self.char_registry_ = dict()

  def RegisterSurface(self, surface):
    self.registry_[self.next_id_] = surface
    id = self.next_id_
    self.next_id_ += 1
    return id

  def UnregisterSurface(self, id):
    if id in self.registry_:
      del self.registry_[id]

  def RegisterTextFromFont(self, font, text, color=WHITE,
                           background=BACKGROUND_COLOR):
    return self.RegisterSurface(font.render(text, True, color, background))

  def GetChar(self, font, c):
    if c not in self.char_registry_:
      id = self.RegisterTextFromFont(font, c)
      self.char_registry_[c] = id
    return self[self.char_registry_[c]]

  def __getitem__(self, idx):
    return self.registry_[idx]

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
def Spawn(entity_template, id, position, image):
  # TODO: Not all things are safe to copy like this. Consider making a Copy()
  # function within Modifier.
  e = copy.deepcopy(entity_template)
  e.id = id
  e[actor.Properties.POS] = position
  e[actor.Properties.IMAGE] = image
  return e

# Some standard components
POS = "pos"
IMAGE = "image"
DESC = "description"

TILE_SIZE = 20
# When not printing mono-spaced, use this as the offset.
SPACING = 1
# THe width of the ' ' character when not printing mono-spaced.
SPACE_LEN = SPACING * 2

# Blit `text` one char at a time to `dest` starting at (0, 0). Handles wrapping.
def blit_text(surfaces, dest, font, text):
  x, y = 0, 0

  def Newline(x, y):
    y += TILE_SIZE
    x = 0
    return x, y

  def NewlineIfNeeded(surface_len, x=x, y=y):
    if x + surface_len >= dest.get_width():
      x, y = Newline(x, y)
    return x, y

  # Blit a word at a time.
  lines = text.split('\n')
  for line in lines:
    words = line.split(' ')
    for i, word in enumerate(words):
      glyphs = [surfaces.GetChar(font, c) for c in word]
      graphical_len = (sum([g.get_width() for g in glyphs]) +
                       (len(glyphs) - 1) * SPACE_LEN)
      if x: x, y = NewlineIfNeeded(graphical_len, x, y)

      for g in glyphs:
        x, y = NewlineIfNeeded(g.get_width(), x, y)
        dest.blit(g, (x,y))
        x += g.get_width() + 1

      if i != len(words) - 1: x += SPACE_LEN

    x, y = Newline(x, y)

def GraphicalPos(camera_offset, grid_pos):
  return ((grid_pos[0] - camera_offset[0]) * TILE_SIZE,
          (grid_pos[1] - camera_offset[1]) * TILE_SIZE)

def GridPos(camera_offset, graphical_pos):
  from math import floor
  return (floor((graphical_pos[0] / TILE_SIZE) + camera_offset[0]),
          floor((graphical_pos[1] / TILE_SIZE) + camera_offset[1]))

WINDOW_SIZE = (1000, 1000)
LOOK_DESCRIPTION_START = (600, 100)

def main():
  random.seed()
  pygame.init()
  pygame.display.set_caption("hello world")
  pygame.font.init()

  font = pygame.font.SysFont(pygame.font.get_default_font(), 20)

  surfaces = SurfaceRegistry()

  tile_types = {
      '#': TileType(False, surfaces.RegisterTextFromFont(font, '#'),
                    'A hard stone wall.'),
      '.': TileType(True, surfaces.RegisterTextFromFont(font, '.'),
                    'A simple stone floor.'),
  }
  tile_grid = TileGridFromString(tile_types, SILLY_STARTING_MAP_FOR_TESTING)

  # TODO: Might use a dict in the future. Otherwise, entities don't really need
  # an ID.
  entities = []

  PLAYER = 0
  player = actor.Actor(PLAYER, 'player')
  player.UpdatePairs((
    (actor.Properties.POS, (5,5)),
    (actor.Properties.IMAGE, surfaces.RegisterTextFromFont(font, '@')),
    (actor.Properties.DESC, "A very simple dood.")),
  )
  entities.append(player)

  # Spawn a random item to play with.
  valid_tiles = ValidTiles(tile_grid, entities)
  valid_pos = random.choice(valid_tiles)
  random_item = random.choice(item_compendium)
  entities.append(Spawn(random_item, 1, valid_pos,
                        surfaces.RegisterTextFromFont(font, 'I')))

  screen = pygame.display.set_mode((TILE_SIZE*52, TILE_SIZE*45))

  map_surface = screen.subsurface((0, 0, TILE_SIZE*40, TILE_SIZE*40))
  info_surface = screen.subsurface((TILE_SIZE*41, TILE_SIZE*2,
                                    TILE_SIZE*10, TILE_SIZE*10))

  running = True

  camera_offset = (0, 0)

  selector_surface = pygame.Surface((TILE_SIZE, TILE_SIZE))
  selector_surface.fill([0,100,100,10])

  previous_mouse_grid_pos = None
  desc = None
  while running:
    for event in pygame.event.get():
      if event.type == pygame.QUIT:
        running = False

    for (x,y), type in tile_grid.Iterate():
      map_surface.blit(surfaces[tile_grid.Get(x, y).surface_handle()],
                       GraphicalPos(camera_offset, (x, y)))

    for e in entities:
      image = e.Get(actor.Properties.IMAGE)
      pos = e.Get(actor.Properties.POS)
      if not (image and pos): continue
      map_surface.blit(surfaces[image], GraphicalPos(camera_offset, pos))

    mouse_pos = GridPos(camera_offset, pygame.mouse.get_pos())
    map_surface.blit(selector_surface, GraphicalPos(camera_offset, mouse_pos),
                     special_flags=pygame.BLEND_RGBA_ADD)

    if (previous_mouse_grid_pos is not None and
        previous_mouse_grid_pos != mouse_pos):
      if not tile_grid.HasTile(mouse_pos):
        desc = None
      else:
        desc = f'{mouse_pos}:\n{tile_grid.Get(mouse_pos).desc()}'
        for e in entities:
          if e.Get(actor.Properties.POS) == mouse_pos:
            desc = f'{mouse_pos}:\n{str(e)}'
            break

    previous_mouse_grid_pos = mouse_pos

    if desc:
      blit_text(surfaces, info_surface, font, desc)

    pygame.display.flip()
    screen.fill((0,0,0))

if __name__ == "__main__":
  main()
