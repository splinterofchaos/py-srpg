import copy
from enum import Enum, auto

# A stat can be integral or fractional; subclasses differentiate the two.
class Stat:
  def __init__(self, name, reason):
    self.name = name
    self.reason = reason
    self.actions = {}

  def EffectRepr(self): pass
  def __repr__(self):
    effect = self.EffectRepr()
    if self.reason:
      return f'{self.name}: {effect} ({self.reason})'
    else:
      return effect

  def HasAction(self, action_id): return action_id in self.actions

  def OnAction(self, action_id, *args, **kwargs):
    return self.actions[action_id](*args, **kwargs)

  def Merge(self, other): pass

  # WARNING: some stats might be written such that a deep copy is or isn't
  # safe. If there is any concern, make sure to override this!
  def Copy(self): return copy.deepcopy(self)

# Flat stats like STR or HIT.
class IntegerStat(Stat):
  def __init__(self, name, value=0, multiplier=1, reason=None):
    super(IntegerStat, self).__init__(name, reason)
    self.value = value
    self.multiplier = multiplier

  def EffectRepr(self):
    if self.multiplier:
      # TODO: convert multiplier to integer out of 100 first
      return (f'{int(self.multiplier*100)}%')
    else:
      return f'{self.value}'

  def Merge(self, other):
    self.value += other.value
    self.multiplier += other.multiplier

# Stats like HP, MP, etc..
CONSUME = 'consume'
REFUND = 'refund'
class FractionStat(Stat):
  def __init__(self, name, numerator, denominator, reason=None):
    super(FractionStat, self).__init__(name, reason)
    self.n = numerator
    self.d = denominator

    def Consume(stat, acc=None):
      if stat != self.name: return acc
      left = max(0, self.n - acc)
      acc -= min(self.n, acc)
      self.n = left
      return acc
    self.actions[CONSUME] = Consume

    def Refund(stat, acc=None):
      if stat != self.name: return acc
      have = min(self.d, self.n + acc)
      acc -= min(acc, self.d - self.n)
      self.n = have
      return acc
    self.actions[REFUND] = Refund

  def EffectRepr(self): return f'{self.n}/{self.d}'

  def Merge(self, other):
    self.n += other.n
    self.d += other.d


# A collection of stats and attributes. The sum of modifiers on an item or
# actor.
class StatSheet:
  def __init__(self):
    self.stats = {}
    self.attributes = []

  def __repr__(self):
    return '\n'.join(map(str, list(self.stats.values()) + self.attributes))

  def AddStat(self, stat):
    if stat.name in self.stats:
      self.stats[stat.name].Merge(stat)
    else:
      self.stats[stat.name] = stat.Copy()

  def Merge(self, other_sheet):
      for s in other_sheet.stats.values(): self.AddStat(s)

# This class is currently unused and may differ from the current stat API.

#MODIFY_INCOMING_DAMAGE = 'modify incomming damage'
#class TemperatureModifier(Modifier):
#  class Value(Enum):
#    COLD = -10
#    LUKWARM = 0
#    HOT = 10
#  
#  def __init__(self, value, reason=None):
#    # We pass the value as the name. Both are the same.
#    super(TemperatureModifier, self).__init__(reason)
#    self.value = value
#    
#    def ModifyIncomingDamage(acc=None):
#      if self.value == TemperatureModifier.Value.COLD:
#        acc /= 2
#      elif self.value == TemperatureModifier.Value.HOT:
#        acc *= 2
#      return acc
#    self.actions[MODIFY_INCOMING_DAMAGE] = ModifyIncomingDamage
#
#  # When we want to copy ourselves, but forget the reason (such as when copying
#  # into a stat sheet.
#  def CopyNoReason(self):
#    ret = copy.deepcopy(self)
#    ret.reason = None
#    return ret
#
#  def EffectRepr(self): return self.value.name
#
#  # Hot and cold mix to become lukwarm.
#  # Hot and hot is just hot and ditto for cold.
#  def ModifyStatSheet(self, sheet):
#    V = TemperatureModifier.Value
#    for i, a in enumerate(sheet.attributes):
#      if type(a) == TemperatureModifier:
#        if self.value == V.LUKWARM or a == self:
#          pass
#        if a.value == V.LUKWARM:
#          sheet.attributes[i] = self.CopyNoReason()
#        else:
#          # By process of elimination, we are hot, they are cold, or the other
#          # way around. The result is the same either way.
#          sheet.attributes[i] = TemperatureModifier(V.LUKWARM)
#        return
#
#    sheet.attributes.append(self.CopyNoReason())
