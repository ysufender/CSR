#pragma once

#include <unordered_map>

#include "CSRConfig.hpp"
#include "bytemode/process.hpp"

using ProcessCollection = std::unordered_map<uchar_t, Process>;

class Board
{
    public:
        Board() = delete;

    private:
        ProcessCollection processes;
        //const CPU cpu; 
        //const RAM ram;
};
