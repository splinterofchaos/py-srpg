import math

class Vec2d:
  def __init__(self, x, y):
    self.x = x
    self.y = y

  def __add__(self, v):
    return Vec2d(self.x + v[0], self.y + v[1])

  def __sub__(self, v):
    return Vec2d(self.x - v[0], self.y - v[1])

  def __getitem__(self, i):
    if i == 0: return self.x
    if i == 1: return self.y
    raise 'unexpected index: ' + str(i)

  def __mul__(self, x):
    return Vec2d(self.x * x, self.y * x)

  def __truediv__(self, x):
    return Vec2d(self.x / x, self.y / x)

  def __neg__(self):
    return Vec2d(-self.x, -self.y)

  def MagnitudeSquared(self):
    return self.x * self.x + self.y * self.y

def DistanceSquared(a, b):
  x = a.x - b.x
  y = a.y - b.y
  return x * x + y * y;

def Distance(a, b):
  return math.sqrt(DistanceSquared(a, b))

def Lerp(a, b, r):
  """Returns a vector between a and b, a if r=0, b if r=1"""
  d = b - a
  return a + (d * r)