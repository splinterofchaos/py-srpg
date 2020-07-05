import stats
import actor
from actor import Entity, NOT_AN_ID, Action
import damage

def Item(name, image, description, modifiers):
  i = Entity(NOT_AN_ID)
  i = Entity(id)
  i.name = name
  i.desc = description
  i.image = image
  i.modifiers = []

  if isinstance(modifiers, stats.Modifier):
    modifiers = [modifiers]
  if isinstance(modifiers, list):
    for m in modifiers:
      m.parent = i
      i.modifiers.append(m)

  return i

def GagueAddMod(stat_name, amount, reason=None):
  return stats.FractionalAdditionModifier(
      stat_name, amount, amount, reason)

def StatAddMod(stat_name, value, reason=None):
  return stats.AdditionModifier(stat_name, value, reason=reason)

def StatMultMod(stat_name, value, reason=None):
  return stats.AdditionModifier(stat_name, 0, value, reason=reason)


class MeleAction(Action):
  MARKER_SURFACE = None

  def __init__(self, damage_vector, target, move_action=None):
    self.damage_vector = damage_vector
    self.target = target
    self.move_action = move_action

  def Run(self, game, actor):
    if self.move_action: self.move_action.Run(game, actor)
    self.damage_vector.DealDamage(self.target)

  def Pos(self): return self.target.pos

  def Markers(self): 
    markers = [actor.Marker(self.Pos(), self.MARKER_SURFACE)]
    if self.move_action:
      markers.extend(self.move_action.Markers())
    return markers


ADD_ACTION = 'add-action'


class AxAttackModifier(stats.Modifier):
  def __init__(self, a=None):
    super().__init__('STR')
    self.actor = a

    def AddMeleAction(game, sheet, pos, move_action, visited):
      STR = sheet.stats.get('STR', stats.IntegerStat('STR', 0)).value
      damage_vec = damage.DamageVector([damage.Damage(STR)])
      for step in ((-1, 0), (1, 0), (0, -1), (0, 1)):
        new_pos = (pos[0] + step[0], pos[1] + step[1])
        if new_pos in visited: continue
        e = game.EntityAt(new_pos)
        if e and 'has_stats' in e:
          visited.add(new_pos)
          yield MeleAction(damage_vec, e, move_action)
    self.actions[ADD_ACTION] = AddMeleAction

  def EffectRepr(self):
    return 'Ax attack'

  def ModifyStatSheet(self, sheet):
    sheet.attributes.append(AxAttackModifier(self.parent.parent))


# A non-exhaustive list of items in the game. Items can be defined from anywhere
# in the code, but randomly spawning items go here.
item_compendium = [
  Item('health potion sample', '!', 'It\'s practically SWAG.',
       GagueAddMod('HP', 5, 'tingly')),
  Item('small health potion', '!', 'Don\'t drop it!',
       GagueAddMod('HP', 15, 'smells nice')),
  Item('boring health potion', '!', 'How... ordinary.',
       GagueAddMod('HP', 25, '*sigh*')),
  Item('large health potion', '!', 'It\s pretty big.',
       GagueAddMod('HP', 25, 'beafy')),
  Item('dull ax', 'x', 'Ax my no questions, I fell you no pines.',
       [StatAddMod('STR', 10, 'hit things'), StatAddMod('DEX', -5, 'slow'),
        AxAttackModifier()]),
  Item('lumberjack\'s ax', 'x', 'I fell pines, all right.',
       [StatAddMod('STR', 20, 'and that\'s okay'), StatAddMod('DEX', -10),
        AxAttackModifier()]),
  Item('slingshot', 'Y', 'Pull hard and aim',
       [StatAddMod('STR', 3, 'pull really hard!'),
        StatAddMod('AIM', 10, 'don\'t miss!'),
        StatAddMod('MATURITY', -7, 'a child\'s weapon')]),
  Item('sleeping bag', '_', 'So tired!',
       [actor.RegenModifier('HP', 3, 'sleep tight')]),
  Item('necronomicon', '=', 'Makes the dead undead and the living unliving.',
       [StatAddMod('WISDOM', 50, 'unearth hidden knowledge'),
        StatAddMod('FAITH', -40, 'They called me blasphemous!')]),
  Item('holy relic', 'T', 'Of vague religious significance.',
       [StatAddMod('FAITH', 40, 'it\'s not vague if you believe')]),
  Item('lucky penney', '$', 'Printed in 1984.',
       [StatMultMod('LUCK', 2, 'very lucky'), StatAddMod('LUCK', 5)]),
]
