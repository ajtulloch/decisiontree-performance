#include <memory>
#include <random>
#include <chrono>
#include <algorithm>
#include <iostream>

#include <gflags/gflags.h>
#include <boost/chrono/chrono.hpp>
#include <boost/chrono/duration.hpp>
#include <boost/chrono/process_cpu_clocks.hpp>

#include "util.h"
#include "node.h"
#include "flattened_tree.h"
#include "compiled_tree.h"
// #include "jit_tree.h"

DEFINE_int64(num_trees, 100, "");
DEFINE_int64(num_features, 1000, "");
DEFINE_int64(num_examples, 1000, "");
DEFINE_int64(num_iterations, 100, "");
DEFINE_int64(depth, 2, "");
DEFINE_bool(debug, false, "");

DEFINE_bool(run_forests, true, "");
DEFINE_bool(run_trees, false, "");
DEFINE_bool(output_csv, false, "");

namespace DT {

namespace {

const double kEps = 1e-4;

// From Facebook's `folly` library.
template <class T>
void doNotOptimizeAway(T&& datum) {
  asm volatile("" : "+r" (datum));
}

FeatureVectorT randomVector(size_t numFeatures) {
  FeatureVectorT result(numFeatures, 0.0);
  for (auto& val : result) {
    val = randomDouble01();
  }
  return result;
}

std::vector<FeatureVectorT> randomVectors(size_t numExamples,
                                          size_t numFeatures) {
  std::vector<FeatureVectorT> result(numExamples);
  for (auto& v : result) {
    v = randomVector(numFeatures);
  }
  return result;
}


std::unique_ptr<Node> randomTree(size_t numFeatures, size_t depth) {
  if (!depth) {
    auto leaf = make_unique<Node>();
    leaf->leafValue_ = randomDouble01();
    return std::move(leaf);
  }

  auto branch = make_unique<Node>();
  branch->feature_ = randomInt64(numFeatures);
  branch->splitValue_ = randomDouble01();
  branch->left_ = randomTree(numFeatures, depth - 1);
  branch->right_ = randomTree(numFeatures, depth - 1);
  return std::move(branch);
}

std::unique_ptr<Forest> randomForest(size_t numTrees,
                                     size_t numFeatures,
                                     size_t depth) {
  auto result = make_unique<Forest>();
  for (size_t i = 0; i < numTrees; ++i) {
    result->trees_.push_back(randomTree(numFeatures, depth));
  }
  return std::move(result);
}

template<typename T>
void benchmark(const std::string& description,
               const T& evaluator,
               const std::vector<FeatureVectorT>& featureVectors,
               size_t numIterations) {
  auto iteration = [&]() {
    const auto start = boost::chrono::process_real_cpu_clock::now();
    ValueT sum = 0.0;
    for (const auto& fv : featureVectors) {
      sum += evaluator.evaluate(fv);
    }
    doNotOptimizeAway(sum);
    const auto end = boost::chrono::process_real_cpu_clock::now();
    const auto delta = boost::chrono::duration_cast<boost::chrono::nanoseconds>(
        end - start);
    return std::chrono::nanoseconds(
        delta.count() / featureVectors.size());
  };

  std::vector<std::chrono::nanoseconds> results(numIterations);
  for (auto& elem : results) {
    elem = iteration();
  }

  std::sort(results.begin(), results.end());
  auto percentile = [&](double percentile) -> size_t {
    return results[results.size() * percentile].count();
  };

  if (FLAGS_output_csv) {
    for (const auto& result : results) {
      std::cout << (boost::format("%s, %s, %s, %s, %s")
                    % description
                    % FLAGS_num_trees
                    % FLAGS_depth
                    % FLAGS_num_features
                    % result.count())
                << std::endl;
    }
  }

  std::cerr << description << std::endl;
  std::cerr << (boost::format("Distribution of time take for %s predictions "
                              "over %s iterations")
                % featureVectors.size()
                % numIterations)
            << std::endl;

  std::cerr << (boost::format("Min: %s, 1: %s, 5: %s, 25: %s, 50: %s, "
                              "75: %s, 95: %s, 99: %s, Max: %s")
                % results.front().count()
                % percentile(0.01)
                % percentile(0.05)
                % percentile(0.25)
                % percentile(0.5)
                % percentile(0.75)
                % percentile(0.95)
                % percentile(0.99)
                % results.back().count())
            << std::endl;
}

void correctness(
    std::vector<std::function<ValueT(const FeatureVectorT&)>> evaluators,
    const std::vector<FeatureVectorT>& featureVectors) {
  for (const auto& fv : featureVectors) {
    std::vector<ValueT> results;
    for (const auto& evaluator : evaluators) {
      results.push_back(evaluator(fv));
    }
    // check pairwise equality
    for (size_t i = 0; i < results.size() - 1; i++) {
      std::cout << results[i] << ", " << results[i+1] << std::endl;
      if (std::abs(results[i] - results[i+1]) > kEps) {
        BOOST_THROW_EXCEPTION(std::runtime_error("Bad evaluator"));
      }
    }
  }
}

#define BENCH(name, tree) \
  benchmark(name, *(tree), featureVectors, FLAGS_num_iterations);

#define EVALUATE(name, tree) \
  [&](const DT::FeatureVectorT& fv) { return (tree)->evaluate(fv); },

void runForests() {
  const auto featureVectors = randomVectors(FLAGS_num_examples,
                                            FLAGS_num_features);
  const auto naiveForest = randomForest(FLAGS_num_trees,
                                        FLAGS_num_features,
                                        FLAGS_depth);
  const auto piecewiseFlatForest =
      make_unique<PiecewiseFlatForest>(*naiveForest);
  const auto contiguousFlatForest =
      make_unique<ContiguousFlatForest>(*naiveForest);

  const auto compiledForest = Compiler::compile(*naiveForest);
  #define FOREST_LIST(X) \
    X("Naive Forest", naiveForest)               \
    X("Piecewise Flat Forest", piecewiseFlatForest)  \
    X("Contiguous Flat Forest", contiguousFlatForest) \
    X("Compiled Forest", compiledForest)

  FOREST_LIST(BENCH)

  if (FLAGS_debug) {
    std::cout << "Code: " << DT::Compiler::codeGen(*naiveForest) << std::endl;
    correctness({FOREST_LIST(EVALUATE)}, featureVectors);
  }

  #undef FOREST_LIST
}

void runTrees() {
  const auto featureVectors = randomVectors(FLAGS_num_examples,
                                            FLAGS_num_features);

  const auto tree = randomTree(FLAGS_num_features, FLAGS_depth);
  const auto flatTree = make_unique<FlatTree>(*tree);
  const auto compiledTree = Compiler::compile(*tree);

  #define TREE_LIST(X) \
    X("Naive Tree", tree)                        \
    X("Flat Tree", flatTree)                  \
    X("Compiled Tree", compiledTree)

  TREE_LIST(BENCH)

  if (FLAGS_debug) {
    std::cout << "Code: " << DT::Compiler::codeGen(*tree) << std::endl;
    DT::correctness({TREE_LIST(EVALUATE)}, featureVectors);
  }
  #undef TREE_LIST
}

}  // namespace

}  // namespace DT

int main(int argc, char** argv) {
  google::ParseCommandLineFlags(&argc, &argv, true);

  if (FLAGS_run_trees) {
    DT::runTrees();
  }

  if (FLAGS_run_forests) {
    DT::runForests();
  }

  return 0;
}
