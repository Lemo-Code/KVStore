#include "kvstore/storage/compactor.h"
namespace zero { namespace kvstore {
Status Compactor::Compact(const std::string& dir, int level) {
    (void)dir; (void)level; return Status::OK();
}
Status Compactor::MergeLevels(int src, int dst) {
    (void)src; (void)dst; return Status::OK();
}
}} // namespace
