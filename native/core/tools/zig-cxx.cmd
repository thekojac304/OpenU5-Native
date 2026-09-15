@echo off
rem Optional portable host compiler adapter. Set ZIG_EXE to a local zig.exe.
"%ZIG_EXE%" c++ %*
