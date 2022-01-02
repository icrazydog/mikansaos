#pragma once

#include <array>

#define MAKE_ERROR(code) Error((code), __LINE__, __FILE__)

class Error{
  public:
    enum Code{
      kSuccess,
      kFull,
      kEmpty,
      kIndexOutOfRange,
      kNoEnoughMemory,
      kNotImplemented,
      kTransferRingNotSet,
      kInvalidPhase,
      kInvalidSlotID,
      kAlreadyAllocated,
      kInvalidDescriptor,
      kInvalidEndpointNumber,
      kTransferFailed,
      kNoCorrespondingSetupStage,
      kNoWaiter,
      kUnknownXHCISpeedID,
      kNoPCIMSI,
      kUnknownPixelFormat,
      kNoSuchTask,
      kInvalidFormat,
      kInvalidFormatLA,
      kFrameTooSmall,
      kInvalidFile,
      kIsDirectory,
      kNoSuchEntry,
      kIsNotDirectory,
      kFreeTypeError,
      //keep this element last
      kLastOfCode,
    };

    Error(Code code, int line, const char* file) : 
      _code{code}, _line{line}, _file{file}{

    }

    Code Cause() const {
      return this->_code;
    }

    operator bool() const{
      return this->_code != kSuccess;
    }

    const char* Name() const {
      return _code_names[static_cast<int>(this->_code)];
    }

    int Line() const{
      return this->_line;
    }

    const char* File() const{
      return this->_file;
    }

  private:
    static constexpr std::array _code_names = {
      "kSuccess",
      "kFull",
      "kEmpty",
      "kIndexOutOfRange",
      "kNoEnoughMemory",
      "kNotImplemented",
      "kTransferRingNotSet",
      "kInvalidPhase",
      "kInvalidSlotID",
      "kAlreadyAllocated",
      "kInvalidDescriptor",
      "kInvalidEndpointNumber",
      "kTransferFailed",
      "kNoCorrespondingSetupStage",
      "kNoWaiter",
      "kUnknownXHCISpeedID",
      "kNoPCIMSI",
      "kUnknownPixelFormat",
      "kNoSuchTask",
      "kInvalidFormat",
      "kInvalidFormatLA",
      "kFrameTooSmall",
      "kIsDirectory",
      "kNoSuchEntry",
      "kIsNotDirectory",
      "kFreeTypeError",
      "kInvalidFile",
    };
    static_assert(Error::Code::kLastOfCode == _code_names.size());

    Code _code;
    int _line;
    const char* _file;
};

template <class T>
struct WithError{
  T value;
  Error error;
};
