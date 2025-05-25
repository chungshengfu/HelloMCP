#pragma once

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include <nlohmann/json.hpp>

#include "transport.h"
#include "tool.h"
#include "utils.h"

class Server
{
public:
    Server(std::string name, std::string version, nlohmann::json capabilities)
        : m_name(std::move(name)),
          m_version(std::move(version)),
          m_capabilities(std::move(capabilities))
    {
    }

    void AddTool(std::unique_ptr<Tool> tool)
    {
        const std::string& name = tool->Name();
        const auto rv = m_tools.emplace(name, std::move(tool));
        if (!rv.second)
        {
            throw std::runtime_error("Tool with name '" + name + "' already exists.");
        }
    }

    void Run(std::unique_ptr<Transport> transport)
    {
        for (;;)
        {
            std::string buf = transport->Receive();
            const auto message = nlohmann::json::parse(buf);
            const auto method = message["method"].template get<std::string>();
            if (method.starts_with("notifications/"))
            {
                HandleNotification(message);
            }
            else
            {
                HandleRequest(message, transport);
            }
        }
    }

private:
    void HandleNotification(const nlohmann::json& message)
    {
        const auto method = message["method"].template get<std::string>();
        S_SWITCH(method)
        {
            S_CASE("notifications/initialized"):
                break;
            S_CASE("notifications/cancelled"):
                exit(0);
                break;
            default:
                break;
        }
    }

    void HandleRequest(const nlohmann::json& message, const std::unique_ptr<Transport>& transport)
    {
        const auto method = message["method"].template get<std::string>();
        nlohmann::json response{};
        S_SWITCH(method)
        {
            S_CASE("initialize") :
                response = Initialize();
                break;
            S_CASE("tools/list") :
                response = ListTools();
                break;
            S_CASE("tools/call") :
                response = Invoke(message["params"]);
                break;
            default:
                response = Error(-32601, "Method not found");
                break;
        }
        response["jsonrpc"] = "2.0";
        response["id"] = message["id"];
        transport->Send(response.dump());
    }

    nlohmann::json Error(int code, const std::string& message)
    {
        nlohmann::json error =
        {
            {"code", code},
            {"message", message},
        };
        return
        {
            {"error", error},
        };
    }

    nlohmann::json Initialize()
    {
        nlohmann::json serverInfo =
        {
            {"name", m_name },
            {"version", m_version},
        };
        nlohmann::json result =
        {
            {"protocolVersion", "2024-11-05"},
            {"serverInfo", serverInfo},
            {"capabilities", m_capabilities},
            //{"instructions", ""},
        };
        return
        {
            {"result", result},
        };
    }

    nlohmann::json ListTools()
    {
        std::vector<nlohmann::json> tools = {};
        for (const auto& [name, tool] : m_tools)
        {
            auto inputSchema = tool->InputSchema();
            inputSchema["type"] = "object";
            tools.push_back(
            {
                {"name", name},
                {"description", tool->Description()},
                {"inputSchema", inputSchema},
            });
        }
        nlohmann::json result =
        {
            {"tools", tools},
        };
        return
        {
            {"result", result},
        };
    }

    nlohmann::json Invoke(const nlohmann::json& params)
    {
        auto name = params["name"].template get<std::string>();
        auto itr = m_tools.find(name);
        if (itr == m_tools.end())
        {
            return Error(-32601, "Method not found");
        }
        return itr->second->Execute(params);
    }

    std::string m_name = "";
    std::string m_version = "";
    nlohmann::json m_capabilities{};
    std::unordered_map<std::string, std::unique_ptr<Tool>> m_tools = {};
};
