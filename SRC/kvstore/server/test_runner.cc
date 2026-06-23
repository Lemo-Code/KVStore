#include "kvstore/kvstore.h"
#include <iostream>
#include <cassert>
#include <chrono>
#include <vector>
#include <map>
#include <sstream>
using namespace zero::kvstore;
static int g_passed = 0, g_failed = 0;

#define TEST(name) void test_##name()
#define RUN(name) do { try { test_##name(); g_passed++; } catch(const std::exception& e) { g_failed++; std::cerr << "FAIL " #name ": " << e.what() << "\n"; } } while(0)
#define CHECK(cond) do { if(!(cond)) throw std::runtime_error(#cond); } while(0)
#define CHECK_EQ(a,b) do { if((a)!=(b)) { std::ostringstream o; o << (a) << " != " << (b); throw std::runtime_error(o.str()); } } while(0)
#define CHECK_OK(st) do { if(!(st).ok()) throw std::runtime_error((st).Msg()); } while(0)

TEST(status_ok) { Status s; CHECK(s.ok()); CHECK(Status::OK().ok()); }
TEST(status_not_found) { Status s = Status::NotFound("k"); CHECK(s.IsNotFound()); CHECK(!s.ok()); CHECK_EQ(s.Code(), kNotFound); }
TEST(status_not_leader) { Status s = Status::NotLeader("n2"); CHECK(s.IsNotLeader()); CHECK_EQ(s.RedirectNode(), "n2"); }
TEST(status_types) {
    CHECK(Status::Conflict("x").IsConflict()); CHECK(Status::IOError("d").IsIOError());
    CHECK(Status::Timeout("op").IsTimeout()); CHECK(Status::InvalidArg("bad").Code() == kInvalidArg);
}
TEST(hash_deterministic) { CHECK_EQ(HashSlot("hello"), HashSlot("hello")); }
TEST(hash_range) { CHECK(HashSlot("anykey") < kNumSlots); }
TEST(memory_put_get) { MemoryEngine e; CHECK_OK(e.Put("k","v")); Value v; CHECK_OK(e.Get("k",v)); CHECK_EQ(v,"v"); }
TEST(memory_delete) { MemoryEngine e; e.Put("k","v"); CHECK_OK(e.Delete("k")); Value v; CHECK(e.Get("k",v).IsNotFound()); }
TEST(memory_scan) {
    MemoryEngine e; for(int i=0;i<50;i++) { char k[8]; snprintf(k,8,"k%02d",i); e.Put(k,"v"+std::to_string(i)); }
    std::vector<KeyValue> r; CHECK_OK(e.Scan("k10","k20",100,r)); CHECK_EQ(r.size(),(size_t)10);
}
TEST(memory_snapshot) {
    MemoryEngine e; e.Put("a","1"); e.Put("b","2"); std::string s; CHECK_OK(e.Snapshot(s));
    MemoryEngine e2; CHECK_OK(e2.RestoreSnapshot(s)); CHECK_EQ(e2.KeyCount(),(size_t)2);
    Value v; CHECK_OK(e2.Get("a",v)); CHECK_EQ(v,"1");
}
TEST(memory_batch) {
    MemoryEngine e; Batch b; b.push_back({BatchOp::PUT,"k1","v1"}); b.push_back({BatchOp::DEL,"k1",""});
    CHECK_OK(e.ApplyBatch(b)); Value v; CHECK(e.Get("k1",v).IsNotFound());
}
TEST(raft_log_append) { RaftLog l; l.Append(1,1,"c1"); l.Append(2,1,"c2"); CHECK_EQ(l.LastIndex(),(RaftIndex)2); CHECK_EQ(l.Size(),(size_t)2); }
TEST(raft_log_truncate) { RaftLog l; for(int i=1;i<=5;i++) l.Append(i,1,"c"); l.TruncateAfter(3); CHECK_EQ(l.LastIndex(),(RaftIndex)3); }
TEST(raft_node_init) { RaftConfig c; RaftNode n(c,"n1",{}); CHECK_EQ(n.CurrentTerm(),(RaftTerm)0); }
TEST(config_validate) { KvConfig c; c.node_id="test"; CHECK_OK(c.Validate()); c.raft_election_min_ms=50; c.raft_heartbeat_ms=100; CHECK(!c.Validate().ok()); }
TEST(consistent_hash) { ConsistentHash ch; ch.AddNode("n1"); ch.AddNode("n2"); CHECK(!ch.GetNode("k").empty()); }
TEST(protocol_codec) { std::string f; CHECK(EncodeFrame(MsgType::KV_PUT,"hello",f)); MsgType t; std::string d; CHECK(DecodeFrame(f.data(),f.size(),t,d)); CHECK_EQ(d,"hello"); }
TEST(varint) { std::string b; EncodeVarint64(300,b); const char* p=b.data(); CHECK_EQ(DecodeVarint64(p,b.data()+b.size()),(uint64_t)300); }
TEST(crc32c) { uint32_t c=CRC32C("abc",3); CHECK_EQ(CRC32C("abc",3),c); }
TEST(txn_context) { TxnContext t; CHECK_OK(t.Begin()); CHECK_OK(t.Put("k","v")); CHECK_OK(t.Commit()); }
TEST(server_lifecycle) { KvConfig c; c.node_id="t"; KvServer s(c); CHECK_OK(s.Start()); CHECK(s.IsRunning()); s.Stop(); }
TEST(server_ops) { KvConfig c; c.node_id="srv"; KvServer s(c); s.Start(); CHECK_OK(s.Api().Put("hello","world")); Value v; CHECK_OK(s.Api().Get("hello",v)); CHECK_EQ(v,"world"); s.Stop(); }

int main() {
    RUN(status_ok); RUN(status_not_found); RUN(status_not_leader); RUN(status_types);
    RUN(hash_deterministic); RUN(hash_range);
    RUN(memory_put_get); RUN(memory_delete); RUN(memory_scan); RUN(memory_snapshot); RUN(memory_batch);
    RUN(raft_log_append); RUN(raft_log_truncate); RUN(raft_node_init);
    RUN(config_validate); RUN(consistent_hash);
    RUN(protocol_codec); RUN(varint); RUN(crc32c);
    RUN(txn_context); RUN(server_lifecycle); RUN(server_ops);
    std::cout << g_passed << " passed, " << g_failed << " failed\n";
    return g_failed > 0 ? 1 : 0;
}
