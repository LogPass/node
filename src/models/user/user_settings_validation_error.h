#pragma once

namespace logpass {

class UserSettingsValidationError : public Exception {
    using Exception::Exception;
};

#define THROW_USER_SETTINGS_EXCEPTION(error) THROW_EXCEPTION(UserSettingsValidationError(error))

}
