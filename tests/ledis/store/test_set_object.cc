/**
 * @file test_set_object.cc
 * @brief Phase 5.3：LedisObject Set 类型
 */
#include "../test_common.h"

#include "ledis/store/keyspace.h"
#include "ledis/store/object.h"

namespace {

void test_set_object_ops() {
  ledis::LedisObject obj = ledis::LedisObject::makeSet();
  LEDIS_CHECK(obj.isSet());
  LEDIS_CHECK(!obj.isString());
  LEDIS_CHECK(obj.setLen() == 0);

  LEDIS_CHECK(obj.setAdd(ledis::Sds("a")));
  LEDIS_CHECK(obj.setAdd(ledis::Sds("b")));
  LEDIS_CHECK(!obj.setAdd(ledis::Sds("a")));
  LEDIS_CHECK(obj.setLen() == 2);
  LEDIS_CHECK(obj.setIsMember(ledis::Sds("a")));
  LEDIS_CHECK(!obj.setIsMember(ledis::Sds("missing")));

  LEDIS_CHECK(obj.setRem(ledis::Sds("a")) == 1);
  LEDIS_CHECK(obj.setLen() == 1);
  LEDIS_CHECK(obj.setRem(ledis::Sds("missing")) == 0);
}

void test_set_in_keyspace() {
  ledis::Keyspace ks;
  ledis::LedisObject obj = ledis::LedisObject::makeSet();
  obj.setAdd(ledis::Sds("x"));
  obj.setAdd(ledis::Sds("y"));
  LEDIS_CHECK(ks.set(ledis::Sds("tags"), Move(obj)));

  ledis::LedisObject loaded;
  LEDIS_CHECK(ks.get(ledis::Sds("tags"), &loaded));
  LEDIS_CHECK(loaded.isSet());
  LEDIS_CHECK(loaded.setLen() == 2);
  LEDIS_CHECK(loaded.setIsMember(ledis::Sds("x")));
}

}  // namespace

int main() {
  test_set_object_ops();
  test_set_in_keyspace();
  return 0;
}
