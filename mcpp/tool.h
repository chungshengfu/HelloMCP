#pragma once

#include <string>

#include <nlohmann/json.hpp>

class Tool
{
public:
    virtual std::string Name() const = 0;
    virtual std::string Description() const = 0;
    virtual nlohmann::json InputSchema() const = 0;

    virtual nlohmann::json Execute(const nlohmann::json& params) = 0;

    nlohmann::json Error(int code, const std::string& message) const
    {
        nlohmann::json error =
        {
            {"code", code},
            {"message", message}
        };
        return
        {
            {"error", error},
        };
    }

    nlohmann::json Result(const std::string& text) const
    {
        nlohmann::json content =
        {
            {"type", "text"},
            {"text", text},
        };
        nlohmann::json result =
        {
            {"content", {content}},
        };
        return
        {
            {"result", result},
        };
    }
};
