#pragma once

namespace logpass {

class InvalidPowerException : public Exception {
    using Exception::Exception;
};

#define THROW_INVALID_POWER_EXCEPTION(error) THROW_EXCEPTION(InvalidPowerException(error))

}
