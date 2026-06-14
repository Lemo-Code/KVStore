#pragma once

#include "ledis/types.h"

namespace ledis {

struct Command {
  Sds name;
  /** MPSC 入队路径暂用 SdsArgList，后续可切换为 SdsVector。 */
  SdsArgList args;
  size_t argc() const { return 1 + args.size(); }
};

}  // namespace ledis
