#pragma once

#include <dlfcn.h>

#include "util.h"
#include "node.h"

#include <boost/noncopyable.hpp>
#include <boost/filesystem.hpp>
#include <boost/filesystem/fstream.hpp>
#include <boost/format.hpp>

#include <vector>
#include <sstream>

namespace DT {

namespace fs = boost::filesystem;

namespace detail {

template<typename T>
struct typeName {};

template<>
struct typeName<double> {
  static const char* name() { return "double"; }
};

template<>
struct typeName<float> {
  static const char* name() { return "float"; }
};

class Writer {
 public:
  template<typename T>
  void writeLine(const T& line) {
    for (auto i = 0; i < indentLevel_; i++) {
      stream_ << "  ";
    }
    stream_ << line;
    stream_ << "\n";
  }

  std::string toString() const { return stream_.str(); }

  // Could just use pipe the output to GNU indent, but this is easy
  // enough
  class IndentGuard : public boost::noncopyable {
   public:
    explicit IndentGuard(Writer* writer) : writer_(writer) {
      BOOST_ASSERT(writer_);
      ++writer_->indentLevel_;
    }

    ~IndentGuard() {
      BOOST_ASSERT(writer_->indentLevel_ > 0);
      --writer_->indentLevel_;
    }
   private:
    Writer* writer_;
  };

 private:
  int64_t indentLevel_{0};
  std::stringstream stream_;
};

const std::string kEvalFunctionName = "evaluate";
const fs::path kCompilerPath = "gcc";

}  // namespace detail

/*
 * RAII wrapper for the state associated with a compiled decision
 * tree.
 */
class CompiledEvaluator : public boost::noncopyable {
 public:
  explicit CompiledEvaluator(const fs::path& soPath) {
    handle_ = dlopen(soPath.c_str(), RTLD_LAZY | RTLD_LOCAL);
    BOOST_ASSERT(handle_);

    const auto ptr = dlsym(handle_, detail::kEvalFunctionName.c_str());
    func_ = reinterpret_cast<EvalFunc>(ptr);
    BOOST_ASSERT(func_);
  }

  ~CompiledEvaluator() {
    dlclose(handle_);
  }

  inline ValueT evaluate(const FeatureVectorT& featureVector) const {
    return (*func_)(featureVector.data());
  }

 private:
  void* handle_{nullptr};
  EvalFunc func_{nullptr};
};

class Compiler {
 public:
  template<typename T>
  static std::unique_ptr<CompiledEvaluator> compile(const T& node) {
    // Generate CPP code for the tree/forest
    auto generatedCode = fs::unique_path("/tmp/generated_code-%%%%.cpp");
    {
      fs::ofstream output(generatedCode);
      output << codeGen(node);
    }

    auto objectFile = fs::unique_path("/tmp/generated_object-%%%%.o");

    // TODO(tulloch) - error checking or folly::subprocess, etc.
    system((boost::format("%s -O3 %s -c -o %s")
            % detail::kCompilerPath
            % generatedCode
            % objectFile).str().c_str());

    // Generate shared object file
    auto sharedObjectFile =
        fs::unique_path("/tmp/generated_shared_object-%%%%.so");
    system((boost::format("%s -shared %s -dynamiclib -o %s")
            % detail::kCompilerPath
            % objectFile
            % sharedObjectFile).str().c_str());

    return make_unique<CompiledEvaluator>(sharedObjectFile);
  }

  static std::string codeGen(const Forest& forest) {
    detail::Writer writer;
    writer.writeLine(R"delimiter(extern "C" {)delimiter");
    writer.writeLine("");

    // Write the functions evaluating the trees
    for (size_t i = 0; i < forest.trees_.size(); ++i) {
      writer.writeLine(boost::format("%s %s_%s(const %s* f) {")
                       % detail::typeName<ValueT>::name()
                       % detail::kEvalFunctionName
                       % i
                       % detail::typeName<ValueT>::name());
      {
        detail::Writer::IndentGuard g(&writer);
        printNode(*(forest.trees_[i]), &writer);
      }
      writer.writeLine("}");
      writer.writeLine("");
    }

    writer.writeLine(boost::format("%s %s(const %s* f) {")
                     % detail::typeName<ValueT>::name()
                     % detail::kEvalFunctionName
                     % detail::typeName<ValueT>::name());
    {
      detail::Writer::IndentGuard g(&writer);
      writer.writeLine(boost::format("%s result = 0.0;")
                       % detail::typeName<ValueT>::name());
      for (size_t i = 0; i < forest.trees_.size(); ++i) {
        writer.writeLine(boost::format("result += %s_%s(f);")
                         % detail::kEvalFunctionName
                         % i);
      }
      writer.writeLine("return result;");
    }
    writer.writeLine("}");
    writer.writeLine("");
    writer.writeLine("}");
    return writer.toString();
  }

  static std::string codeGen(const Node& root) {
    detail::Writer writer;
    writer.writeLine(R"delimiter(extern "C" {)delimiter");
    writer.writeLine("");
    writer.writeLine(boost::format("%s %s(const %s* f) {")
                     % detail::typeName<ValueT>::name()
                     % detail::kEvalFunctionName
                     % detail::typeName<ValueT>::name());
    {
      detail::Writer::IndentGuard g(&writer);
      printNode(root, &writer);
    }
    writer.writeLine("}");
    writer.writeLine("");
    writer.writeLine("}");
    return writer.toString();
  }

 private:
  static void printNode(const Node& current, detail::Writer* writer) {
    if (current.isLeaf()) {
      // TODO(tulloch) - double literals seems ~10% faster than float literals.
      writer->writeLine(boost::format("return %s;") % current.leafValue_);
      return;
    }

    writer->writeLine(boost::format("if (f[%s] < %s) {")
                      % current.feature_
                      % current.splitValue_);
    {
      detail::Writer::IndentGuard g(writer);
      printNode(*(current.left_), writer);
    }
    writer->writeLine("} else {");
    {
      detail::Writer::IndentGuard g(writer);
      printNode(*(current.right_), writer);
    }
    writer->writeLine("}");
  }
};

}  // namespace DT
