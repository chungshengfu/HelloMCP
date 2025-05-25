#pragma once

#include <string>

class Transport
{
public:
    virtual void Send(const std::string& message) = 0;
    virtual std::string Receive() = 0;
};
