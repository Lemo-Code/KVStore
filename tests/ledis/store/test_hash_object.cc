/**
 * @file test_hash.cc
 * @brief Phase 5.1：LedisObject Hash 类型
 */
#include "../test_common.h"

#include "ledis/store/keyspace.h"
#include "ledis/store/object.h"

namespace {

void test_hash_object_ops() {
  ledis::LedisObject obj = ledis::LedisObject::makeHash();
  LEDIS_CHECK(obj.isHash());
  LEDIS_CHECK(!obj.isString());
  LEDIS_CHECK(obj.hashLen() == 0);

  LEDIS_CHECK(obj.hashSet(ledis::Sds("f1"), ledis::Sds("v1")));
  LEDIS_CHECK(obj.hashLen() == 1);
  LEDIS_CHECK(!obj.hashSet(ledis::Sds("f1"), ledis::Sds("v1-updated")));

  ledis::Sds value;
  LEDIS_CHECK(obj.hashGet(ledis::Sds("f1"), &value));
  LEDIS_CHECK(value.str() == "v1-updated");
  LEDIS_CHECK(!obj.hashGet(ledis::Sds("missing"), &value));

  LEDIS_CHECK(obj.hashSet(ledis::Sds("f2"), ledis::Sds("v2")));
  LEDIS_CHECK(obj.hashLen() == 2);
  LEDIS_CHECK(obj.hashDel(ledis::Sds("f1")) == 1);
  LEDIS_CHECK(obj.hashLen() == 1);
}

void test_hash_in_keyspace() {
  ledis::Keyspace ks;
  ledis::LedisObject obj = ledis::LedisObject::makeHash();
  obj.hashSet(ledis::Sds("name"), ledis::Sds("ledis"));
  LEDIS_CHECK(ks.set(ledis::Sds("meta"), std::move(obj)));

  ledis::LedisObject loaded;
  LEDIS_CHECK(ks.get(ledis::Sds("meta"), &loaded));
  LEDIS_CHECK(loaded.isHash());
  LEDIS_CHECK(loaded.hashLen() == 1);

  ledis::Sds name;
  LEDIS_CHECK(loaded.hashGet(ledis::Sds("name"), &name));
  LEDIS_CHECK(name.str() == "ledis");
}

void test_string_wrong_as_hash() {
  ledis::LedisObject str = ledis::LedisObject::makeString(ledis::Sds("hello"));
  LEDIS_CHECK(str.isString());
  LEDIS_CHECK(!str.isHash());
  LEDIS_CHECK(str.asHash() == nullptr);
  LEDIS_CHECK(str.hashLen() == 0);
}

}  // namespace

int main() {
  test_hash_object_ops();
  test_hash_in_keyspace();
  test_string_wrong_as_hash();
  return 0;
}
