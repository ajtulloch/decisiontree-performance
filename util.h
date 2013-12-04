#pragma once

#include <memory>
#include <random>

namespace DT {

template<class T, class... Args>
std::unique_ptr<T> make_unique(Args&&... args) {
  return std::unique_ptr<T>(new T(std::forward<Args>(args)...));
}

/*
 * Returns a double uniformly distributed on [0, 1]
 */
double randomDouble01();

/*
 * Returns an integer uniformly distributed on [0, max)
 */
int64_t randomInt64(int64_t max);

}  // namespace DT
