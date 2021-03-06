import pygame
from collections import defaultdict
from vec import *

BACKGROUND_COLOR = [0,0,0]
WHITE = [255,255,255]

TILE_SIZE = 20
# When not printing mono-spaced, use this as the offset.
SPACING = 1
# THe width of the ' ' character when not printing mono-spaced.
SPACE_LEN = SPACING * 2


def TiledPos(pos):
  return Vec2d(pos[0] * TILE_SIZE, pos[1] * TILE_SIZE)

def GraphicalPos(camera_offset, grid_pos):
  return ((grid_pos[0]) * TILE_SIZE - camera_offset[0],
          (grid_pos[1]) * TILE_SIZE - camera_offset[1])

def GridPos(camera_offset, graphical_pos):
  from math import floor
  return (floor(((graphical_pos[0] + camera_offset[0]) / TILE_SIZE)),
          floor(((graphical_pos[1] + camera_offset[1]) / TILE_SIZE)))

# TODO: In a language where every variable is a pointer to a reference counted
# value, what do we gain by using ID's instead of referencing the objects
# themselves?
class SurfaceRegistry:
  def __init__(self):
    self.next_id = 1
    self.registry = dict()
    self.char_registry = dict()
    self.default_font = pygame.font.SysFont(
        pygame.font.get_default_font(), TILE_SIZE)

  def RegisterSurface(self, surface):
    self.registry[self.next_id] = surface
    id = self.next_id
    self.next_id += 1
    return id

  def UnregisterSurface(self, id):
    if id in self.registry:
      del self.registry[id]

  def RegisterTextFromFont(self, text, font=None, color=WHITE,
                           background=BACKGROUND_COLOR):
    font = font or self.default_font
    return self.RegisterSurface(font.render(text, True, color, background))

  def GetChar(self, c):
    if c not in self.char_registry:
      id = self.RegisterTextFromFont(c)
      self.char_registry[c] = id
    return self.char_registry[c]

  def __getitem__(self, idx):
    if isinstance(idx, str) and len(idx) == 1:
      idx = self.GetChar(idx)
    return self.registry[idx]

def Blit(surface, dest, pos, **kwargs):
  target_rect = surface.get_rect()
  target_rect.move(pos[0], pos[1])
  if dest.get_rect().colliderect(target_rect):
    dest.blit(surface, pos, **kwargs)

def GridBlit(surface, dest, camera_offset, grid_pos, **kwargs):
  Blit(surface, dest, GraphicalPos(camera_offset, grid_pos), **kwargs)

# Blit `text` one char at a time to `dest` starting at (0, 0). Handles wrapping.
def BlitText(surface_reg, dest, text):
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
      glyphs = [surface_reg[c] for c in word]
      graphical_len = (sum([g.get_width() for g in glyphs]) +
                       (len(glyphs) - 1) * SPACE_LEN)
      if x: x, y = NewlineIfNeeded(graphical_len, x, y)

      for g in glyphs:
        x, y = NewlineIfNeeded(g.get_width(), x, y)
        Blit(g, dest, (x,y))
        x += g.get_width() + 1

      if i != len(words) - 1: x += SPACE_LEN

    x, y = Newline(x, y)

class RenderTask:
  def __init__(self, pos, surface_id, rgb_add=None):
    self.pos = pos
    self.surface_id = surface_id
    self.rgb_add = rgb_add

class Frame:
  """Manages tasks for rendering images onto a surface or subsurface."""
  def __init__(self, surface_reg, destination, bounds):
    self.subsurface = destination.subsurface(bounds)
    self.camera_offset = Vec2d(0, 0)
    self.surface_reg = surface_reg
    self.tasks = defaultdict(list)

  def Dimensions(self):
    return Vec2d(self.subsurface.get_width(), self.subsurface.get_height())

  def AddTask(self, pos, z, surface_id, rgb_add=None):
    self.tasks[z].append(RenderTask(pos, surface_id, rgb_add))

  def Render(self):
    zs = sorted(self.tasks.keys())
    for z in zs:
      for t in self.tasks[z]:
        flags = 0
        if t.rgb_add: flags |= pygame.BLEND_RGBA_ADD
        Blit(self.surface_reg[t.surface_id], self.subsurface,
             (t.pos[0] - self.camera_offset[0],
              t.pos[1] - self.camera_offset[1]),
             special_flags=flags)
    self.tasks.clear()
