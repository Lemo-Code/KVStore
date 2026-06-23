#include "kvstore/server/kv_server.h"
#include "kvstore/config/kv_config.h"
#include "kvstore/common/kv_utils.h"
#include <iostream>
#include <csignal>
#include <atomic>
#include <sstream>
#include <iomanip>
#include <getopt.h>

namespace { std::atomic<bool> running{true}; void sig_handler(int) { running = false; } }

void PrintHelp(const char* prog) {
    std::cout << "KVStore — Distributed Key-Value Store\n\n"
              << "Usage: " << prog << " [options]\n\n"
              << "Options:\n"
              << "  --id <node-id>       Node identifier (default: node1)\n"
              << "  --port <port>        Client port (default: 9700)\n"
              << "  --raft-port <port>   Raft port (default: 9701)\n"
              << "  --peer <addr>        Cluster peer address (repeatable)\n"
              << "  --wal-dir <dir>      WAL directory (default: ./data/wal)\n"
              << "  --data-dir <dir>     Data directory (default: ./data)\n"
              << "  --shards <N>         Number of shards (default: 8)\n"
              << "  --replicas <N>       Replica count (default: 3)\n"
              << "  --log-level <level>  Log level (default: info)\n"
              << "  -h, --help           Show this help\n\n"
              << "Interactive Commands:\n"
              << "  PUT <key> <value>     Store a key-value pair\n"
              << "  GET <key>             Retrieve a value\n"
              << "  DEL <key>             Delete a key\n"
              << "  SCAN <start> <limit> [count]  Range scan\n"
              << "  STATUS                Show server status\n"
              << "  NODES                 List cluster nodes\n"
              << "  SHARDS                List shard information\n"
              << "  KEYS                  Show key count\n"
              << "  BATCH                 Batch mode: PUT k1 v1; PUT k2 v2; ...; EXEC\n"
              << "  QUIT                  Exit\n"
              << std::endl;
}

void PrintStatus(const zero::kvstore::AdminStatusRsp& st) {
    std::cout << "╔══════════════════════════════════════╗\n"
              << "║          KVSTORE STATUS              ║\n"
              << "╠══════════════════════════════════════╣\n"
              << "║ Node:    " << std::left << std::setw(28) << st.node_id << "║\n"
              << "║ Role:    " << std::setw(28) << st.role << "║\n"
              << "║ Term:    " << std::setw(28) << st.term << "║\n"
              << "║ Leader:  " << std::setw(28) << st.leader_id << "║\n"
              << "║ Keys:    " << std::setw(28) << st.key_count << "║\n"
              << "║ Uptime:  " << std::setw(25) << (st.uptime_ms / 1000) << "s" << " ║\n"
              << "╚══════════════════════════════════════╝\n";
}

int main(int argc, char** argv) {
    signal(SIGINT, sig_handler); signal(SIGTERM, sig_handler);

    zero::kvstore::KvConfig cfg;
    cfg.node_id = "node1";
    cfg.listen_port = 9700;

    // Parse command line
    for (int i = 1; i < argc; ++i) {
        std::string arg(argv[i]);
        if (arg == "-h" || arg == "--help") { PrintHelp(argv[0]); return 0; }
        else if (arg == "--id" && i + 1 < argc) cfg.node_id = argv[++i];
        else if (arg == "--port" && i + 1 < argc) cfg.listen_port = std::atoi(argv[++i]);
        else if (arg == "--raft-port" && i + 1 < argc) cfg.raft_port = std::atoi(argv[++i]);
        else if (arg == "--peer" && i + 1 < argc) cfg.peer_addrs.push_back(argv[++i]);
        else if (arg == "--wal-dir" && i + 1 < argc) cfg.wal_dir = argv[++i];
        else if (arg == "--data-dir" && i + 1 < argc) cfg.data_dir = argv[++i];
        else if (arg == "--shards" && i + 1 < argc) cfg.shard_count = std::atoi(argv[++i]);
        else if (arg == "--replicas" && i + 1 < argc) cfg.replica_count = std::atoi(argv[++i]);
        else if (arg == "--log-level" && i + 1 < argc) cfg.log_level = argv[++i];
        else if (arg.find("--id=") == 0) cfg.node_id = arg.substr(5);
        else if (arg.find("--port=") == 0) cfg.listen_port = std::atoi(arg.substr(7).c_str());
        else if (arg.find("--peer=") == 0) cfg.peer_addrs.push_back(arg.substr(7));
    }

    std::cout << "╔══════════════════════════════════════╗\n"
              << "║   KVSTORE v1.0 — Distributed KV     ║\n"
              << "║   Node: " << std::left << std::setw(27) << cfg.node_id << "║\n"
              << "║   Port: " << std::setw(27) << cfg.listen_port << "║\n"
              << "╚══════════════════════════════════════╝\n";

    zero::kvstore::KvServer server(cfg);
    auto st = server.Start();
    if (!st.ok()) { std::cerr << "Failed to start: " << st.Msg() << std::endl; return 1; }

    std::cout << "\nServer ready. Type HELP for commands, QUIT to exit.\n\n";

    std::string line;
    std::vector<std::pair<std::string, std::string>> batch_buf;
    bool batch_mode = false;

    while (running && std::getline(std::cin, line)) {
        if (line.empty()) continue;
        std::istringstream iss(line);
        std::string cmd; iss >> cmd;

        if (cmd == "QUIT" || cmd == "quit" || cmd == "exit") break;
        else if (cmd == "HELP" || cmd == "help") PrintHelp(argv[0]);
        else if (cmd == "STATUS" || cmd == "status") {
            auto status = server.GetStatus(); PrintStatus(status);
        }
        else if (cmd == "PUT" || cmd == "put") {
            std::string k, v; iss >> k; std::getline(iss, v);
            if (!v.empty() && v[0] == ' ') v = v.substr(1);
            if (batch_mode) { batch_buf.push_back({k, v}); std::cout << "  [batch] " << k << "\n"; }
            else { auto st = server.Api().Put(k, v); std::cout << (st.ok() ? "OK" : "ERR: "+st.Msg()) << "\n"; }
        }
        else if (cmd == "GET" || cmd == "get") {
            std::string k; iss >> k;
            if (!k.empty()) { std::string v; auto st = server.Api().Get(k, v);
                std::cout << (st.ok() ? v : "ERR: "+st.Msg()) << "\n"; }
        }
        else if (cmd == "DEL" || cmd == "del") {
            std::string k; iss >> k;
            if (!k.empty()) { auto st = server.Api().Delete(k);
                std::cout << (st.ok() ? "OK" : "ERR: "+st.Msg()) << "\n"; }
        }
        else if (cmd == "SCAN" || cmd == "scan") {
            std::string start, limit; size_t max = 100; iss >> start >> limit;
            if (iss >> max) {}
            std::vector<zero::kvstore::KeyValue> results;
            auto st = server.Api().Scan(start, limit, max, results);
            for (auto& kv : results) std::cout << "  " << kv.key << " => " << kv.val << "\n";
            std::cout << results.size() << " results\n";
        }
        else if (cmd == "KEYS" || cmd == "keys") {
            std::cout << "Key count: " << server.GetStatus().key_count << "\n";
        }
        else if (cmd == "BATCH") { batch_mode = true; batch_buf.clear(); std::cout << "Batch mode: enter PUT/DEL commands, EXEC to commit\n"; }
        else if (cmd == "EXEC" && batch_mode) {
            for (auto& [k, v] : batch_buf) server.Api().Put(k, v);
            std::cout << "Committed " << batch_buf.size() << " operations\n";
            batch_mode = false; batch_buf.clear();
        }
        else if (cmd == "CANCEL" && batch_mode) { batch_buf.clear(); batch_mode = false; std::cout << "Batch cancelled\n"; }
        else std::cout << "Unknown: " << cmd << " (try HELP)\n";
    }

    server.Stop();
    std::cout << "Server stopped. Goodbye.\n";
    return 0;
}
