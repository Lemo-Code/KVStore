/*
 * zero Channel<T> — explicit template instantiations
 *
 * Channel<T> is a template-heavy class whose full implementation lives in
 * the header (zero/fiber/channel.h).  Every method is defined inline
 * within the class body, so there is no out-of-line implementation to
 * provide here.
 *
 * This file serves two purposes:
 *
 *   1. Explicit template instantiations for the most commonly used types.
 *      This reduces compile-time bloat in downstream translation units by
 *      emitting the template code once, here, rather than once per user.
 *
 *   2. Resolving the hidden Scheduler dependency.  The Channel template
 *      calls Scheduler::GetThis() and Scheduler::schedule() in its
 *      blocking send/recv paths, yet the header does not include the
 *      Scheduler header.  By including it here and performing explicit
 *      instantiations, we ensure the dependency is satisfied at library
 *      build time rather than at the point of use.
 *
 * Types instantiated:
 *   Channel<int>              — event counters, small control signals
 *   Channel<long>             — large counters, timestamps
 *   Channel<unsigned int>     — sizes, counts, indices
 *   Channel<std::string>      — textual message passing between fibers
 *   Channel<Fiber::Ptr>       — fiber handoff (migration between threads)
 */

#include "zero/fiber/channel.h"
#include "zero/scheduler/scheduler.h"

#include <string>

namespace zero {

// Explicit template instantiations.
// Each line forces the compiler to emit the full set of methods for that
// specialization in this translation unit.

template class Channel<int>;
template class Channel<long>;
template class Channel<unsigned int>;
template class Channel<std::string>;
template class Channel<Fiber::Ptr>;

} // namespace zero
