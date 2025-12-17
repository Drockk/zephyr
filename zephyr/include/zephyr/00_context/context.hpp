#pragma once

#include <map>
#include <memory>
#include <string>

namespace zephyr::context
{
class Context
{
public:
    template<typename T>
    auto set(const std::string& t_name, std::shared_ptr<T> t_resource) -> void
    {
        m_resources[t_name] = t_resource;
    }

    template<typename T>
    auto get(const std::string& t_name) const
    {
        auto it = m_resources.find(t_name);
        if (it == m_resources.end()) {
            return nullptr;
        }

        return std::static_pointer_cast<T>(it->second);
    }

private:
    std::map<std::string, std::shared_ptr<void>> m_resources;
};
}
