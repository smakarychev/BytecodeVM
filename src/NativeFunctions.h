#pragma once
#include "Core.h"
#include "Common/Random.h"
#include "Obj.h"
#include "Types.h"
#include "VirtualMachine.h"
#include "ValueFormatter.h"

#include <cstdlib>


class Value;

namespace NativeFunctionsUtils
{
    std::vector<std::string> SplitFormatString(const std::string& formatString, u32 count);
}

namespace NativeFunctions
{
    inline NativeFn Print = [](u8 argc, Value* argv, VirtualMachine* vm) {
        NativeFnCallResult result = {};
        CHECK_RETURN_RES(argc >= 1, result, "'print()' accepts at least 1 argument, but {} given", argc)
        const std::string& formatString = argv[0].As<ObjHandle>().As<StringObj>().String;
        if (argc == 1)
        {
            std::cout << formatString;
            result.IsOk = true;
        }
        else
        {
            std::vector<std::string> subFormats = NativeFunctionsUtils::SplitFormatString(formatString, argc);
            if (subFormats.size() == argc - 1)
            {
                result.IsOk = true;
                for (u32 i = 1; i < argc; i++)
                {
                    std::cout << std::vformat(subFormats[i - 1], std::make_format_args(argv[i]));
                }
            }
            else
            {
                LOG_ERROR("Format string expects {} agruments but {} given.", subFormats.size(), argc - 1);
            }
        }
        return result;
    };
    
    inline NativeFn Clock = [](u8 argc, Value* argv, VirtualMachine* vm) {
        NativeFnCallResult result = {};
        CHECK_RETURN_RES(argc == 0, result, "'clock()' accepts 0 arguments, but {} given", argc)
        result.Result = (f64)std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now().time_since_epoch()).count();
        result.IsOk = true;
        return result;
    };

    inline NativeFn Sleep = [](u8 argc, Value* argv, VirtualMachine* vm) {
        NativeFnCallResult result = {};
        CHECK_RETURN_RES(argc == 1, result, "'sleep()' accepts 1 argument, but {} given", argc)
        if (argv[0].HasType<f64>() && argv[0].As<f64>() >= 0)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds((u64)argv[0].As<f64>()));
            result.IsOk = true;
        }
        return result;
    };

    inline NativeFn Str = [](u8 argc, Value* argv, VirtualMachine* vm) {
        NativeFnCallResult result = {};
        CHECK_RETURN_RES(argc == 1, result, "'str()' accepts 1 argument, but {} given", argc)
        if (argv[0].HasType<ObjHandle>() && argv[0].As<ObjHandle>().HasType<StringObj>())
        {
            result.Result = *argv;
            result.IsOk = true;
        }
        else if (argv[0].HasType<f64>())
        {
            result.Result = vm->AddString(std::format("{}", argv[0].As<f64>()));
            result.IsOk = true;
        }
        return result;
    };

    inline NativeFn Int = [](u8 argc, Value* argv, VirtualMachine* vm) {
        NativeFnCallResult result = {};
        CHECK_RETURN_RES(argc == 1, result, "'int()' accepts 1 argument, but {} given", argc)
        if (argv[0].HasType<ObjHandle>() && argv[0].As<ObjHandle>().HasType<StringObj>())
        {
            const std::string& string = argv[0].As<ObjHandle>().As<StringObj>().String;
            if (string.length() == 0)
                return result;
            
            char* end;
            i32 asInt = strtol(argv[0].As<ObjHandle>().As<StringObj>().String.c_str(), &end, 0);
            // if `end` is not `\0` we failed to parse
            if (*end == '\0')
            {
                result.Result = (f64)asInt;
                result.IsOk = true;                
            }
        }
        else if (argv[0].HasType<f64>())
        {
            result.Result = std::floor(argv[0].As<f64>());
            result.IsOk = true;
        }
        else if (argv[0].HasType<bool>())
        {
            result.Result = (f64)argv[0].As<bool>();
            result.IsOk = true;
        }
        return result;
    };

    inline NativeFn Float = [](u8 argc, Value* argv, VirtualMachine* vm) {
        NativeFnCallResult result = {};
        CHECK_RETURN_RES(argc == 1, result, "'float()' accepts 1 argument, but {} given", argc)
        if (argv[0].HasType<ObjHandle>() && argv[0].As<ObjHandle>().HasType<StringObj>())
        {
            const std::string& string = argv[0].As<ObjHandle>().As<StringObj>().String;
            if (string.length() == 0)
                return result;
            
            char* end;
            f64 asF64 = strtod(argv[0].As<ObjHandle>().As<StringObj>().String.c_str(), &end);
            // if `end` is not `\0` we failed to parse
            if (*end == '\0')
            {
                result.Result = asF64;
                result.IsOk = true;                
            }
        }
        else if (argv[0].HasType<f64>())
        {
            result.Result = argv[0];
            result.IsOk = true;
        }
        else if (argv[0].HasType<bool>())
        {
            result.Result = (f64)argv[0].As<bool>();
            result.IsOk = true;
        }
        return result;
    };

    inline NativeFn Rand = [](u8 argc, Value* argv, VirtualMachine* vm) {
        NativeFnCallResult result = {};
        CHECK_RETURN_RES(argc == 0, result, "'rand()' accepts 0 argument, but {} given", argc)
        result.Result = Random::F64();
        result.IsOk = true;
        return result;
    };

    inline NativeFn Len = [](u8 argc, Value* argv, VirtualMachine* vm) {
        NativeFnCallResult result = {};
        CHECK_RETURN_RES(argc == 1, result, "'len()' accepts 1 argument, but {} given", argc)
        if (argv[0].HasType<ObjHandle>() && argv[0].As<ObjHandle>().HasType<StringObj>())
        {
            result.Result = (f64)argv[0].As<ObjHandle>().As<StringObj>().String.length();
            result.IsOk = true;
        }
        else if (argv[0].HasType<ObjHandle>() && argv[0].As<ObjHandle>().HasType<CollectionObj>())
        {
            result.Result = (f64)argv[0].As<ObjHandle>().As<CollectionObj>().ItemCount;
            result.IsOk = true;
        }
        return result;
    };
}
