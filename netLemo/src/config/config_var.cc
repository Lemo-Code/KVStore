#include "lemo/config/config_var.h"

namespace lemo {
namespace config {

ConfigVarBase::ConfigVarBase(const std::string& name,
                             const std::string& description)
    : name_(utils::ToLower(name)), description_(description) {}

}  // namespace config
}  // namespace lemo
