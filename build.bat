@echo off

where /q cl || (echo WARNING: cl not found -- executable will not be built)
where /q node || (echo WARNING: node not found -- executable will not be built)

where /q node && (
	setlocal
	
	IF NOT EXIST build mkdir build

	node export_runtime_binary.js enemy_instances.shema.yml build/enemy_instances.bin simple_dod_game.xlsx
	node export_imhex_pattern.js enemy_instances.shema.yml enemy_instances.hexpat
	node export_c_header.js enemy_instances.shema.yml enemy_instances.h
	
	pushd build

	where /q cl && (
		call cl -Zi -W4 -wd4201 -wd4100 -wd4505 -wd4127 -nologo ..\win32_d3d11.c -Fewin32_d3d11_dm.exe
		call cl -O2 -Zi -W4 -wd4201 -wd4100 -wd4505 -wd4127 -nologo ..\win32_d3d11.c -Fewin32_d3d11_rm.exe -Fmwin32_d3d11_rm.map /DNDEBUG
	)

	popd

	endlocal
)