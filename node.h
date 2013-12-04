#pragma once

#include <memory>
#include <vector>
#include <boost/assert.hpp>
#include <type_traits>

namespace DT {

/*
 * Primitive types used
 */
using FeatureT = uint32_t;
using FeatureVectorT = std::vector<float>;
using ValueT = FeatureVectorT::value_type;
using EvalFunc = ValueT (*)(const ValueT*);


struct Node {
  FeatureT feature_{0};
  ValueT splitValue_{0};
  ValueT leafValue_{0};
  std::unique_ptr<Node> left_;
  std::unique_ptr<Node> right_;

  inline bool isLeaf() const { return !left_ && !right_; }

  inline ValueT evaluate(const FeatureVectorT& featureVector) const {
    if (isLeaf()) {
      return leafValue_;
    }
    const auto& branch = featureVector[feature_] < splitValue_ ? left_ : right_;
    return branch->evaluate(featureVector);
  }
};

struct Forest {
  std::vector<std::unique_ptr<Node>> trees_;

  inline ValueT evaluate(const FeatureVectorT& featureVector) const {
    ValueT result{0.0};
    for (const auto& tree : trees_) {
      result += tree->evaluate(featureVector);
    }
    return result;
  }
};


}  // namespace DT
