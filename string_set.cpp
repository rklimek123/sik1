#include "string_set.hpp"

static std::set<std::string> instance;

void set_reset() noexcept {
    instance.clear();
}

bool set_add(const char* str_raw) noexcept {
    bool result;
    std::string str(str_raw);

    try {
        instance.insert(str);
        result = true;
    } catch (std::exception) {
        result = false;
    }

    return result;
}

bool set_exists(const char* str_raw) noexcept {
    bool result;
    std::string str(str_raw);

    if (instance.find(str) == instance.end())
        result = false;
    else
        result = true;
    
    return result;
}

