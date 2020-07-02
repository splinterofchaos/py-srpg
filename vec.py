class Vec2d:
  def __init__(self, x, y):
    self.x = x
    self.y = y

  def __add__(self, v):
    return Vec2d(self.x + v.x, self.y + v.y)

  def __sub__(self, v):
    return Vec2d(self.x - v.x, self.y - v.y)

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
