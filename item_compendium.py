import stats
import actor
from actor import Entity, NOT_AN_ID, Properties

def Item(name, description, modifiers):
  i = Entity(NOT_AN_ID)
  i = Entity(id)
  i[Properties.NAME] = name
  i[Properties.DESC] = description
  i[Properties.MODIFIERS] = []

  if isinstance(modifiers, stats.Modifier):
    modifiers = [modifiers]
  if isinstance(modifiers, list):
    for m in modifiers:
      m.parent = i
      i[Properties.MODIFIERS].append(m)

  return i

def GagueAddMod(stat_name, amount, reason=None):
  return stats.FractionalAdditionModifier(
      stat_name, amount, amount, reason)

def StatAddMod(stat_name, value, reason=None):
  return stats.AdditionModifier(stat_name, value, reason=reason)

item_compendium = [
  Item('health potion sample', 'It\'s practically SWAG.',
       GagueAddMod('HP', 5, 'tingly')),
  Item('small health potion', 'Don\'t drop it!',
       GagueAddMod('HP', 15, 'smells nice')),
  Item('boring health potion', 'How... ordinary.',
       GagueAddMod('HP', 25, '*sigh*')),
  Item('large health potion', 'It\s pretty big.',
       GagueAddMod('HP', 25, 'beafy')),
  Item('dull ax', 'Ax my no questions, I fell you no pines.',
       [StatAddMod('STR', 10, 'hit things'), StatAddMod('DEX', -5, 'slow')]),
  Item('lumberjack\'s ax', 'I fell pines, all right.',
       [StatAddMod('STR', 20, 'and that\'s okay'), StatAddMod('DEX', -10)]),
  Item('slingshot', 'Pull hard and aim',
       [StatAddMod('STR', 3, 'pull really hard!'),
        StatAddMod('AIM', 10, 'don\'t miss!'),
        StatAddMod('MATURITY', -7, 'a child\'s weapon')]),
  Item('sleeping bag', 'So tired!',
       [actor.RegenModifier('HP', 3, 'sleep tight')]),
  Item('necronomicon', 'Makes the dead undead and the living unliving.',
       [StatAddMod('WISDOM', 50, 'unearth hidden knowledge'),
        StatAddMod('FAITH', -40, 'They called me blasphemous!')]),
  Item('holy relic', 'Of vague religious significance.',
       [StatAddMod('FAITH', 40, 'it\'s not vague if you believe')]),
]