#pragma once
#include "Core.h"
#include "Obj.h"
#include "Types.h"
#include "VirtualMachine.h"

#include <cstdlib>

class Value;

namespace NativeFunctions
{
    inline NativeFn Clock = [](u8 argc, Value* argv, VirtualMachine* vm) {
        NativeFnCallResult result = {};
        CHECK_RETURN_RES(argc == 0, result, "'clock()' accepts 0 arguments, but {} given", argc)
        result.Result = (f64)std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now().time_since_epoch()).count();
        result.IsOk = true;
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
