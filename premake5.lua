workspace "BytecodeVM"
    outputdir = "%{cfg.buildcfg}-%{cfg.system}-%{cfg.architecture}"
    configurations { "Debug", "Release"}
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
		defines "DEBUG_TRACE"
		runtime "Debug"
		symbols "on"

	filter "configurations:Release"
        buildoptions { "/utf-8" }
		runtime "Release"
		optimize "on"
