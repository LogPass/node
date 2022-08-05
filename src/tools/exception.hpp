#pragma once

namespace logpass {

class Exception : public std::exception {
public:
    Exception(std::string_view error) : m_error(error) {}

    virtual const char* what() const noexcept override
    {
        return m_error.c_str();
    }

protected:
    std::string m_error;
};

}
