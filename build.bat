@echo off

where /q cl || (echo WARNING: cl not found -- MSVC executable will not be built)

setlocal

IF NOT EXIST build mkdir build
pushd build

where /q cl && (
  call cl -Zi -W4 -wd4201 -wd4100 -wd4505 -wd4127 -nologo ..\win32_d3d11.c -Fewin32_d3d11_dm.exe
  call cl -O2 -Zi -W4 -wd4201 -wd4100 -wd4505 -wd4127 -nologo ..\win32_d3d11.c -Fewin32_d3d11_rm.exe -Fmwin32_d3d11_rm.map /DNDEBUG
)

popd

endlocal