#include "util.h"
#include <chrono>
#include <atomic>
#include <unistd.h>
#include <sys/time.h>

namespace DT {

// Copied from folly::randomNumberSeed()
// in https://github.com/facebook/folly/blob/master/folly/Random.cpp
namespace {

std::atomic<uint32_t> seedInput(0);

}

uint32_t randomNumberSeed() {
  struct timeval tv;
  gettimeofday(&tv, NULL);
  const uint32_t kPrime0 = 51551;
  const uint32_t kPrime1 = 61631;
  const uint32_t kPrime2 = 64997;
  const uint32_t kPrime3 = 111857;
  return kPrime0 * (seedInput++)
    + kPrime1 * static_cast<uint32_t>(getpid())
    + kPrime2 * static_cast<uint32_t>(tv.tv_sec)
    + kPrime3 * static_cast<uint32_t>(tv.tv_usec);
}


std::mt19937& rng() {
  static std::mt19937 generator(randomNumberSeed());
  return generator;
}

double randomDouble01() {
  std::uniform_real_distribution<> distribution(0, 1);
  return distribution(rng());
}

int64_t randomInt64(int64_t max) {
  // inclusive bounds
  std::uniform_int_distribution<int64_t> distribution(0, max - 1);
  return distribution(rng());
}

}  // namespace DT
