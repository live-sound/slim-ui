@echo off
rem Builds tiny.c into build\tiny.exe. Run it from a Visual Studio developer prompt.
rem The libraries after kernel32 add only small helpers, such as wcslen and the stack check. None of them brings in the C runtime's startup code.
cd /d "%~dp0"
if not exist build mkdir build
cl /nologo /O1 /Oi /Gy /Gw /GS /utf-8 /std:c11 /I ..\include tiny.c /Fo:build\ /Fe:build\tiny.exe /link /NODEFAULTLIB /ENTRY:WinMainCRTStartup /SUBSYSTEM:WINDOWS /OPT:REF /OPT:ICF /INCREMENTAL:NO d2d1.lib dwrite.lib dwmapi.lib user32.lib gdi32.lib ole32.lib oleaut32.lib shell32.lib windowscodecs.lib uuid.lib kernel32.lib BufferOverflowU.lib libcmt.lib libucrt.lib libvcruntime.lib
