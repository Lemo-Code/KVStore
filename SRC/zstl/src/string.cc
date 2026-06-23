// zstl string — compiled translation unit
// Most string operations are inline in string.h
#include "zstl/containers/string.h"

namespace zstl {
// Explicit template instantiation for char
template class basic_string<char>;
} // namespace zstl
