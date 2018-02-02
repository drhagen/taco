#ifndef TACO_STORAGE_ARRAY_H
#define TACO_STORAGE_ARRAY_H

#include <memory>
#include <ostream>
#include <taco/type.h>

namespace taco {
namespace storage {

/// An array is a smart pointer to raw memory together with an element type,
/// a size (number of elements) and a reclamation policy.
class Array {
public:
  /// The memory reclamation policy of Array objects. UserOwns means the Array
  /// object will not free its data, free means it will reclaim data  with the
  /// C free function and delete means it will reclaim data with delete[].
  enum Policy {UserOwns, Free, Delete};

  /// Construct an empty array of undefined elements.
  Array();

  /// Construct an array of elements of the given type.
  Array(DataType type, void* data, size_t size, Policy policy=Free);

  /// Returns the type of the array elements
  const DataType& getType() const;

  /// Returns the number of array elements
  size_t getSize() const;

  /// Returns the array data.
  /// @{
  const void* getData() const;
  void* getData();
  /// @}

  /// Zero the array content
  void zero();

private:
  struct Content;
  std::shared_ptr<Content> content;
};

/// Send the array data as text to a stream.
std::ostream& operator<<(std::ostream&, const Array&);
std::ostream& operator<<(std::ostream&, Array::Policy);


/// Allows for performing certain operations on dynamically typed value
class TypedValue {
public:
  /// Allocates a memory location
  TypedValue();
  TypedValue(DataType type);
  TypedValue(DataType type, void *memLocation);
  const DataType& getType() const;

  /// Returns a pointer to the memory location
  void* get() const;

  void set(TypedValue value);

  //Casts constant to type
  void set(int constant);

  //calls free on the memory location
  void freeMemory();

  bool operator> (const TypedValue &other) const;
  bool operator<= (const TypedValue &other) const;

  bool operator< (const TypedValue &other) const;
  bool operator>= (const TypedValue &other) const;

  bool operator== (const TypedValue &other) const;
  bool operator!= (const TypedValue &other) const;

private:
  DataType type;
  void *memLocation;
  void set(void *location);
};

}}
#endif
