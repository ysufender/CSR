#include <exception>

#include "bytemode/assemblyinfo.hpp"
#include "extensions/streamextensions.hpp"
#include "system.hpp"

Error AssemblyInfo::Deserialize(std::istream& inFile) noexcept
{
    using namespace Extensions;

    if (inFile.fail() || inFile.bad())
    {
        LOGE(System::LogLevel::Medium, "Couldn't open file while deserializing assembly info.");
        return System::ErrorCode::FileIOError;
    }

   try
   {
        // Flags
        Serialization::DeserializeInteger(flags, inFile);

        //Name
        if (flags & AssemblyFlags::StoreName)
            Serialization::DeserializeContainer<std::string, size_t, uchar_t>(
                path, 
                inFile,
                Serialization::DeserializeInteger<uchar_t>
            );

        // Imports
        Serialization::DeserializeContainer<AssemblyInfo::ImportCollection, sysbit_t, std::string>(
            runtimeImports, 
            inFile,
            [](std::string& data, std::istream& stream){
            Serialization::DeserializeContainer<std::string, uint16_t, uchar_t>(
                data, 
                stream,
                &Serialization::DeserializeInteger<uchar_t>
            );} 
        );

        // Symbol Info
        if (!(flags & AssemblyFlags::SymbolInfo))
            return System::ErrorCode::Ok;

        // Defined Symbols
        Serialization::DeserializeContainer<SymbolMap, sysbit_t, SymbolInfo>(
            symbolMap,
            inFile,
            [](std::pair<size_t, sysbit_t>& keyValue, std::istream& in) {
                Serialization::DeserializeInteger(keyValue.first, in);
                Serialization::DeserializeInteger(keyValue.second, in);
            } 
        );

        // Unknown Symbols
        Serialization::DeserializeContainer<UnknownSymbolCollection, sysbit_t, SymbolInfo>(
                unknownSymbols,
                inFile,
                [](SymbolInfo& symbolInfo, std::istream& in) {
                Serialization::DeserializeInteger(symbolInfo.first, in);
                Serialization::DeserializeInteger(symbolInfo.second, in);
            }
        );
   }
   catch (const std::exception&)
   {
       return Error::UnhandledException;
   }

    return System::ErrorCode::Ok;
}
