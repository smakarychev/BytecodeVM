#include "NativeFunctions.h"

std::vector<std::string> NativeFunctionsUtils::SplitFormatString(const std::string& formatString, u32 count)
{
    std::vector<std::string> result = {};
    result.reserve(count);

    usize offset = 0;
    i32 openBrackets = 0;
    u32 depth = 0;
    bool first = true;
    for (u32 i = 0; i < formatString.size(); i++)
    {
        if (formatString[i] == '{')
        {
            depth++;
            openBrackets++;
            first = false;
        }
        else if (formatString[i] == '}')
        {
            openBrackets--;
        }
        if (openBrackets < 0)
            return {};
        if (openBrackets == 0 && depth & 1 && !first)
        {
            result.push_back(formatString.substr(offset, i - offset + 1));
            offset = i + 1;
            depth = 0;
        }
    }
    if (offset != formatString.length())
        result.back() += formatString.substr(offset);
    return result;
}
