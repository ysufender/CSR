#pragma once

#include <unordered_map>

#include "CSRConfig.hpp"
#include "bytemode/process.hpp"

class Board
{
    private:
        //const CPU cpu; 
        //const RAM ram;

    public:
        std::unordered_map<uchar_t, Process> processes;
};
