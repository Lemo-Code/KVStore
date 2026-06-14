#pragma once

#include "ledis/containers.h"

namespace ledis {

enum class ObjectType { kString, kHash, kList, kSet, kZset };
enum class ObjectEncoding { kRaw, kHashMap, kListDeque, kSetHash, kZsetSkipList };

class LedisObject {
 public:
  LedisObject() = default;
  LedisObject(const LedisObject& other);
  LedisObject& operator=(const LedisObject& other);
  LedisObject(LedisObject&& other) noexcept = default;
  LedisObject& operator=(LedisObject&& other) noexcept = default;

  static LedisObject makeString(Sds data);
  static LedisObject makeHash();
  static LedisObject makeList();
  static LedisObject makeSet();
  static LedisObject makeZset();

  ObjectType type() const { return type_; }
  ObjectEncoding encoding() const { return encoding_; }
  bool isString() const { return type_ == ObjectType::kString; }
  bool isHash() const { return type_ == ObjectType::kHash; }
  bool isList() const { return type_ == ObjectType::kList; }
  bool isSet() const { return type_ == ObjectType::kSet; }
  bool isZset() const { return type_ == ObjectType::kZset; }

  bool asString(Sds* out) const;
  HashDict* asHash();
  const HashDict* asHash() const;
  ListDeque* asList();
  const ListDeque* asList() const;
  SdsSet* asSet();
  const SdsSet* asSet() const;
  ZsetDict* asZset();
  const ZsetDict* asZset() const;

  size_t hashLen() const;
  bool hashGet(const Sds& field, Sds* out) const;
  bool hashExists(const Sds& field) const;
  /** @return true 表示新增 field，false 表示覆盖已有 field。 */
  bool hashSet(const Sds& field, Sds value);
  size_t hashDel(const Sds& field);
  /** field 不存在视为 0；非整数字段返回 false。 */
  bool hashIncrBy(const Sds& field, int64_t delta, int64_t* new_value);

  size_t listLen() const;
  void listPushFront(Sds value);
  void listPushBack(Sds value);
  bool listPopFront(Sds* out);
  bool listPopBack(Sds* out);
  bool listIndex(int64_t index, Sds* out) const;
  void listTrim(int64_t start, int64_t stop);
  void listRange(int64_t start, int64_t stop, Vector<Sds>* out) const;

  size_t setLen() const;
  /** @return true 表示新增 member。 */
  bool setAdd(const Sds& member);
  size_t setRem(const Sds& member);
  bool setIsMember(const Sds& member) const;
  void setMembers(Vector<Sds>* out) const;
  /** nullptr 或空集按 Redis 语义视为空集。 */
  static void setIntersect(const Vector<const SdsSet*>& sets, Vector<Sds>* out);
  static void setUnion(const Vector<const SdsSet*>& sets, Vector<Sds>* out);
  static void setDiff(const SdsSet* first, const Vector<const SdsSet*>& others,
                      Vector<Sds>* out);

  size_t zsetLen() const;
  /** @return true 表示新增 member，false 表示更新已有 member 的 score。 */
  bool zsetAdd(const Sds& member, double score);
  /** member 不存在时 increment 即为新 score。 */
  double zsetIncrBy(const Sds& member, double increment);
  size_t zsetRem(const Sds& member);
  size_t zsetCountByScore(double min_score, bool min_exclusive, double max_score,
                          bool max_exclusive) const;
  bool zsetScore(const Sds& member, double* score) const;
  /** -1 表示 member 不存在。 */
  int64_t zsetRank(const Sds& member) const;
  int64_t zsetRevRank(const Sds& member) const;
  void zsetRangeByRank(int64_t start, int64_t stop, Vector<Sds>* members,
                       Vector<double>* scores) const;
  void zsetRevRangeByRank(int64_t start, int64_t stop, Vector<Sds>* members,
                          Vector<double>* scores) const;
  /** limit < 0 表示不限制条数。 */
  void zsetRangeByScore(double min_score, bool min_exclusive, double max_score,
                       bool max_exclusive, int64_t offset, int64_t limit,
                       Vector<Sds>* members, Vector<double>* scores) const;
  void zsetRevRangeByScore(double min_score, bool min_exclusive, double max_score,
                          bool max_exclusive, int64_t offset, int64_t limit,
                          Vector<Sds>* members, Vector<double>* scores) const;

 private:
  ObjectType type_ = ObjectType::kString;
  ObjectEncoding encoding_ = ObjectEncoding::kRaw;
  Sds data_;
  UniquePtr<HashDict> hash_;
  UniquePtr<ListDeque> list_;
  UniquePtr<SdsSet> set_;
  UniquePtr<ZsetDict> zset_;
};

}  // namespace ledis
