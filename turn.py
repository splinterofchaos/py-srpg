import actor

actor.Entity.valid_components.add('turn_points')

class Turn:
  def __init__(self):
    self.passed = False
    self.did_move = False
    self.did_attack = False

  def Over(self):
    return self.passed or (self.did_move and self.did_attack)

TP_NEEDED = 1000
DEFAULT_SPD = 10

def NextTurn(actors):
  """Adds the TP for each actor and picks the first that gets to TP_NEEDED"""
  actors_by_ticks_needed = []
  for a in actors:
    # SPD * SPD_FACTOR determines how many TP to award per tick. We're looking
    # for the number of ticks needed:
    #     turn_points + SPD*ticks_needed = TP_NEEDED
    spd = a.Stats().stats.get('SPD', DEFAULT_SPD)
    actors_by_ticks_needed.append((a, (TP_NEEDED - turn_points) / spd))
  
  snd = lambda (_, x): x
  actors_by_ticks_needed.sort(reverse=True, key=snd)

  to_add = actors_by_ticks_needed[0][1]
