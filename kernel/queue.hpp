#pragma once

#include <array>

#include "error.hpp"
#include "logger.hpp"

template<typename T>
class ArrayQueue{
  public:
    template<size_t N>
    ArrayQueue(std::array<T, N>& buf);
    Error Push(const T& value);
    bool HasFront() const;
    const T Front();

  
  private:
    const size_t _capacity;
    T* _data;
    size_t _read_pos, _write_pos, _count;

    ArrayQueue(T* buf, size_t size);
    Error Pop();

};

template<typename T>
template <size_t N>
ArrayQueue<T>::ArrayQueue(std::array<T, N>& buf) :
    ArrayQueue(buf.data(), N){
}

template<typename T>
ArrayQueue<T>::ArrayQueue(T* buf, size_t size) :
    _capacity{size},_data{buf},
    _read_pos{0},_write_pos{0},_count{0}{
}

template<typename T>
Error ArrayQueue<T>::Push(const T& value){
  if(_count == _capacity){
    return MAKE_ERROR(Error::kFull);
  }

  _data[_write_pos] = value;
  _count++;
  _write_pos++;
  if(_write_pos == _capacity){
    _write_pos = 0;
  }
  return MAKE_ERROR(Error::kSuccess);
}

template<typename T>
bool ArrayQueue<T>::HasFront() const{
  return _count > 0;
}

template<typename T>
const T  ArrayQueue<T>::Front() {
  auto ret = _data[_read_pos];
  if(auto err = Pop()){
    Log(kError, err, "Error while Queue Front: %s", err.Name());
    T empty;
    return empty;
  }
  return ret;
}

template<typename T>
Error ArrayQueue<T>::Pop(){
  if(_count==0){
    return MAKE_ERROR(Error::kEmpty);
  }

  _count--;
  _read_pos++;
  if(_read_pos == _capacity){
    _read_pos = 0;
  }
  return MAKE_ERROR(Error::kSuccess);
}