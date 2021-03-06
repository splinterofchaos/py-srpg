import stats

def DealDamage(actor, amount):
  sheet = actor.Stats()
  for a in sheet.attributes:
    if a.HasAction(stats.MODIFY_INCOMING_DAMAGE):
      amount = a.OnAction(stats.MODIFY_INCOMING_DAMAGE, amount)
  amount = int(amount)

  for i in actor.children:
    hp = i.stat_sheet.stats.get('HP')
    if hp: amount = hp.Consume(amount)


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

    remaining_hp = sheet.stats['HP'].n - amount
    DealDamage(defender, amount)
    return remaining_hp
