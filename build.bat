@echo off

where /q cl || (echo WARNING: cl not found -- executable will not be built)
where /q node || (echo WARNING: node not found -- executable will not be built)

where /q node && (
	setlocal
	
	IF NOT EXIST build mkdir build
    IF NOT EXIST generated mkdir generated
    (
        start /B node export_runtime_binary.js enemy_instances.schema.yml build/enemy_instances.bin enemy_instances.xlsx >CON 2>CON
        start /B node export_imhex_pattern.js enemy_instances.schema.yml generated/enemy_instances.hexpat >CON 2>CON
        start /B node export_c_header.js enemy_instances.schema.yml generated/enemy_instances.h >CON 2>CON
	
        start /B node export_c_header.js enemy_instances_wave.schema.yml generated/enemy_instances_wave.h >CON 2>CON

        start /B node export_c_header.js hero_instances_draw.schema.yml generated/hero_instances_draw.h >CON 2>CON

        start /B node export_runtime_binary.js hero_instances.schema.yml build/hero_instances.bin hero_instances.xlsx >CON 2>CON
        start /B node export_imhex_pattern.js hero_instances.schema.yml generated/hero_instances.hexpat >CON 2>CON
        start /B node export_c_header.js hero_instances.schema.yml generated/hero_instances.h >CON 2>CON

        start /B node export_runtime_binary.js frame_data.schema.yml build/frame_data.bin >CON 2>CON
        start /B node export_imhex_pattern.js frame_data.schema.yml generated/frame_data.hexpat >CON 2>CON
        start /B node export_c_header.js frame_data.schema.yml generated/frame_data.h >CON 2>CON

        start /B node export_runtime_binary.js bullets.schema.yml build/enemy_bullets.bin enemy_bullets.xlsx >CON 2>CON
        start /B node export_runtime_binary.js bullets.schema.yml build/hero_bullets.bin hero_bullets.xlsx >CON 2>CON
        start /B node export_imhex_pattern.js bullets.schema.yml generated/bullets.hexpat >CON 2>CON
        start /B node export_c_header.js bullets.schema.yml generated/bullets.h >CON 2>CON

        start /B node export_c_header.js bullets_update.schema.yml generated/bullets_update.h >CON 2>CON
        start /B node export_runtime_binary.js bullets_update.schema.yml build/enemy_bullets_update.bin >CON 2>CON
        start /B node export_runtime_binary.js bullets_update.schema.yml build/hero_bullets_update.bin >CON 2>CON
        start /B node export_imhex_pattern.js bullets_update.schema.yml generated/bullets_update.hexpat >CON 2>CON

        start /B node export_c_header.js bullets_draw.schema.yml generated/bullets_draw.h >CON 2>CON

        start /B node export_c_header.js enemy_instances_draw.schema.yml generated/enemy_instances_draw.h >CON 2>CON

        start /B node export_c_header.js game_state.schema.yml generated/game_state.h >CON 2>CON
        start /B node export_runtime_binary.js game_state.schema.yml build/game_state.bin >CON 2>CON
        start /B node export_imhex_pattern.js game_state.schema.yml generated/game_state.hexpat >CON 2>CON

        start /B node export_c_header.js bullet_source_instances.schema.yml generated/bullet_source_instances.h >CON 2>CON

        start /B node export_c_header.js wave_update.schema.yml generated/wave_update.h >CON 2>CON
        start /B node export_runtime_binary.js wave_update.schema.yml build/wave_update.bin >CON 2>CON
        start /B node export_imhex_pattern.js wave_update.schema.yml generated/wave_update.hexpat >CON 2>CON

        start /B node export_c_header.js level_update.schema.yml generated/level_update.h >CON 2>CON
        start /B node export_runtime_binary.js level_update.schema.yml build/level_update.bin >CON 2>CON
        start /B node export_imhex_pattern.js level_update.schema.yml generated/level_update.hexpat >CON 2>CON

        start /B node export_c_header.js play_area.schema.yml generated/play_area.h >CON 2>CON

        start /B node export_c_header.js play_clock.schema.yml generated/play_clock.h >CON 2>CON

        start /B node export_c_header.js collision_source_instances.schema.yml generated/collision_source_instances.h >CON 2>CON
        start /B node export_c_header.js collision_source_types.schema.yml generated/collision_source_types.h >CON 2>CON

        start /B node export_c_header.js collision_grid.schema.yml generated/collision_grid.h >CON 2>CON
        start /B node export_runtime_binary.js collision_grid.schema.yml build/collision_grid.bin >CON 2>CON
        start /B node export_imhex_pattern.js collision_grid.schema.yml generated/collision_grid.hexpat >CON 2>CON

    ) | pause

	pushd build

	where /q cl && (
		call cl -Zi -W4 -wd4201 -wd4100 -wd4505 -wd4127 -nologo /I..\generated ..\win32_main.c -Fewin32_d3d11_dm.exe
		call cl -O2 -Zi -W4 -wd4201 -wd4100 -wd4505 -wd4127 -nologo /I..\generated ..\win32_main.c -Fewin32_d3d11_rm.exe -Fmwin32_d3d11_rm.map /DNDEBUG
	)

	popd

	endlocal
)