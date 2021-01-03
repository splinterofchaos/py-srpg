#include "../stats.h"
#include "test.h"

int main() {
  // === IntStat tests ===

  TEST(IntStat("foo", "bar", 1, 0).repr(), "foo: 1 (bar)");
  TEST(IntStat("foo", "", 1, 0).repr(), "foo: 1");
  TEST(IntStat("foo", "bar", 0, 0.5).repr(), "foo: 50% (bar)");
  TEST_WITH(IntStat one_one("foo", "", 1, 1);
            one_one.merge(IntStat("foo", "", 1, 1)),
            one_one.value(), 4u);

  // === MeterStat tests ===

  TEST(MeterStat("hp", "", 5, 5).repr(), "hp: 5/5");

  TEST_WITH(MeterStat hp("hp", "", 5, 5);
            hp.merge(MeterStat("hp", "", 5, 5)),
            hp.repr(), "hp: 10/10");
  TEST_WITH(MeterStat hp("hp", "", 5, 5);
            unsigned int damage = 4;
            hp.consume(damage),
            hp.repr(), "hp: 1/5");
  TEST_WITH(MeterStat hp("hp", "", 5, 5);
            unsigned int damage = 4;
            hp.consume(damage), damage, 0u);
  TEST_WITH(MeterStat hp("hp", "", 5, 5);
            unsigned int damage = 10;
            hp.consume(damage), damage, 5u);
  
  TEST_WITH(MeterStat hp("hp", "", 0, 5);
            unsigned int heal = 4;
            hp.refund(heal),
            hp.repr(), "hp: 4/5");
  TEST_WITH(MeterStat hp("hp", "", 0, 5u);
            unsigned int heal = 10;
            hp.refund(heal), heal, 5u);
}
