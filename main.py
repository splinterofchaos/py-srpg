import pygame
import typing

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

  def register_surface(self, surface):
    self.registry_[self.next_id_] = surface
    id = self.next_id_
    self.next_id_ += 1
    return id

  def unregister_surface(self, id):
    if id in self.registry_:
      del self.registry_[id]

  def register_text_from_font(self, font, text, color=WHITE,
                              background=BACKGROUND_COLOR):
    return self.register_surface(font.render(text, True, color, background))

  def get_char(self, font, c):
    if c not in self.char_registry_:
      id = self.register_text_from_font(font, c)
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

  def get(self, xy, y=None):
    return self.grid_[xy if y is None else (xy, y)]

  def iterate(self):
    for (x, y), type in self.grid_.items():
      yield (x, y), type

  def set(self, x, y, tile_type):
    self.min_x_ = x if self.min_x_ is None else min(self.min_x_, x)
    self.min_y_ = y if self.min_y_ is None else min(self.min_y_, y)
    self.max_x_ = x if self.max_x_ is None else max(self.max_x_, x)
    self.max_y_ = y if self.max_y_ is None else max(self.max_y_, y)
    self.grid_[(x, y)] = tile_type

  def register_tile_type(self, handle, type):
    # TODO: make sure an entry doesn't already exist
    self.tile_types_[handle] = type

  def has_tile(self, xy, y=None):
    return (xy if y is None else (x,y)) in self.grid_

  def min_x_y(self): return (self.min_x_, self.min_y_)
  def max_x_y(self): return (self.max_x_, self.max_y_)

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

def tile_grid_from_string(tile_types, string_map):
  grid = TileGrid()
  for row, line in enumerate(string_map.split('\n')):
    for col, c in enumerate(line):
      if c == ' ': continue
      grid.set(col, row, tile_types[c])
  return grid


class Entity:
  def __init__(self, id):
    self.id_ = id
    self.components_ = dict()

  def set(self, key, data): self.components_[key] = data
  def get(self, key, default=None):
     return self.components_[key] if key in self.components_ else default

  def set_many(self, key_datas):
    for key, data in key_datas: self.set(key, data)

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

  def new_line_if_needed(surface_len, x=x, y=y):
    if x + surface_len >= dest.get_width():
      y += TILE_SIZE
      x = 0
    return x, y

  # Blit a word at a time.
  words = text.split(' ')
  for i, word in enumerate(words):
    glyphs = [surfaces.get_char(font, c) for c in word]
    graphical_len = (sum([g.get_width() for g in glyphs]) +
                     (len(glyphs) - 1) * SPACE_LEN)
    if x: x, y = new_line_if_needed(graphical_len, x, y)

    for g in glyphs:
      x, y = new_line_if_needed(g.get_width(), x, y)
      dest.blit(g, (x,y))
      x += g.get_width() + 1

    if i != len(words) - 1: x += SPACE_LEN

def graphical_pos(camera_offset, grid_pos):
  return ((grid_pos[0] - camera_offset[0]) * TILE_SIZE,
          (grid_pos[1] - camera_offset[1]) * TILE_SIZE)

def grid_pos(camera_offset, graphical_pos):
  from math import floor
  return (floor((graphical_pos[0] / TILE_SIZE) + camera_offset[0]),
          floor((graphical_pos[1] / TILE_SIZE) + camera_offset[1]))

WINDOW_SIZE = (1000, 1000)
LOOK_DESCRIPTION_START = (600, 100)

def main():
  pygame.init()
  pygame.display.set_caption("hello world")
  pygame.font.init()

  font = pygame.font.SysFont(pygame.font.get_default_font(), 20)

  surfaces = SurfaceRegistry()

  tile_types = {
      '#': TileType(False, surfaces.register_text_from_font(font, '#'),
                    'A hard stone wall.'),
      '.': TileType(True, surfaces.register_text_from_font(font, '.'),
                    'A simple stone floor.'),
  }
  tile_grid = tile_grid_from_string(tile_types, SILLY_STARTING_MAP_FOR_TESTING)

  entities = []
  player = Entity(0)
  player.set_many(((POS, (5,5)),
                   (IMAGE, surfaces.register_text_from_font(font, '@')),
                   (DESC, "A very simple dood, but with too many words.")))
  entities.append(player)

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

    for (x,y), type in tile_grid.iterate():
      map_surface.blit(surfaces[tile_grid.get(x, y).surface_handle()],
                       graphical_pos(camera_offset, (x, y)))

    for e in entities:
      image = e.get(IMAGE)
      pos = e.get(POS)
      if not (image and pos): continue
      map_surface.blit(surfaces[image], graphical_pos(camera_offset, pos))

    mouse_pos = grid_pos(camera_offset, pygame.mouse.get_pos())
    map_surface.blit(selector_surface, graphical_pos(camera_offset, mouse_pos),
                     special_flags=pygame.BLEND_RGBA_ADD)

    if (previous_mouse_grid_pos is not None and
        previous_mouse_grid_pos != mouse_pos):
      if not tile_grid.has_tile(mouse_pos)):
        desc = None
      else:
        desc = tile_grid.get(mouse_pos).desc()
        for e in entities:
          if e.get(POS) == mouse_pos:
            desc = e.get(DESC)
            break

    previous_mouse_grid_pos = mouse_pos

    if desc:
      blit_text(surfaces, info_surface, font, desc)

    pygame.display.flip()
    screen.fill((0,0,0))

if __name__ == "__main__":
  main()
