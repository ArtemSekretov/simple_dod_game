
static void
enemy_instances_draw(EnemyInstancesDrawContext *context)
{
    FrameData *frame_data                        = context->FrameDataBin;
    EnemyInstances *enemy_instances              = context->EnemyInstancesBin;
    EnemyInstancesWave *enemy_instances_wave = EnemyInstancesEnemyInstancesWaveMapPrt(enemy_instances);

    EnemyInstancesEnemyTypes *enemy_sheet = EnemyInstancesEnemyTypesPrt(enemy_instances);

    EnemyInstancesWaveEnemyInstances *enemy_instances_wave_sheet  = EnemyInstancesWaveEnemyInstancesPrt(enemy_instances_wave);

    u8 *enemy_instances_enemy_index = EnemyInstancesWaveEnemyInstancesEnemyIndexPrt(enemy_instances_wave, enemy_instances_wave_sheet);

    u8 *enemy_radius_q4 = EnemyInstancesEnemyTypesRadiusQ4Prt(enemy_instances, enemy_sheet);

    FrameDataFrameData *frame_data_sheet = FrameDataFrameDataPrt(frame_data);
    FrameDataFrameDataObjectData *object_data_column = FrameDataFrameDataObjectDataPrt(frame_data, frame_data_sheet);

    u16 *frame_data_count_ptr = FrameDataFrameDataCountPrt(frame_data);

    EnemyInstancesEnemyPositions *enemy_instances_positions_sheet = EnemyInstancesEnemyPositionsPrt(enemy_instances);
    v2 *enemy_instances_positions                                 = (v2 *)EnemyInstancesEnemyPositionsPositionsPrt(enemy_instances, enemy_instances_positions_sheet);

    u16 enemy_positions_count = *EnemyInstancesEnemyPositionsCountPrt(enemy_instances);
    u64 enemy_instances_live  = *EnemyInstancesInstancesLivePrt(enemy_instances);

    for (u8 wave_instance_index = 0; wave_instance_index < enemy_positions_count; wave_instance_index++)
    {
        if ((enemy_instances_live & (1ULL << wave_instance_index)) == 0)
        {
            continue;
        }

        u8 flat_enemy_variation_index = enemy_instances_enemy_index[wave_instance_index];

        u8 radius_q4 = enemy_radius_q4[flat_enemy_variation_index];
        f32 radius = ((f32)radius_q4) * kQ4ToFloat;

        v2 enemy_instance_position = enemy_instances_positions[wave_instance_index];

        u16 frame_data_count = *frame_data_count_ptr;
        FrameDataFrameDataObjectData *object_data = object_data_column + frame_data_count;

        object_data->PositionAndScale[0] = enemy_instance_position.x;
        object_data->PositionAndScale[1] = enemy_instance_position.y;
        object_data->PositionAndScale[2] = radius * 2.0f;

        object_data->Color[0] = 0.0f;
        object_data->Color[1] = 1.0f;
        object_data->Color[2] = 1.0f;

        *frame_data_count_ptr = (frame_data_count + 1) % kFrameDataMaxObjectDataCapacity;
    }
}