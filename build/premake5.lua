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
		"../deps/hiredis-boostasio-adapter/boostasio.*",
		"../include/*.h",
		"../include/adapters/adapter.h",
		"../include/adapters/boostasioadapter.h",
	}    
	includedirs {
		"../include",
		"../deps",
		"../deps/hiredis",
		"../deps/boost",
	}
	libdirs {
		"../deps/hiredis/Debug",
		"../deps/boost/lib",
	}
	links {
		"hiredis",
		"ws2_32",
	}
	
