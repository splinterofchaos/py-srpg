from enum import Enum, auto

import stats


def Hp(have, total, reason=None):
  return stats.FractionalAdditionModifier('HP', have, total, reason)

NOT_AN_ID = -1

class Entity:
  # Add your property to this before using them!
  valid_components = {'modifiers', 'pos',
                      'name', 'desc', 'has_stats', 'image'}
  instance_properties = {'id', 'components', 'children', 'parent'}

  def __init__(self, id):
    self.id = id
    self.components = dict()
    self.children = []
    self.parent = None

  def Set(self, key, data):
    # Needed to bootstrap the class.
    if key in Entity.instance_properties:
      return super().__setattr__(key, data)

    if key not in Entity.valid_components:
      raise AttributeError(f'Component "{key}" not registered in '
                           f'Entity.valid_components. Valid properties are:'
                           f'{", ".join(Entity.valid_components)}')

    self.components[key] = data

  def Get(self, key):
    class DefinitelyAnError: pass
    result = self.__getattribute__('components').get(key, DefinitelyAnError)
    if result == DefinitelyAnError:
      raise AttributeError(f'This entity does not have the attribute, {key}.')
    return result

  def GetOr(self, key, default=None):
    return getattr(self, key, default)

  def Stats(self):
    sheet = stats.StatSheet()
    for c in self.children:
      for m in c.modifiers:
        m.ModifyStatSheet(sheet)
    return sheet

  def PickUp(self, entity):
    if entity.parent: entity.parent.children.remove(entity)
    entity.parent = self
    self.children.append(entity)

  def __setattr__(self, key, data): self.Set(key, data)
  def __getattr__(self, key): return self.Get(key)
  def __delattr__(self, key): del self.components[key]
  def __iter__(self): return self.components.__iter__()
  def __contains__(self, key): return self.components.__contains__(key)

  def __repr__(self):
    lines = []

    name = self.GetOr('name')
    if name: lines.append(name)

    desc = self.GetOr('desc')
    if desc: lines.append(f'"{desc}"')

    if self.GetOr('has_stats'):
      lines.append(str(self.Stats()))

    modifiers = self.GetOr('modifiers')
    if modifiers:
      lines.extend(str(m) for m in modifiers)

    if self.children:
      inventory = [c.name for c in self.children]
      lines.append(f'holding: {", ".join(inventory)}')

    return '\n'.join(lines)


  def UpdatePairs(self, key_datas):
    for key, data in key_datas: self.Set(key, data)


def AddModifier(e, mod):
  mod.parent = e
  e.modifiers.append(mod)
  e.modifiers.append(mod)

def OnAction(e, action, *args, **kwargs):
  for c in e.children: OnAction(c, action, *args, **kwargs)
  for m in e.Get('MODIFIERS', []):
    if m.HasAction(action):
      m.OnAction(action, *args, **kwargs)


def DealDamage(actor, amount):
  sheet = actor.Stats()
  for a in sheet.attributes:
    if a.HasAction(stats.MODIFY_INCOMING_DAMAGE):
      amount = a.OnAction(stats.MODIFY_INCOMING_DAMAGE, amount)
  amount = int(amount)

  for i in actor.children:
    for m in i.modifiers:
      if m.HasAction(stats.CONSUME):
        amount = m.OnAction(stats.CONSUME, 'HP', amount)


class Damage:
  def __init__(self, amount, resistence=None):
    self.amount = amount
    self.resistence = resistence

  def ResolveAmount(self, stat_sheet):
    resist_amount = stat_sheet.stats.get(self.resistence, 0)
    return self.amount - resist_amount


class DamageVector:
  def __init__(self, damages, resistences = None):
    self.damages = damages
    self.resistences = resistences or {}

  def DealDamage(self, defender):
    sheet = defender.Stats()

    amount = 0
    for damage in self.damages:
      amount += damage.ResolveAmount(sheet)

    DealDamage(defender, amount)

    return amount

# Represents the existence and execution of any valid action. This allows us to
# abstract player/AI decision making as well as get data on all possible
# actions. Marker() exists to specify if the UI should draw a marker at any
# location.
class Action:
  # Changes the state of the game to reflect the action.
  def Run(self, game, actor): pass
  # If this action is on a position (like to where we would move or attack),
  # returns that position so it can be marked in the UI.
  def Marker(self): pass


def Actor(id, name):
  e = Entity(id)
  e.name = name
  e.has_stats = True
  return e


# TODO: This class needs to know about the Properties enum, but is this the best
# place for it?
REGEN = 'regen'
class RegenModifier(stats.Modifier):
  def __init__(self, name, amount=1, reason=None):
    super().__init__(reason)
    self.name = name
    self.amount = amount

    def Regen():
      amount = self.amount

      def RegenItem(item, amount):
        for m in item.modifiers:
          if m.HasAction(stats.REFUND):
            amount = m.OnAction(stats.REFUND, self.name, acc=amount)
        return amount

      item = self.parent
      if item.parent:
        for i in item.parent.children:
          amount = RegenItem(i, amount)
      else:
        amount = RegenItem(item, amount)
    self.actions[REGEN] = Regen

  def EffectRepr(self):
    return f'{self.name} regen x{self.amount}'

  def ModifyStatSheet(self, sheet):
    for a in sheet.attributes:
      if isinstance(a, RegenModifier) and a.name == self.name:
        a.amount += self.amount
        return
    copy = RegenModifier(self.name, self.amount)
    copy.parent = self.parent
    sheet.attributes.append(copy)
