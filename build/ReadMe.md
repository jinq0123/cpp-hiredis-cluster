# How to build using Premake

Use cpp-hiredis-cluster.sln or Makefile.

## How to generate sln or Makefile

* Windows
    1. Copy premake.exe in build dir.
    2. Run premake5.bat

* Linux

    ./premake5 --os=linux gmake

## Change premake5.lua

* boost, hiredis and other libraries are default in deps dir.
  You can add your include dirs in includedirs{}.
* You can add additional library dirs in libdirs{}.  