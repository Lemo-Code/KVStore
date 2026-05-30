#ifndef LSTL_EXCEPTION_H
#define LSTL_EXCEPTION_H

namespace lstl {

struct exception {};

struct bad_alloc : exception {};

struct out_of_range : exception {
  explicit out_of_range(const char* msg = "") : msg_(msg) {}

  const char* what() const throw() { return msg_ ? msg_ : ""; }

 private:
  const char* msg_;
};

}  // namespace lstl

#endif  // LSTL_EXCEPTION_H
