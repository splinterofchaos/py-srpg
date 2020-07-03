class _Node:
  def __init__(self, value, weight):
    self.value = value
    self.weight = weight

def _Weight(node):
  return node.weight

class PriorityQueue:
  """An implementation of a priority queue using a sorted list.

  Maximum element returned first.
  """

  def __init__(self):
    self.queue = []

  def Push(self, value, weight):
    self.queue.append(_Node(value, weight))
    self.queue.sort(key=_Weight)

  def Pop(self):
    node = self.queue.pop()
    return node.value, node.weight

  def __len__(self): return self.queue.__len__()

  def values(self): return [n.value for n in self.queue]
