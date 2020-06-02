import copy
from enum import Enum, auto

# A stat can be integral or fractional; subclasses differentiate the two.
class Stat:
  def __init__(self, name):
    self.name = name

  def __repr__(self): pass


# Flat stats like STR or HIT.
class IntegerStat(Stat):
  def __init__(self, name, value=0, multiplier=1):
    super(IntegerStat, self).__init__(name)
    self.value = value
    self.multiplier = multiplier

  def __repr__(self): return f'{self.name}: {self.value * self.multiplier}'


# Stats like HP, MP, etc..
class FractionStat(Stat):
  def __init__(self, name, numerator, denominator):
    super(FractionStat, self).__init__(name)
    self.n = numerator
    self.d = denominator

  def __repr__(self): return f'{self.name}: {self.n}/{self.d}'


# These are special attributes which always exist.
HP = 'HP'  # If not present or zero, the actor is dead.
SPEED = 'SPD'  # Determines move order
HIT = 'HIT'  # The chance to land a blow out of 100.


# A collection of stats and attributes. The sum of modifiers on an item or
# actor.
class StatSheet:
  def __init__(self):
    self.stats = {}
    self.attributes = []

  def __repr__(self):
    return '\n'.join(map(str, list(self.stats.values()) + self.attributes))


# Modifies a stat sheet or actor in various ways.
class Modifier:  
  def __init__(self, reason=None):
    self.actions = {}
    self.reason = reason
    self.parent = None

  def EffectRepr(self): pass
  def __repr__(self):
    effect = self.EffectRepr()
    if self.reason:
      return f'{effect} ({self.reason})'
    else:
      return effect

  def HasAction(self, action_id): return action_id in self.actions

  def OnAction(self, action_id, *args, **kwargs):
    return self.actions[action_id](*args, **kwargs)

  # Modifies the stat sheet in place, if this modifier does so.
  def ModifyStatSheet(self, sheet): pass

  # Modifies incoming damage and returns the result.
  def ModifyIncomingDamage(self, amount): return amount


def ModifierPrefix(x): return '+' if x >= 0 else '-'


# Adds to a stat or its multiplier.
class AdditionModifier(Modifier):
  def __init__(self, name, value=0, multiplier=1, reason=None):
    super(AdditionModifier, self).__init__(reason)
    self.name = name
    assert bool(value) ^ bool(multiplier)
    self.value = value
    self.multiplier = multiplier

  def EffectRepr(self):
    pre = '+' if self.value > 0 and self.multiplier > 0 else '-'
    if self.multiplier:
      # TODO: convert multiplier to integer out of 100 first
      return (f'{self.name}: '
              f'{ModifierPrefix(self.multiplier)}{self.multiplier*100}%')
    else:
      return f'{self.name}: {ModifierPrefix(self.value)}{self.value}'

  def ModifyStatSheet(self, sheet):
    stats = sheet.stats()
    if self.name not in stats:
      stats[name] = IntegerValue(self.name, self.value, self.multiplier)
    else:
      stat = stats[stat]
      stat.value += self.value
      stat.multiplier *= self.multiplier


# TODO: support multipliers on fractions, but on the numerator, denominator,
# or both?
CONSUME = 'consume'
REFUND = 'refund'
class FractionalAdditionModifier(Modifier):
  def __init__(self, name, n=0, d=0,reason=None):
    super(FractionalAdditionModifier, self).__init__(reason)
    self.name = name
    self.n = n
    self.d = d

    def Consume(stat, amount):
      if stat != self.name: return amount
      left = max(0, self.n - amount)
      amount -= min(self.n, amount)
      self.n = left
      return amount
    self.actions[CONSUME] = Consume

    def Refund(stat, amount):
      if stat != self.name: return amount
      have = min(self.d, self.n + amount)
      amount -= min(amount, self.d - self.n)
      self.n = have
      return amount
    self.actions[REFUND] = Refund

  def EffectRepr(self):
    return (f'{self.name}: '
            f'{ModifierPrefix(self.n)}{self.n}/'
            f'{ModifierPrefix(self.d)}{self.d}')

  def ModifyStatSheet(self, sheet):
    if self.name not in sheet.stats:
      sheet.stats[self.name] = FractionStat(self.name, self.n, self.d)
    else:
      stat = sheet.stats[self.name]
      stat.n += self.n
      stat.d += self.d


def Hp(have, total, reason=None):
  return FractionalAdditionModifier(HP, have, total, reason)


MODIFY_INCOMING_DAMAGE = 'modify incomming damage'


class TemperatureModifier(Modifier):
  class Value(Enum):
    COLD = -10
    LUKWARM = 0
    HOT = 10
  
  def __init__(self, value, reason=None):
    # We pass the value as the name. Both are the same.
    super(TemperatureModifier, self).__init__(reason)
    self.value = value
    
    def ModifyIncomingDamage(amount):
      if self.value == TemperatureModifier.Value.COLD:
        amount /= 2
      elif self.value == TemperatureModifier.Value.HOT:
        amount *= 2
      return amount
    self.actions[MODIFY_INCOMING_DAMAGE] = ModifyIncomingDamage

  # When we want to copy ourselves, but forget the reason (such as when copying
  # into a stat sheet.
  def CopyNoReason(self):
    ret = copy.deepcopy(self)
    ret.reason = None
    return ret

  def EffectRepr(self): return self.value.name

  # Hot and cold mix to become lukwarm.
  # Hot and hot is just hot and ditto for cold.
  def ModifyStatSheet(self, sheet):
    V = TemperatureModifier.Value
    for i, a in enumerate(sheet.attributes):
      if type(a) == TemperatureModifier:
        if self.value == V.LUKWARM or a == self:
          pass
        if a.value == V.LUKWARM:
          sheet.attributes[i] = self.CopyNoReason()
        else:
          # By process of elimination, we are hot, they are cold, or the other
          # way around. The result is the same either way.
          sheet.attributes[i] = TemperatureModifier(V.LUKWARM)
        return

    sheet.attributes.append(self.CopyNoReason())


# Anything that exists in this world.
class Item:
  def __init__(self, name, description=None):
    self.name = name
    self.description = description
    self.modifiers = []
    self.parent = None

  def AddModifier(self, mod):
    mod.parent = self
    self.modifiers.append(mod)

  def OnAction(self, action, *args, **kwargs):
    for m in self.modifiers:
      if m.HasAction(action): m.OnAction(action, *args, **kwargs)

  def __repr__(self):
    return self.name + '\n' + '\n'.join(map(str, self.modifiers))


class Actor:
  def __init__(self, name):
    self.name = name
    self.inventory = []

  def PickUp(self, entity):
    entity.parent = self
    self.inventory.append(entity)

  def ItemNames(self):
    for i in self.inventory: yield i.name

  def Stats(self):
    sheet = StatSheet()
    for i in self.inventory:
      for m in i.modifiers:
        m.ModifyStatSheet(sheet)
    return sheet

  def OnAction(self, action, *args, **kwargs):
    for i in self.inventory:
      i.OnAction(action, *args, **kwargs)

  def __repr__(self):
    stats = self.Stats()
    return '%s\n%s\nHolding: %s' % (
        self.name,
        stats,
        ', '.join(self.ItemNames())
    )
    return self.name() + '\n' + '\n'.join(map(str, sheet.attributes))


# TODO: We probably want each modifier and item to refer to its parent in some
# way. Otherwise, how does regen know what to generate?
REGEN = 'regen'
class RegenModifier(Modifier):
  def __init__(self, name, amount=1, reason=None):
    super().__init__(reason)
    self.name = name
    self.amount = amount

    def Regen():
      amount = self.amount

      def RegenItem(item, amount):
        for m in i.modifiers:
          if m.HasAction(REFUND):
            amount = m.OnAction(REFUND, self.name, amount)
        return amount

      item = self.parent
      if item.parent:
        for i in item.parent.inventory:
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
    sheet.attributes.append(copy.deepcopy(self))


def DealDamage(actor, amount):
  sheet = actor.Stats()
  for a in sheet.attributes:
    if a.HasAction(MODIFY_INCOMING_DAMAGE):
      amount = a.OnAction(MODIFY_INCOMING_DAMAGE, amount)
  amount = int(amount)

  for i in actor.inventory:
    for m in i.modifiers:
      if m.HasAction(CONSUME):
        amount = m.OnAction(CONSUME, 'HP', amount)


def Test_MakeItem(name, modifiers):
  i = Item(name)
  for m in modifiers: i.AddModifier(m)
  print('created item:\n%s\n' % i)
  return i

def Test_MakeActor(name, inventory):
  a = Actor(name)
  for i in inventory: a.PickUp(i)
  print('created actor:\n%s\n' % a)
  return a

def Test_Pickup(actor, item):
  actor.PickUp(item)
  print(actor.name, 'picked up', item, '\n')

def Test_ActorTakesDamage(actor, d):
  DealDamage(a, d)
  print('%s took %s damage\n%s\n' % (actor.name, d, actor))

potion1 = Test_MakeItem('health potion', [Hp(5, 5)])
potion2 = Test_MakeItem('donut', [Hp(5, 5, 'tasty')])
a = Test_MakeActor('foo', [potion1, potion2])
Test_ActorTakesDamage(a, 5)

ice = Test_MakeItem('ice cube', [TemperatureModifier(TemperatureModifier.Value.COLD)])
Test_Pickup(a, ice)
Test_ActorTakesDamage(a, 5)

coffee = Test_MakeItem('coffee', [TemperatureModifier(TemperatureModifier.Value.HOT, 'steamy')])
Test_Pickup(a, coffee)
Test_ActorTakesDamage(a, 1)
Test_Pickup(a, coffee)
Test_ActorTakesDamage(a, 1)

hand_lotion = Test_MakeItem('hand lotion', [RegenModifier(HP, 1)])
Test_Pickup(a, hand_lotion)
a.OnAction(REGEN)
print(a)

