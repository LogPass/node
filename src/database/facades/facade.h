#pragma once

#include <database/columns/column.h>

namespace logpass {
namespace database {

class Facade {
public:
    Facade() = default;
    virtual ~Facade() = default;
    Facade(const Facade&) = delete;
    Facade& operator = (const Facade&) = delete;

private:

};

}
}
