#pragma once

#include <iostream>

#include "../transport.h"

class Stdio : public Transport
{
public:
    void Send(const std::string &message) override
    {
        std::cout << message << std::endl;
    }

    std::string Receive() override
    {
        std::string buf;
        std::getline(std::cin, buf);
        return buf;
    }
};
