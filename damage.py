import stats

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