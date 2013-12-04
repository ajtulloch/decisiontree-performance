#pragma once

#include "util.h"
#include "node.h"
#include <boost/format.hpp>

namespace DT {

namespace detail {

const FeatureT kLeafFeature = -1;

}

struct FlatNode {
  FeatureT feature_{detail::kLeafFeature};
  ValueT value_{0.0};
  uint32_t leftChild_{0};
};

class FlatTree {
 public:
  explicit FlatTree(const Node& root) {
    nodes_ = { FlatNode{} };
    recur(root, 0);
  }

  ValueT evaluate(const FeatureVectorT& featureVector) const {
    for (uint32_t current = 0; ;) {
      const auto n = nodes_[current];
      if (n.feature_ == detail::kLeafFeature) {
        return n.value_;
      }

      current = featureVector[n.feature_] < n.value_
                                            ? n.leftChild_
                                            : n.leftChild_ + 1;
    }
  }

  // Public for {Contiguous/Piecewise}FlatForest
  std::vector<FlatNode> nodes_;

 private:
  void recur(const Node& current, uint32_t currentIndex) {
    if (current.isLeaf()) {
      nodes_[currentIndex].feature_ = detail::kLeafFeature;
      nodes_[currentIndex].value_ = current.leafValue_;
      return;
    }

    const auto leftChild = nodes_.size();
    nodes_.resize(nodes_.size() + 2);
    nodes_[currentIndex].feature_ = current.feature_;
    nodes_[currentIndex].value_ = current.splitValue_;
    nodes_[currentIndex].leftChild_ = leftChild;
    recur(*current.left_, leftChild);
    recur(*current.right_, leftChild + 1);
  }
};

class PiecewiseFlatForest {
 public:
  explicit PiecewiseFlatForest(const Forest& forest) {
    trees_.reserve(forest.trees_.size());
    for (const auto& tree : forest.trees_) {
      trees_.push_back(FlatTree(*tree));
    }
  }

  ValueT evaluate(const FeatureVectorT& featureVector) const {
    ValueT result = 0.0;
    for (const auto& tree : trees_) {
      result += tree.evaluate(featureVector);
    }
    return result;
  }

  std::vector<FlatTree> trees_;
};

class ContiguousFlatForest {
 public:
  explicit ContiguousFlatForest(const Forest& forest) {
    // Could be easily optimized to avoid vector reallocation
    for (const auto& tree : forest.trees_) {
      const auto startingIndex = nodes_.size();
      roots_.push_back(startingIndex);
      for (auto node : FlatTree(*tree).nodes_) {
        node.leftChild_ += startingIndex;
        nodes_.push_back(node);
      }
    }
  }

  ValueT evaluate(const FeatureVectorT& featureVector) const {
    ValueT result = 0.0;
    for (const auto root : roots_) {
      for (uint32_t current = root; ;) {
        const auto n = nodes_[current];
        if (n.feature_ == detail::kLeafFeature) {
          result += n.value_;
          break;
        }

        current = featureVector[n.feature_] < n.value_
                                              ? n.leftChild_
                                              : n.leftChild_ + 1;
      }
    }
    return result;
  }

  std::vector<uint32_t> roots_;
  std::vector<FlatNode> nodes_;
};

}  // namespace DT
