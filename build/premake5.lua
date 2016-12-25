-- premake5.lua
--[[
Usage: 
	windows: premake5.exe --os=windows vs2015
	linux:  premake5.exe --os=linux gmake
]]

hiredis_windows_dir = "../deps/hiredis-0.13.3-windows"

workspace "cpp-hiredis-cluster"
	configurations { "Debug", "Release" }
	targetdir "../bin/%{cfg.buildcfg}"
	language "C++"
	includedirs {
		"../include",
		hiredis_windows_dir,
		hiredis_windows_dir.."/hiredis",
	}
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
		hiredis_windows_dir.."/hiredis/msvs/deps/ad*",
		hiredis_windows_dir.."/hiredis/msvs/deps/ae.*",
		"../include/*.h",
	}    
	links {
		"hiredis",
		"win32_interop",
	}
	
