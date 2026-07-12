#pragma once
#include <stdexcept>
#include <string>

namespace graphlib {

/// Base of all graphlib exceptions.
class Error : public std::runtime_error {
    using std::runtime_error::runtime_error;
};

/// Invalid user input — mirrors Python's ValueError raised by matplotlib.
class ValueError final : public Error {
    using Error::Error;
};

/// Unknown key (rc parameter, registry lookup) — mirrors Python's KeyError.
class KeyError final : public Error {
    using Error::Error;
};

} // namespace graphlib
