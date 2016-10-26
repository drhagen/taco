#ifndef TACO_TENSOR_H
#define TACO_TENSOR_H

#include <vector>
#include <queue>
#include <algorithm>
#include <memory>
#include <utility>
#include <iostream>

#include "internal_tensor.h"
#include "operator.h"
#include "format.h"
#include "expr.h"
#include "error.h"
#include "component_types.h"
#include "util/strings.h"
#include "util/variadic.h"
#include "util/comparable.h"
#include "util/intrusive_ptr.h"

namespace taco {
class PackedTensor;
std::ostream& operator<<(std::ostream& os, const PackedTensor& tp);

class Var;
class Expr;

namespace util {
std::string uniqueName(char prefix);
}

struct Read;

namespace ir {
class Stmt;
}

template <typename T>
class Tensor {
public:
  typedef std::vector<int>        Dimensions;
  typedef std::vector<int>        Coordinate;
  typedef std::pair<Coordinate,T> Value;

  Tensor(std::string name, Dimensions dimensions, Format format) : tensor(
      internal::Tensor(name, dimensions, format, internal::typeOf<T>())) {
    uassert(format.getLevels().size() == dimensions.size())
        << "The format size (" << format.getLevels().size()-1 << ") "
        << "of " << name
        << " does not match the dimension size (" << dimensions.size() << ")";
  }

  Tensor(Dimensions dimensions, Format format)
      : Tensor(util::uniqueName('A'), dimensions, format) {
  }

  std::string getName() const {
    return tensor.getName();
  }

  const std::vector<int>& getDimensions() const {
    return tensor.getDimensions();
  }

  size_t getOrder() const {
    return tensor.getOrder();
  }

  /// Get the format the tensor is packed into
  Format getFormat() const {
    return tensor.getFormat();
  }

  void insert(const Coordinate& coord, T val) {
    iassert(coord.size() == getOrder()) << "Wrong number of indices";
    tensor.insert(coord, val);
  }

  void insert(const Value& value) {
    insert(value.first, value.second);
  }

  void insert(const std::vector<Value>& values) {
    for (auto& value : values) {
      insert(value);
    }
  }

  /// Pack tensor into the given format
  void pack() {
    tensor.pack();
  }

  Read operator()(const std::vector<Var>& indices) {
    uassert(indices.size() == getOrder())
        << "A tensor of order " << getOrder() << " must be indexed with "
        << getOrder() << " variables. "
        << "Is indexed with: " << util::join(indices);
    return Read(tensor, indices);
  }

  template <typename... Vars>
  Read operator()(const Vars&... indices) {
    uassert(sizeof...(indices) == getOrder())
        << "A tensor of order " << getOrder() << " must be indexed with "
        << getOrder() << " variables. "
        << "Is indexed with: " << util::join(std::vector<Var>({indices...}));
    return Read(tensor, {indices...});
  }

  /// Compile the tensor expression.
  void compile() {
    uassert(getExpr().defined())
        << "The tensor does not have an expression to evaluate";
    tensor.compile();
  }

  // Assemble the tensor storage, including index and value arrays.
  void assemble() {
    // TODO: assert tensor has been compiled
    tensor.assemble();
  }

  // evaluate the values into the tensor storage.
  void evaluate() {
    // TODO: assert tensor has been compiled
    // TODO: assert tensor has been assembled
    tensor.evaluate();
  }

  friend std::ostream& operator<<(std::ostream& os, const Tensor<T>& t) {
    os << t.tensor;
    if (t.coordinates.size() > 0) {
      os << std::endl << "Coordinates: ";
      for (auto& coord : t.coordinates) {
        os << std::endl << "  (" << util::join(coord.loc) << "): " << coord.val;
      }
    }
    return os;
  }

  const std::vector<Var>& getIndexVars() const {
    return tensor.getIndexVars();
  }

  template <typename E = Expr>
  E getExpr() const {
    return to<E>(tensor.getExpr());
  }

  const std::shared_ptr<PackedTensor> getPackedTensor() const {
    return tensor.getPackedTensor();
  }

  void printIterationSpace() const {
    tensor.printIterationSpace();
  }

  // TODO: This implementation works by materializing a list of nonzeros and 
  //       iterating over that list. We might want to change this at some point 
  //       in the future to traverse the nonzeros in-place; this can probably 
  //       be done with some relatively minor adjustments to the current 
  //       implementation using Boost coroutines.
  class const_iterator {
  public:
    typedef const_iterator self_type;
    typedef Value value_type;
    typedef Value& reference;
    typedef Value* pointer;
    typedef std::forward_iterator_tag iterator_category;

    const_iterator(const const_iterator&) = default;

    const_iterator operator++() {
      nonzeros.pop();
      curVal = nonzeros.empty() ? Value() : nonzeros.front();
      return *this;
    }

    const Value& operator*() const {
      return curVal;
    }

    const Value* operator->() const {
      return &curVal;
    }

    bool operator==(const const_iterator& rhs) {
      return tensor == rhs.tensor && nonzeros.size() == rhs.nonzeros.size();
    }

    bool operator!=(const const_iterator& rhs) {
      return !(*this == rhs);
    }

  private:
    friend class Tensor;

    const_iterator(const Tensor<T>* tensor, bool isEnd = false) : 
        tensor(tensor) {
      Coordinate coord(tensor->getOrder());
      Coordinate ptrs(tensor->getOrder());

      if (!isEnd) {
        iterateOverIndices(0, coord, ptrs);
      }
      curVal = nonzeros.empty() ? Value() : nonzeros.front();
    }

    void iterateOverIndices(size_t lvl, Coordinate& coord, Coordinate& ptrs) {
      const auto& levels  = tensor->getFormat().getLevels();
      const auto& indices = tensor->getPackedTensor()->getIndices();

      if (lvl == tensor->getOrder()) {
        const T elem = tensor->getPackedTensor()->getValues()[ptrs[lvl - 1]];
        Value val(Coordinate(lvl), elem);

        for (size_t i = 0; i < lvl; ++i) {
          const size_t dim = levels[i].getDimension();
          val.first[dim] = coord[i];
        }

        nonzeros.push(val);
        return;
      }

      switch (levels[lvl].getType()) {
        case Dense: {
          const auto& dims = tensor->getDimensions();

          const size_t base = (lvl == 0) ? 0 : (ptrs[lvl - 1] * dims[lvl]);
          for (coord[lvl] = 0; coord[lvl] < dims[lvl]; ++coord[lvl]) {
            ptrs[lvl] = base + coord[lvl];
            iterateOverIndices(lvl + 1, coord, ptrs);
          }
          break;
        }
        case Sparse: {
          const auto& segs = indices[lvl][0];
          const auto& vals = indices[lvl][1];
          
          const size_t k = (lvl == 0) ? 0 : ptrs[lvl - 1];
          for (ptrs[lvl] = segs[k]; ptrs[lvl] < segs[k + 1]; ++ptrs[lvl]) {
            coord[lvl] = vals[ptrs[lvl]];
            iterateOverIndices(lvl + 1, coord, ptrs);
          }
          break;
        }
        default:
          not_supported_yet;
          break;
      } 
    }

    const Tensor<T>*  tensor;
    std::queue<Value> nonzeros;
    Value             curVal;
  };

  const_iterator begin() const {
    return const_iterator(this);
  }

  const_iterator end() const {
    return const_iterator(this, true);
  }

private:
  friend struct Read;

  internal::Tensor tensor;
};

}

#endif
