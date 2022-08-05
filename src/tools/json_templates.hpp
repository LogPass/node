#pragma once

namespace logpass {

template<typename T>
inline void to_json(json& j, const std::shared_ptr<T>& object)
{
    if (!object) {
        j = json::value_t::null;
    } else {
        object->toJSON(j);
    }
}

template<typename T>
concept has_toJSON = requires(const T& v, json& j)
{
    { v.toJSON(j) } -> std::same_as<void>;
};

template<has_toJSON T>
inline void to_json(json& j, const T& object)
{
    object.toJSON(j);
}

}
