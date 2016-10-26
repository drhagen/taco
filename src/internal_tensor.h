#ifndef TACO_INTERNAL_TENSOR_H
#define TACO_INTERNAL_TENSOR_H

#include <memory>
#include <string>
#include <vector>

#include "format.h"
#include "component_types.h"
#include "util/comparable.h"
#include "util/strings.h"

namespace taco {
class Var;
class Expr;
class PackedTensor;

namespace internal {

class Tensor : public util::Comparable<Tensor> {
public:
  Tensor(std::string name, std::vector<int> dimensions, 
         Format format, ComponentType);

  std::string getName() const;
  size_t getOrder() const;
  const std::vector<int>& getDimensions() const;
  const Format& getFormat() const;
  const ComponentType getComponentType() const;
  const std::vector<taco::Var>& getIndexVars() const;
  const taco::Expr& getExpr() const;
  const std::shared_ptr<PackedTensor> getPackedTensor() const;

  void insert(const std::vector<int>& coord, int val);
  void insert(const std::vector<int>& coord, float val);
  void insert(const std::vector<int>& coord, double val);
  void insert(const std::vector<int>& coord, bool val);

  void pack();
  void compile();
  void assemble();
  void evaluate();

  void setExpr(taco::Expr expr);
  void setIndexVars(std::vector<taco::Var> indexVars);

  void printIterationSpace() const;

  friend bool operator!=(const Tensor&, const Tensor&);
  friend bool operator<(const Tensor&, const Tensor&);

private:
  struct Content;

  struct Coordinate : util::Comparable<Coordinate> {
    typedef std::vector<int> Coord;

    Coordinate(const Coord& loc, int val)    : loc{loc}, intVal{val} {}
    Coordinate(const Coord& loc, float val)  : loc{loc}, floatVal{val} {}
    Coordinate(const Coord& loc, double val) : loc{loc}, doubleVal{val} {}
    Coordinate(const Coord& loc, bool val)   : loc{loc}, boolVal{val} {}

    std::vector<int> loc;
    union {
      int    intVal;
      float  floatVal;
      double doubleVal;
      bool   boolVal;
    };

    friend bool operator==(const Coordinate& l, const Coordinate& r) {
      iassert(l.loc.size() == r.loc.size());
      for (size_t i=0; i < l.loc.size(); ++i) {
        if (l.loc[i] != r.loc[i]) return false;
      }
      return true;
    }
    friend bool operator<(const Coordinate& l, const Coordinate& r) {
      iassert(l.loc.size() == r.loc.size());
      for (size_t i=0; i < l.loc.size(); ++i) {
        if (l.loc[i] < r.loc[i]) return true;
        else if (l.loc[i] > r.loc[i]) return false;
      }
      return true;
    }
  };

  std::shared_ptr<Content> content;
};

std::ostream& operator<<(std::ostream& os, const internal::Tensor& t);

}}
#endif
