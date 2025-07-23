@echo off

where /q cl || (echo WARNING: cl not found -- executable will not be built)
where /q node || (echo WARNING: node not found -- executable will not be built)

where /q node && (
	setlocal
	
	IF NOT EXIST build mkdir build
    IF NOT EXIST generated mkdir generated

	node export_runtime_binary.js enemy_instances.schema.yml build/enemy_instances.bin enemy_instances.xlsx
	node export_imhex_pattern.js enemy_instances.schema.yml generated/enemy_instances.hexpat
	node export_c_header.js enemy_instances.schema.yml generated/enemy_instances.h
	
    node export_runtime_binary.js frame_data.schema.yml build/frame_data.bin
    node export_imhex_pattern.js frame_data.schema.yml generated/frame_data.hexpat
    node export_c_header.js frame_data.schema.yml generated/frame_data.h

    node export_runtime_binary.js bullets.schema.yml build/enemy_bullets.bin enemy_bullets.xlsx
    node export_runtime_binary.js bullets.schema.yml build/hero_bullets.bin hero_bullets.xlsx
    node export_imhex_pattern.js bullets.schema.yml generated/bullets.hexpat
    node export_c_header.js bullets.schema.yml generated/bullets.h

    node export_c_header.js bullets_update.schema.yml generated/bullets_update.h
    node export_runtime_binary.js bullets_update.schema.yml build/enemy_bullets_update.bin
    node export_imhex_pattern.js bullets_update.schema.yml generated/bullets_update.hexpat

    node export_c_header.js bullets_draw.schema.yml generated/bullets_draw.h

    node export_c_header.js enemy_instances_update.schema.yml generated/enemy_instances_update.h
    node export_runtime_binary.js enemy_instances_update.schema.yml build/enemy_instances_update.bin
    node export_imhex_pattern.js enemy_instances_update.schema.yml generated/enemy_instances_update.hexpat

    node export_c_header.js enemy_instances_draw.schema.yml generated/enemy_instances_draw.h

    node export_c_header.js game_state.schema.yml generated/game_state.h
    node export_runtime_binary.js game_state.schema.yml build/game_state.bin
    node export_imhex_pattern.js game_state.schema.yml generated/game_state.hexpat

    node export_c_header.js bullet_source_instances.schema.yml generated/bullet_source_instances.h
    node export_c_header.js bullet_source_instances_update.schema.yml generated/bullet_source_instances_update.h

    node export_c_header.js wave_update.schema.yml generated/wave_update.h
    node export_runtime_binary.js wave_update.schema.yml build/wave_update.bin
    node export_imhex_pattern.js wave_update.schema.yml generated/wave_update.hexpat

    node export_c_header.js play_area.schema.yml generated/play_area.h

	pushd build

	where /q cl && (
		call cl -Zi -W4 -wd4201 -wd4100 -wd4505 -wd4127 -nologo /I..\generated ..\win32_d3d11.c -Fewin32_d3d11_dm.exe
		call cl -O2 -Zi -W4 -wd4201 -wd4100 -wd4505 -wd4127 -nologo /I..\generated ..\win32_d3d11.c -Fewin32_d3d11_rm.exe -Fmwin32_d3d11_rm.map /DNDEBUG
	)

	popd

	endlocal
)