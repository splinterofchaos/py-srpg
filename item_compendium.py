import stats
import actor
from actor import Entity, NOT_AN_ID, Action
import damage

def Item(name, image, description, item_stats):
  i = Entity(NOT_AN_ID)
  i = Entity(id)
  i.name = name
  i.desc = description
  i.image = image
  i.stat_sheet = stats.StatSheet()


  for s in item_stats:
    i.stat_sheet.AddStat(s)

  return i

def FractionAdd(stat_name, amount, reason=None):
  return stats.FractionStat(stat_name, amount, amount, reason)

def IntAdd(stat_name, value, reason=None):
  return stats.IntegerStat(stat_name, value, reason=reason)

def IntMult(stat_name, value, reason=None):
  return stats.IntegerStat(stat_name, 0, value, reason=reason)


# A non-exhaustive list of items in the game. Items can be defined from anywhere
# in the code, but randomly spawning items go here.
item_compendium = [
  Item('health potion sample', '!', 'It\'s practically SWAG.',
       [FractionAdd('HP', 5, 'tingly')]),
  Item('small health potion', '!', 'Don\'t drop it!',
       [FractionAdd('HP', 15, 'smells nice')]),
  Item('boring health potion', '!', 'How... ordinary.',
       [FractionAdd('HP', 25, '*sigh*')]),
  Item('large health potion', '!', 'It\s pretty big.',
       {FractionAdd('HP', 25, 'beafy')}),
  Item('dull ax', 'x', 'Ax my no questions, I fell you no pines.',
       [IntAdd('ATK', 10, 'hit things'), IntAdd('DEX', -5, 'slow')]),
  Item('lumberjack\'s ax', 'x', 'I fell pines, all right.',
       [IntAdd('ATK', 20, 'and that\'s okay'), IntAdd('DEX', -10)]),
  Item('slingshot', 'Y', 'Pull hard and aim',
       [IntAdd('ATK', 3, 'pull really hard!'),
        IntAdd('AIM', 10, 'don\'t miss!'),
        IntAdd('MATURITY', -7, 'a child\'s weapon')]),
  #Item('sleeping bag', '_', 'So tired!',
  #     [actor.RegenModifier('HP', 3, 'sleep tight')]),
  Item('necronomicon', '=', 'Makes the dead undead and the living unliving.',
       [IntAdd('WISDOM', 50, 'unearth hidden knowledge'),
        IntAdd('FAITH', -40, 'They called me blasphemous!')]),
  Item('holy relic', 'T', 'Of vague religious significance.',
       [IntAdd('FAITH', 40, 'it\'s not vague if you believe')]),
  Item('lucky penney', '$', 'Printed in 1984.',
       [IntMult('LUCK', 2, 'very lucky'), IntAdd('LUCK', 5)]),
]
