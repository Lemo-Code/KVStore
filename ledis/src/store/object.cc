#include "ledis/store/object.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <string>

namespace ledis {
namespace {

void copyDeque(const ListDeque& src, ListDeque* dst) {
  for (size_t i = 0; i < src.size(); ++i) {
    dst->push_back(src[i]);
  }
}

void copySet(const SdsSet& src, SdsSet* dst) {
  for (SdsSet::const_iterator it = src.begin(); it != src.end(); ++it) {
    dst->insert(*it);
  }
}

void copyZset(const ZsetDict& src, ZsetDict* dst) {
  for (ZsetDict::const_iterator it = src.begin(); it != src.end(); ++it) {
    dst->insert(ZsetDict::value_type(it->first, it->second));
  }
}

struct ZsetEntry {
  Sds member;
  double score = 0;
};

struct ZsetEntryLess {
  bool operator()(const ZsetEntry& lhs, const ZsetEntry& rhs) const {
    if (lhs.score != rhs.score) {
      return lhs.score < rhs.score;
    }
    return lhs.member.str() < rhs.member.str();
  }
};

void collectSortedZset(const ZsetDict& zset, Vector<ZsetEntry>* out) {
  if (!out) {
    return;
  }
  out->clear();
  for (ZsetDict::const_iterator it = zset.begin(); it != zset.end(); ++it) {
    ZsetEntry entry;
    entry.member = it->first;
    entry.score = it->second;
    out->push_back(Move(entry));
  }
  std::sort(out->begin(), out->end(), ZsetEntryLess());
}

}  // namespace

LedisObject::LedisObject(const LedisObject& other)
    : type_(other.type_), encoding_(other.encoding_), data_(other.data_) {
  if (other.hash_) {
    hash_.reset(new HashDict());
    for (HashDict::const_iterator it = other.hash_->begin(); it != other.hash_->end(); ++it) {
      hash_->insert(HashDict::value_type(it->first, it->second));
    }
  }
  if (other.list_) {
    list_.reset(new ListDeque());
    copyDeque(*other.list_, list_.get());
  }
  if (other.set_) {
    set_.reset(new SdsSet());
    copySet(*other.set_, set_.get());
  }
  if (other.zset_) {
    zset_.reset(new ZsetDict());
    copyZset(*other.zset_, zset_.get());
  }
}

LedisObject& LedisObject::operator=(const LedisObject& other) {
  if (this == &other) {
    return *this;
  }
  type_ = other.type_;
  encoding_ = other.encoding_;
  data_ = other.data_;
  if (other.hash_) {
    HashDict next;
    for (HashDict::const_iterator it = other.hash_->begin(); it != other.hash_->end(); ++it) {
      next.insert(HashDict::value_type(it->first, it->second));
    }
    hash_.reset(new HashDict(Move(next)));
  } else {
    hash_.reset();
  }
  if (other.list_) {
    ListDeque next;
    copyDeque(*other.list_, &next);
    list_.reset(new ListDeque(Move(next)));
  } else {
    list_.reset();
  }
  if (other.set_) {
    SdsSet next;
    copySet(*other.set_, &next);
    set_.reset(new SdsSet(Move(next)));
  } else {
    set_.reset();
  }
  if (other.zset_) {
    ZsetDict next;
    copyZset(*other.zset_, &next);
    zset_.reset(new ZsetDict(Move(next)));
  } else {
    zset_.reset();
  }
  return *this;
}

LedisObject LedisObject::makeString(Sds data) {
  LedisObject obj;
  obj.type_ = ObjectType::kString;
  obj.encoding_ = ObjectEncoding::kRaw;
  obj.data_ = Move(data);
  obj.hash_.reset();
  obj.list_.reset();
  obj.set_.reset();
  obj.zset_.reset();
  return obj;
}

LedisObject LedisObject::makeHash() {
  LedisObject obj;
  obj.type_ = ObjectType::kHash;
  obj.encoding_ = ObjectEncoding::kHashMap;
  obj.data_ = Sds();
  obj.hash_.reset(new HashDict());
  obj.list_.reset();
  obj.set_.reset();
  obj.zset_.reset();
  return obj;
}

LedisObject LedisObject::makeList() {
  LedisObject obj;
  obj.type_ = ObjectType::kList;
  obj.encoding_ = ObjectEncoding::kListDeque;
  obj.data_ = Sds();
  obj.hash_.reset();
  obj.list_.reset(new ListDeque());
  obj.set_.reset();
  obj.zset_.reset();
  return obj;
}

LedisObject LedisObject::makeSet() {
  LedisObject obj;
  obj.type_ = ObjectType::kSet;
  obj.encoding_ = ObjectEncoding::kSetHash;
  obj.data_ = Sds();
  obj.hash_.reset();
  obj.list_.reset();
  obj.set_.reset(new SdsSet());
  obj.zset_.reset();
  return obj;
}

LedisObject LedisObject::makeZset() {
  LedisObject obj;
  obj.type_ = ObjectType::kZset;
  obj.encoding_ = ObjectEncoding::kZsetSkipList;
  obj.data_ = Sds();
  obj.hash_.reset();
  obj.list_.reset();
  obj.set_.reset();
  obj.zset_.reset(new ZsetDict());
  return obj;
}

bool LedisObject::asString(Sds* out) const {
  if (!out || type_ != ObjectType::kString) {
    return false;
  }
  *out = data_;
  return true;
}

HashDict* LedisObject::asHash() {
  if (type_ != ObjectType::kHash || !hash_) {
    return nullptr;
  }
  return hash_.get();
}

const HashDict* LedisObject::asHash() const {
  if (type_ != ObjectType::kHash || !hash_) {
    return nullptr;
  }
  return hash_.get();
}

ListDeque* LedisObject::asList() {
  if (type_ != ObjectType::kList || !list_) {
    return nullptr;
  }
  return list_.get();
}

const ListDeque* LedisObject::asList() const {
  if (type_ != ObjectType::kList || !list_) {
    return nullptr;
  }
  return list_.get();
}

size_t LedisObject::hashLen() const {
  return hash_ ? hash_->size() : 0;
}

bool LedisObject::hashGet(const Sds& field, Sds* out) const {
  if (!out || !hash_) {
    return false;
  }
  const HashDict::const_iterator it = hash_->find(field);
  if (it == hash_->end()) {
    return false;
  }
  *out = it->second;
  return true;
}

bool LedisObject::hashSet(const Sds& field, Sds value) {
  if (type_ != ObjectType::kHash || !hash_) {
    return false;
  }
  const HashDict::iterator it = hash_->find(field);
  const bool is_new = it == hash_->end();
  (*hash_)[field] = Move(value);
  return is_new;
}

size_t LedisObject::hashDel(const Sds& field) {
  if (!hash_) {
    return 0;
  }
  return hash_->erase(field);
}

bool LedisObject::hashExists(const Sds& field) const {
  if (!hash_) {
    return false;
  }
  return hash_->find(field) != hash_->end();
}

bool LedisObject::hashIncrBy(const Sds& field, int64_t delta, int64_t* new_value) {
  if (!new_value || type_ != ObjectType::kHash || !hash_) {
    return false;
  }
  int64_t current = 0;
  const HashDict::iterator it = hash_->find(field);
  if (it != hash_->end()) {
    char* end = nullptr;
    const long long v = std::strtoll(it->second.data(), &end, 10);
    if (end != it->second.data() + static_cast<ptrdiff_t>(it->second.size())) {
      return false;
    }
    current = static_cast<int64_t>(v);
  }
  *new_value = current + delta;
  (*hash_)[field] = Sds(std::to_string(*new_value));
  return true;
}

size_t LedisObject::listLen() const {
  return list_ ? list_->size() : 0;
}

void LedisObject::listPushFront(Sds value) {
  if (type_ != ObjectType::kList || !list_) {
    return;
  }
  list_->push_front(Move(value));
}

void LedisObject::listPushBack(Sds value) {
  if (type_ != ObjectType::kList || !list_) {
    return;
  }
  list_->push_back(Move(value));
}

bool LedisObject::listPopFront(Sds* out) {
  if (!out || !list_ || list_->empty()) {
    return false;
  }
  *out = list_->front();
  list_->pop_front();
  return true;
}

bool LedisObject::listPopBack(Sds* out) {
  if (!out || !list_ || list_->empty()) {
    return false;
  }
  *out = list_->back();
  list_->pop_back();
  return true;
}

void LedisObject::listRange(int64_t start, int64_t stop, Vector<Sds>* out) const {
  if (!out || !list_ || list_->empty()) {
    return;
  }
  const size_t len = list_->size();
  if (start < 0) {
    start = static_cast<int64_t>(len) + start;
  }
  if (stop < 0) {
    stop = static_cast<int64_t>(len) + stop;
  }
  if (start < 0) {
    start = 0;
  }
  if (stop >= static_cast<int64_t>(len)) {
    stop = static_cast<int64_t>(len) - 1;
  }
  if (start > stop) {
    return;
  }
  for (int64_t i = start; i <= stop; ++i) {
    out->push_back((*list_)[static_cast<size_t>(i)]);
  }
}

bool LedisObject::listIndex(int64_t index, Sds* out) const {
  if (!out || !list_ || list_->empty()) {
    return false;
  }
  const size_t len = list_->size();
  if (index < 0) {
    index = static_cast<int64_t>(len) + index;
  }
  if (index < 0 || index >= static_cast<int64_t>(len)) {
    return false;
  }
  *out = (*list_)[static_cast<size_t>(index)];
  return true;
}

void LedisObject::listTrim(int64_t start, int64_t stop) {
  if (!list_) {
    return;
  }
  Vector<Sds> kept;
  listRange(start, stop, &kept);
  list_->clear();
  for (size_t i = 0; i < kept.size(); ++i) {
    list_->push_back(Move(kept[i]));
  }
}

SdsSet* LedisObject::asSet() {
  if (type_ != ObjectType::kSet || !set_) {
    return nullptr;
  }
  return set_.get();
}

const SdsSet* LedisObject::asSet() const {
  if (type_ != ObjectType::kSet || !set_) {
    return nullptr;
  }
  return set_.get();
}

size_t LedisObject::setLen() const {
  return set_ ? set_->size() : 0;
}

bool LedisObject::setAdd(const Sds& member) {
  if (type_ != ObjectType::kSet || !set_) {
    return false;
  }
  return set_->insert(member).second;
}

size_t LedisObject::setRem(const Sds& member) {
  if (!set_) {
    return 0;
  }
  return set_->erase(member);
}

bool LedisObject::setIsMember(const Sds& member) const {
  if (!set_) {
    return false;
  }
  return set_->find(member) != set_->end();
}

void LedisObject::setMembers(Vector<Sds>* out) const {
  if (!out || !set_) {
    return;
  }
  for (SdsSet::const_iterator it = set_->begin(); it != set_->end(); ++it) {
    out->push_back(*it);
  }
}

void LedisObject::setIntersect(const Vector<const SdsSet*>& sets, Vector<Sds>* out) {
  if (!out) {
    return;
  }
  out->clear();
  if (sets.empty()) {
    return;
  }
  for (size_t i = 0; i < sets.size(); ++i) {
    if (!sets[i] || sets[i]->empty()) {
      return;
    }
  }
  size_t base_idx = 0;
  for (size_t i = 1; i < sets.size(); ++i) {
    if (sets[i]->size() < sets[base_idx]->size()) {
      base_idx = i;
    }
  }
  const SdsSet* base = sets[base_idx];
  for (SdsSet::const_iterator it = base->begin(); it != base->end(); ++it) {
    bool ok = true;
    for (size_t i = 0; i < sets.size(); ++i) {
      if (i == base_idx) {
        continue;
      }
      if (sets[i]->find(*it) == sets[i]->end()) {
        ok = false;
        break;
      }
    }
    if (ok) {
      out->push_back(*it);
    }
  }
}

void LedisObject::setUnion(const Vector<const SdsSet*>& sets, Vector<Sds>* out) {
  if (!out) {
    return;
  }
  out->clear();
  SdsSet merged;
  for (size_t i = 0; i < sets.size(); ++i) {
    if (!sets[i]) {
      continue;
    }
    for (SdsSet::const_iterator it = sets[i]->begin(); it != sets[i]->end(); ++it) {
      merged.insert(*it);
    }
  }
  for (SdsSet::const_iterator it = merged.begin(); it != merged.end(); ++it) {
    out->push_back(*it);
  }
}

void LedisObject::setDiff(const SdsSet* first, const Vector<const SdsSet*>& others,
                          Vector<Sds>* out) {
  if (!out) {
    return;
  }
  out->clear();
  if (!first || first->empty()) {
    return;
  }
  SdsSet remove;
  for (size_t i = 0; i < others.size(); ++i) {
    if (!others[i]) {
      continue;
    }
    for (SdsSet::const_iterator it = others[i]->begin(); it != others[i]->end(); ++it) {
      remove.insert(*it);
    }
  }
  for (SdsSet::const_iterator it = first->begin(); it != first->end(); ++it) {
    if (remove.find(*it) == remove.end()) {
      out->push_back(*it);
    }
  }
}

ZsetDict* LedisObject::asZset() {
  if (type_ != ObjectType::kZset || !zset_) {
    return nullptr;
  }
  return zset_.get();
}

const ZsetDict* LedisObject::asZset() const {
  if (type_ != ObjectType::kZset || !zset_) {
    return nullptr;
  }
  return zset_.get();
}

size_t LedisObject::zsetLen() const {
  return zset_ ? zset_->size() : 0;
}

bool LedisObject::zsetAdd(const Sds& member, double score) {
  if (type_ != ObjectType::kZset || !zset_) {
    return false;
  }
  const ZsetDict::iterator it = zset_->find(member);
  const bool is_new = it == zset_->end();
  (*zset_)[member] = score;
  return is_new;
}

double LedisObject::zsetIncrBy(const Sds& member, double increment) {
  double score = increment;
  if (zset_) {
    const ZsetDict::const_iterator it = zset_->find(member);
    if (it != zset_->end()) {
      score = it->second + increment;
    }
    (*zset_)[member] = score;
  }
  return score;
}

size_t LedisObject::zsetCountByScore(double min_score, bool min_exclusive,
                                     double max_score, bool max_exclusive) const {
  if (!zset_) {
    return 0;
  }
  Vector<ZsetEntry> entries;
  collectSortedZset(*zset_, &entries);
  size_t count = 0;
  for (size_t i = 0; i < entries.size(); ++i) {
    const double score = entries[i].score;
    if (min_exclusive) {
      if (score <= min_score) {
        continue;
      }
    } else if (score < min_score) {
      continue;
    }
    if (max_exclusive) {
      if (score >= max_score) {
        continue;
      }
    } else if (score > max_score) {
      continue;
    }
    ++count;
  }
  return count;
}

size_t LedisObject::zsetRem(const Sds& member) {
  if (!zset_) {
    return 0;
  }
  return zset_->erase(member);
}

bool LedisObject::zsetScore(const Sds& member, double* score) const {
  if (!score || !zset_) {
    return false;
  }
  const ZsetDict::const_iterator it = zset_->find(member);
  if (it == zset_->end()) {
    return false;
  }
  *score = it->second;
  return true;
}

int64_t LedisObject::zsetRank(const Sds& member) const {
  if (!zset_) {
    return -1;
  }
  Vector<ZsetEntry> entries;
  collectSortedZset(*zset_, &entries);
  for (size_t i = 0; i < entries.size(); ++i) {
    if (entries[i].member == member) {
      return static_cast<int64_t>(i);
    }
  }
  return -1;
}

int64_t LedisObject::zsetRevRank(const Sds& member) const {
  const int64_t rank = zsetRank(member);
  if (rank < 0) {
    return -1;
  }
  return static_cast<int64_t>(zsetLen()) - 1 - rank;
}

void LedisObject::zsetRangeByRank(int64_t start, int64_t stop,
                                    Vector<Sds>* members,
                                    Vector<double>* scores) const {
  if (!members || !zset_) {
    return;
  }
  members->clear();
  if (scores) {
    scores->clear();
  }
  Vector<ZsetEntry> entries;
  collectSortedZset(*zset_, &entries);
  if (entries.empty()) {
    return;
  }
  const int64_t len = static_cast<int64_t>(entries.size());
  if (start < 0) {
    start = len + start;
  }
  if (stop < 0) {
    stop = len + stop;
  }
  if (start < 0) {
    start = 0;
  }
  if (stop >= len) {
    stop = len - 1;
  }
  if (start > stop) {
    return;
  }
  for (int64_t i = start; i <= stop; ++i) {
    members->push_back(entries[static_cast<size_t>(i)].member);
    if (scores) {
      scores->push_back(entries[static_cast<size_t>(i)].score);
    }
  }
}

void LedisObject::zsetRevRangeByRank(int64_t start, int64_t stop,
                                     Vector<Sds>* members,
                                     Vector<double>* scores) const {
  if (!members || !zset_) {
    return;
  }
  Vector<ZsetEntry> entries;
  collectSortedZset(*zset_, &entries);
  if (entries.empty()) {
    members->clear();
    if (scores) {
      scores->clear();
    }
    return;
  }
  const int64_t len = static_cast<int64_t>(entries.size());
  if (start < 0) {
    start = len + start;
  }
  if (stop < 0) {
    stop = len + stop;
  }
  if (start < 0) {
    start = 0;
  }
  if (stop >= len) {
    stop = len - 1;
  }
  if (start > stop) {
    members->clear();
    if (scores) {
      scores->clear();
    }
    return;
  }
  members->clear();
  if (scores) {
    scores->clear();
  }
  for (int64_t rev_i = start; rev_i <= stop; ++rev_i) {
    const size_t idx = static_cast<size_t>(len - 1 - rev_i);
    members->push_back(entries[idx].member);
    if (scores) {
      scores->push_back(entries[idx].score);
    }
  }
}

void LedisObject::zsetRangeByScore(double min_score, bool min_exclusive,
                                   double max_score, bool max_exclusive,
                                   int64_t offset, int64_t limit,
                                   Vector<Sds>* members,
                                   Vector<double>* scores) const {
  if (!members || !zset_) {
    return;
  }
  members->clear();
  if (scores) {
    scores->clear();
  }
  Vector<ZsetEntry> entries;
  collectSortedZset(*zset_, &entries);
  Vector<ZsetEntry> matched;
  for (size_t i = 0; i < entries.size(); ++i) {
    const double score = entries[i].score;
    if (min_exclusive) {
      if (score <= min_score) {
        continue;
      }
    } else if (score < min_score) {
      continue;
    }
    if (max_exclusive) {
      if (score >= max_score) {
        continue;
      }
    } else if (score > max_score) {
      continue;
    }
    matched.push_back(entries[i]);
  }
  if (offset < 0) {
    offset = 0;
  }
  if (offset >= static_cast<int64_t>(matched.size())) {
    return;
  }
  size_t begin = static_cast<size_t>(offset);
  size_t end = matched.size();
  if (limit >= 0 && begin + static_cast<size_t>(limit) < end) {
    end = begin + static_cast<size_t>(limit);
  }
  for (size_t i = begin; i < end; ++i) {
    members->push_back(matched[i].member);
    if (scores) {
      scores->push_back(matched[i].score);
    }
  }
}

void LedisObject::zsetRevRangeByScore(double min_score, bool min_exclusive,
                                      double max_score, bool max_exclusive,
                                      int64_t offset, int64_t limit,
                                      Vector<Sds>* members,
                                      Vector<double>* scores) const {
  if (!members || !zset_) {
    return;
  }
  members->clear();
  if (scores) {
    scores->clear();
  }
  Vector<ZsetEntry> entries;
  collectSortedZset(*zset_, &entries);
  Vector<ZsetEntry> matched;
  for (size_t i = 0; i < entries.size(); ++i) {
    const double score = entries[i].score;
    if (min_exclusive) {
      if (score <= min_score) {
        continue;
      }
    } else if (score < min_score) {
      continue;
    }
    if (max_exclusive) {
      if (score >= max_score) {
        continue;
      }
    } else if (score > max_score) {
      continue;
    }
    matched.push_back(entries[i]);
  }
  std::reverse(matched.begin(), matched.end());
  if (offset < 0) {
    offset = 0;
  }
  if (offset >= static_cast<int64_t>(matched.size())) {
    return;
  }
  size_t begin = static_cast<size_t>(offset);
  size_t end = matched.size();
  if (limit >= 0 && begin + static_cast<size_t>(limit) < end) {
    end = begin + static_cast<size_t>(limit);
  }
  for (size_t i = begin; i < end; ++i) {
    members->push_back(matched[i].member);
    if (scores) {
      scores->push_back(matched[i].score);
    }
  }
}

}  // namespace ledis
