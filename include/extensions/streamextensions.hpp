#pragma once

#define OStreamPos(stream, varName, errAct) \
    std::streamoff varName { stream.tellp() }; \
    if (varName == -1) errAct 

#define IStreamPos(stream, varName, errAct) \
    std::streamoff varName { stream.tellg() }; \
    if (varName == -1) errAct 
