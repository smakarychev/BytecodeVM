workspace "BytecodeVM"
    outputdir = "%{cfg.buildcfg}-%{cfg.system}-%{cfg.architecture}"
    configurations { "Debug", "Release", "DebugNaNBoxing", "ReleaseNaNBoxing"}
    architecture "x86_64"
    
    project "BytecodeVM"
	kind "ConsoleApp"
	language "C++"
    cppdialect "C++20"
	targetdir ("bin/" .. outputdir .. "/%{prj.name}")
    objdir ("bin-int/" .. outputdir .. "/%{prj.name}")

	files
    {
        "src/**.h",
        "src/**.cpp",
    }

    includedirs
    {
        "src",
    }

	filter "configurations:Debug"
        buildoptions { "/utf-8" }
		defines { "DEBUG_TRACE", "GC_STRESS_TEST" }
		runtime "Debug"
		symbols "on"

	filter "configurations:Release"
        buildoptions { "/utf-8" }
		runtime "Release"
		optimize "on"

    filter "configurations:DebugNaNBoxing"
        buildoptions { "/utf-8" }
        defines { "NAN_BOXING", "DEBUG_TRACE", "GC_STRESS_TEST" }
		runtime "Debug"
		symbols "on"

    filter "configurations:ReleaseNaNBoxing"
        buildoptions { "/utf-8" }
        defines { "NAN_BOXING", }
		runtime "Release"
		optimize "on"