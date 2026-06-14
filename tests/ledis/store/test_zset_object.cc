/**
 * @file test_zset_object.cc
 * @brief Phase 5.4：LedisObject Zset 类型
 */
#include "../test_common.h"

#include "ledis/store/keyspace.h"
#include "ledis/store/object.h"

namespace {

void test_zset_object_ops() {
  ledis::LedisObject obj = ledis::LedisObject::makeZset();
  LEDIS_CHECK(obj.isZset());
  LEDIS_CHECK(!obj.isString());
  LEDIS_CHECK(obj.zsetLen() == 0);

  LEDIS_CHECK(obj.zsetAdd(ledis::Sds("a"), 1.0));
  LEDIS_CHECK(obj.zsetAdd(ledis::Sds("b"), 2.0));
  LEDIS_CHECK(!obj.zsetAdd(ledis::Sds("a"), 3.0));
  LEDIS_CHECK(obj.zsetLen() == 2);

  double score = 0;
  LEDIS_CHECK(obj.zsetScore(ledis::Sds("a"), &score));
  LEDIS_CHECK(score == 3.0);
  LEDIS_CHECK(!obj.zsetScore(ledis::Sds("missing"), &score));

  LEDIS_CHECK(obj.zsetRank(ledis::Sds("b")) == 0);
  LEDIS_CHECK(obj.zsetRank(ledis::Sds("a")) == 1);
  LEDIS_CHECK(obj.zsetRank(ledis::Sds("missing")) == -1);

  ledis::Vector<ledis::Sds> members;
  ledis::Vector<double> scores;
  obj.zsetRangeByRank(0, -1, &members, &scores);
  LEDIS_CHECK(members.size() == 2);
  LEDIS_CHECK(members[0].str() == "b");
  LEDIS_CHECK(members[1].str() == "a");
  LEDIS_CHECK(scores[0] == 2.0);
  LEDIS_CHECK(scores[1] == 3.0);

  LEDIS_CHECK(obj.zsetRem(ledis::Sds("a")) == 1);
  LEDIS_CHECK(obj.zsetLen() == 1);
  LEDIS_CHECK(obj.zsetRem(ledis::Sds("missing")) == 0);
}

void test_zset_in_keyspace() {
  ledis::Keyspace ks;
  ledis::LedisObject obj = ledis::LedisObject::makeZset();
  obj.zsetAdd(ledis::Sds("m1"), 10.0);
  obj.zsetAdd(ledis::Sds("m2"), 20.0);
  LEDIS_CHECK(ks.set(ledis::Sds("rank"), Move(obj)));

  ledis::LedisObject loaded;
  LEDIS_CHECK(ks.get(ledis::Sds("rank"), &loaded));
  LEDIS_CHECK(loaded.isZset());
  LEDIS_CHECK(loaded.zsetLen() == 2);
  double score = 0;
  LEDIS_CHECK(loaded.zsetScore(ledis::Sds("m2"), &score));
  LEDIS_CHECK(score == 20.0);
}

void test_zset_copy() {
  ledis::LedisObject src = ledis::LedisObject::makeZset();
  src.zsetAdd(ledis::Sds("x"), 1.5);
  const ledis::LedisObject copy = src;
  LEDIS_CHECK(copy.isZset());
  LEDIS_CHECK(copy.zsetLen() == 1);
  double score = 0;
  LEDIS_CHECK(copy.zsetScore(ledis::Sds("x"), &score));
  LEDIS_CHECK(score == 1.5);
}

}  // namespace

int main() {
  test_zset_object_ops();
  test_zset_in_keyspace();
  test_zset_copy();
  return 0;
}
