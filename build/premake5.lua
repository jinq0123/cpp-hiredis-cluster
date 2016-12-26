-- premake5.lua
--[[
Usage: 
	windows: premake5.exe --os=windows vs2015
	linux:  premake5.exe --os=linux gmake
]]

workspace "cpp-hiredis-cluster"
	configurations { "Debug", "Release" }
	targetdir "../bin/%{cfg.buildcfg}"
	language "C++"
	flags {
		"C++11",
		"StaticRuntime",
	}
	filter "configurations:Debug"
		flags { "Symbols" }
	filter "configurations:Release"
		defines { "NDEBUG" }
		optimize "On"
	filter {}

project "asyncexample"
	kind "ConsoleApp"
	files {
		"../src/examples/asyncexample.cpp",
		"../src/examples/hiredis-boostasio-adapter/*",
		"../include/*.h",
		"../include/adapters/adapter.h",
	}    
	includedirs {
		"../include",
		"../deps",
		"../deps/hiredis",
		"E:/ThirdParty/boost_1_60_0",
	}
	libdirs {
		"../deps/hiredis/Debug",
		"E:/ThirdParty/boost_1_60_0/stage/lib",
	}
	links {
		"hiredis",
	}
	
