#include "NativeFunctions.h"

std::vector<std::string> NativeFunctionsUtils::SplitFormatString(const std::string& formatString, u32 count)
{
    std::vector<std::string> result = {};
    result.reserve(count);

    i32 openBrackets = 0;
    for (u32 i = 0; i < formatString.size(); i++)
    {
        if (formatString[i] == '{') openBrackets++;
        else if (formatString[i] == '}') openBrackets--;
        if (openBrackets < 0) return result;

        
    }
    
    return result;
}
