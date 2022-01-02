#pragma once

#include <cstdint>
#include <cstddef>

#include "logger.hpp"

template <typename T>
struct ArrayLength {};

template<typename T, size_t N>
struct ArrayLength<T[N]>{
  static const size_t value = N;
};

template <typename T>
class MemMapRegister {
  public:
    T Read() const{
      T tmp;
      for(size_t i = 0; i< _len; i++){
        tmp.data[i] = _value.data[i];
      }
      return tmp;
    }

    void Write(const T& value){
      for(size_t i=0 ; i < _len; i++){
        _value.data[i] = value.data[i];
      }
    }

  private:
    volatile T _value;
    static const size_t _len = ArrayLength<decltype(T::data)>::value;
};

template <typename T>
struct DefaultBitmap {
  T data[1];

  DefaultBitmap& operator =(const T& value){
    data[0] = value;
  }

  operator T() const { return data[0]; }
};

template <typename T>
class ArrayWrapper{
  public:
    using Iterator = T*;
    using ConstIterator = const T*;

    ArrayWrapper(uintptr_t base_addr, size_t size)
        : _array(reinterpret_cast<T*>(base_addr)),
          _size(size){

    }

    size_t Size() const { return _size; }
    Iterator begin(){return _array; }
    Iterator end(){ return _array + _size; }
    ConstIterator cbegin(){return _array; }
    ConstIterator cend(){ return _array + _size; }

    T& operator [](size_t index){ return _array[index]; }

  private:
    T* const _array;
    const size_t _size;
};