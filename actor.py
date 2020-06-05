import entity


def Hp(have, total, reason=None):
  return entity.FractionalAdditionModifier('HP', have, total, reason)


# Any item that exists in this world.
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


# Any intelligent actor.
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
    sheet = entity.StatSheet()
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

def DealDamage(actor, amount):
  sheet = actor.Stats()
  for a in sheet.attributes:
    if a.HasAction(entity.MODIFY_INCOMING_DAMAGE):
      amount = a.OnAction(entity.MODIFY_INCOMING_DAMAGE, amount)
  amount = int(amount)

  for i in actor.inventory:
    for m in i.modifiers:
      if m.HasAction(entity.CONSUME):
        amount = m.OnAction(entity.CONSUME, 'HP', amount)


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

ice = Test_MakeItem('ice cube', [entity.TemperatureModifier(entity.TemperatureModifier.Value.COLD)])
Test_Pickup(a, ice)
Test_ActorTakesDamage(a, 5)

coffee = Test_MakeItem('coffee', [entity.TemperatureModifier(entity.TemperatureModifier.Value.HOT, 'steamy')])
Test_Pickup(a, coffee)
Test_ActorTakesDamage(a, 1)
Test_Pickup(a, coffee)
Test_ActorTakesDamage(a, 1)

hand_lotion = Test_MakeItem('hand lotion', [entity.RegenModifier('HP', 1)])
Test_Pickup(a, hand_lotion)
a.OnAction(entity.REGEN)
print(a)