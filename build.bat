@echo off

where /q cl || (echo WARNING: cl not found -- executable will not be built)
where /q node || (echo WARNING: node not found -- executable will not be built)

where /q node && (
	setlocal
	
	IF NOT EXIST build mkdir build
    IF NOT EXIST generated mkdir generated

	node export_runtime_binary.js enemy_instances.schema.yml build/enemy_instances.bin simple_dod_game.xlsx
	node export_imhex_pattern.js enemy_instances.schema.yml generated/enemy_instances.hexpat
	node export_c_header.js enemy_instances.schema.yml generated/enemy_instances.h
	
    node export_runtime_binary.js frame_data.schema.yml build/frame_data.bin
    node export_imhex_pattern.js frame_data.schema.yml generated/frame_data.hexpat
    node export_c_header.js frame_data.schema.yml generated/frame_data.h

    node export_runtime_binary.js enemy_bullets.schema.yml build/enemy_bullets.bin simple_dod_game.xlsx
    node export_imhex_pattern.js enemy_bullets.schema.yml generated/enemy_bullets.hexpat
    node export_c_header.js enemy_bullets.schema.yml generated/enemy_bullets.h

    node export_c_header.js enemy_bullets_update.schema.yml generated/enemy_bullets_update.h
    node export_runtime_binary.js enemy_bullets_update.schema.yml build/enemy_bullets_update.bin
    node export_imhex_pattern.js enemy_bullets_update.schema.yml generated/enemy_bullets_update.hexpat

    node export_c_header.js enemy_bullets_draw.schema.yml generated/enemy_bullets_draw.h

	pushd build

	where /q cl && (
		call cl -Zi -W4 -wd4201 -wd4100 -wd4505 -wd4127 -nologo /I..\generated ..\win32_d3d11.c -Fewin32_d3d11_dm.exe
		call cl -O2 -Zi -W4 -wd4201 -wd4100 -wd4505 -wd4127 -nologo /I..\generated ..\win32_d3d11.c -Fewin32_d3d11_rm.exe -Fmwin32_d3d11_rm.map /DNDEBUG
	)

	popd

	endlocal
)