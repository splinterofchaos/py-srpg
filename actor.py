from enum import Enum, auto

import stats


def Hp(have, total, reason=None):
  return stats.FractionalAdditionModifier('HP', have, total, reason)

NOT_AN_ID = -1

class Entity:
  def __init__(self, id):
    self.id = id
    self.components = dict()
    self.children = []
    self.parent = None

  def Set(self, key, data): self.components[key] = data
  def Get(self, key, default=None):
     return self.components[key] if key in self.components else default

  def Stats(self):
    sheet = stats.StatSheet()
    for c in self.children:
      for m in c[Properties.MODIFIERS]:
        m.ModifyStatSheet(sheet)
    return sheet

  def PickUp(self, entity):
    if entity.parent: entity.parent.children.remove(entity)
    entity.parent = self
    self.children.append(entity)

  def __setitem__(self, key, data): self.Set(key, data)
  def __getitem__(self, key): return self.Get(key)
  def __delitem__(self, key): del self.components[key]
  def __iter__(self): return self.components.__iter__()
  def __contains__(self, key): return self.components.__contains__(key)

  def __repr__(self):
    lines = []
    name = self.Get(Properties.NAME)
    if name: lines.append(name)

    desc = self.Get(Properties.DESC)
    if desc: lines.append(f'"{desc}"')

    if Properties.HAS_STATS in self:
      lines.append(str(self.Stats()))

    if Properties.MODIFIERS in self:
      lines.extend(str(m) for m in self[Properties.MODIFIERS])

    if self.children:
      inventory = [c[Properties.NAME] for c in self.children]
      lines.append(f'holding: {", ".join(inventory)}')

    return '\n'.join(lines)


  def UpdatePairs(self, key_datas):
    for key, data in key_datas: self.Set(key, data)

# Stabdard entity properties.
class Properties(Enum):
  NAME = 'NAME'
  DESC = 'DESC'
  PARENT = 'PARENT'
  POS = 'POS'
  IMAGE = 'IMAGE'
  MODIFIERS = 'MODIFIERS'
  HAS_STATS = 'HAS_STATS'


def Item(id, name, description=None):
  e = Entity(id)
  e[Properties.NAME] = name
  e[Properties.DESC] = description
  e[Properties.MODIFIERS] = []
  return e

def AddModifier(e, mod):
  mod.parent = e
  e[Properties.MODIFIERS].append(mod)

def OnAction(e, action, *args, **kwargs):
  for c in e.children: OnAction(c, action, *args, **kwargs)
  for m in e.Get(Properties.MODIFIERS, []):
    if m.HasAction(action):
      m.OnAction(action, *args, **kwargs)

def Actor(id, name):
  e = Entity(id)
  e[Properties.NAME] = name
  e[Properties.HAS_STATS] = True
  return e

def DealDamage(actor, amount):
  sheet = actor.Stats()
  for a in sheet.attributes:
    if a.HasAction(stats.MODIFY_INCOMING_DAMAGE):
      amount = a.OnAction(stats.MODIFY_INCOMING_DAMAGE, amount)
  amount = int(amount)

  for i in actor.children:
    for m in i[Properties.MODIFIERS]:
      if m.HasAction(stats.CONSUME):
        amount = m.OnAction(stats.CONSUME, 'HP', amount)


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
        for m in item[Properties.MODIFIERS]:
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


if __name__ == '__main__':
  _Test_id = 0

  def Test_MakeItem(name, modifiers):
    global _Test_id
    _Test_id += 1
    i = Item(_Test_id, name)
    for m in modifiers: AddModifier(i, m)
    print('created item:\n%s\n' % i)
    return i

  def Test_MakeActor(name, inventory):
    global _Test_id
    _Test_id += 1
    a = Actor(_Test_id, name)
    for i in inventory: a.PickUp(i)
    print('created actor:\n%s\n' % a)
    return a

  def Test_Pickup(actor, item):
    actor.PickUp(item)
    print(f'{actor[Properties.NAME]} picked up {item}')
    print()

  def Test_ActorTakesDamage(actor, d):
    DealDamage(a, d)
    print('%s took %s damage\n%s\n' % (actor[Properties.NAME], d, actor))

  potion1 = Test_MakeItem('health potion', [Hp(5, 5)])
  potion2 = Test_MakeItem('donut', [Hp(5, 5, 'tasty')])
  a = Test_MakeActor('foo', [potion1, potion2])
  Test_ActorTakesDamage(a, 5)

  ice = Test_MakeItem('ice cube', [stats.TemperatureModifier(stats.TemperatureModifier.Value.COLD)])
  Test_Pickup(a, ice)
  Test_ActorTakesDamage(a, 5)

  coffee = Test_MakeItem('coffee', [stats.TemperatureModifier(stats.TemperatureModifier.Value.HOT, 'steamy')])
  Test_Pickup(a, coffee)
  Test_ActorTakesDamage(a, 1)
  coffee2 = Test_MakeItem('coffee', [stats.TemperatureModifier(stats.TemperatureModifier.Value.HOT, 'steamy')])
  Test_Pickup(a, coffee2)
  Test_ActorTakesDamage(a, 1)

  hand_lotion = Test_MakeItem('hand lotion', [RegenModifier('HP', 1)])
  Test_Pickup(a, hand_lotion)
  OnAction(a, REGEN)
  print(a)