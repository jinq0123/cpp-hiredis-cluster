-- premake5.lua
--[[
Usage: 
	windows: premake5.exe --os=windows vs2015
	linux:	premake5.exe --os=linux gmake
]]

workspace "cpp-hiredis-cluster"
	kind "ConsoleApp"
	configurations { "Debug", "Release" }
	targetdir "../bin/%{cfg.buildcfg}"
	language "C++"
	flags {
		"C++11",
		-- "StaticRuntime",
	}
	files {
		"../include/**.h",
		"../include/**.hpp",
	}
	includedirs {
		"../include",
		"../deps",
		"../deps/hiredis",
		"../deps/boost",
		"../deps/libevent/include",
		"../deps/libevent/build/include",
	}
	libdirs {
		"../deps/boost/lib",
	}
	links {
		"event",
	}

	filter "configurations:Debug"
		flags { "Symbols" }
		libdirs {
			"../deps/hiredis/Debug",
			"../deps/libevent/build/lib/Debug",
		}
	filter "configurations:Release"
		defines { "NDEBUG" }
		optimize "On"
		libdirs {
			"../deps/hiredis/Release",
			"../deps/libevent/build/lib/Release",
		}
	filter {}

	if os.is("windows") then
		defines {
			"SIGPIPE=13",
			"usleep=(void)",
		}
		links {
			"hiredis",
			"ws2_32",
			"event_core",
			"event_extra",
		}
	end	 -- if

project "async"
	files { "../src/examples/asyncexample.cpp" }

project "asyncasio"
	files {
		"../src/examples/asyncexampleasio.cpp",
		"../include/adapters/hiredis-boostasio-adapter/boostasio.cpp",
	}
	
project "asyncerr"
	files { "../src/examples/asyncerrorshandling.cpp" }

project "testing"
	files { "../src/testing/testing.cpp" }

project "sync"
	files { "../src/examples/example.cpp" }

project "unix"
	files { "../src/examples/unixsocketexample.cpp" }

project "threadedpool"
	files { "../src/examples/threadpool.cpp" }

project "testing_disconnect_cluster"
	files { "../src/testing/clusterdisconnect.cpp" }
