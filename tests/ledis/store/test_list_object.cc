/**
 * @file test_list_object.cc
 * @brief Phase 5.2：LedisObject List 类型
 */
#include "../test_common.h"

#include "ledis/store/keyspace.h"
#include "ledis/store/object.h"

namespace {

void test_list_object_ops() {
  ledis::LedisObject obj = ledis::LedisObject::makeList();
  LEDIS_CHECK(obj.isList());
  LEDIS_CHECK(!obj.isString());
  LEDIS_CHECK(!obj.isHash());
  LEDIS_CHECK(obj.listLen() == 0);

  obj.listPushBack(ledis::Sds("a"));
  obj.listPushBack(ledis::Sds("b"));
  obj.listPushFront(ledis::Sds("head"));
  LEDIS_CHECK(obj.listLen() == 3);

  ledis::Vector<ledis::Sds> range;
  obj.listRange(0, -1, &range);
  LEDIS_CHECK(range.size() == 3);
  LEDIS_CHECK(range[0].str() == "head");
  LEDIS_CHECK(range[1].str() == "a");
  LEDIS_CHECK(range[2].str() == "b");

  ledis::Sds value;
  LEDIS_CHECK(obj.listPopFront(&value));
  LEDIS_CHECK(value.str() == "head");
  LEDIS_CHECK(obj.listPopBack(&value));
  LEDIS_CHECK(value.str() == "b");
  LEDIS_CHECK(obj.listLen() == 1);
}

void test_list_in_keyspace() {
  ledis::Keyspace ks;
  ledis::LedisObject obj = ledis::LedisObject::makeList();
  obj.listPushBack(ledis::Sds("x"));
  obj.listPushBack(ledis::Sds("y"));
  LEDIS_CHECK(ks.set(ledis::Sds("items"), std::move(obj)));

  ledis::LedisObject loaded;
  LEDIS_CHECK(ks.get(ledis::Sds("items"), &loaded));
  LEDIS_CHECK(loaded.isList());
  LEDIS_CHECK(loaded.listLen() == 2);
}

}  // namespace

int main() {
  test_list_object_ops();
  test_list_in_keyspace();
  return 0;
}
