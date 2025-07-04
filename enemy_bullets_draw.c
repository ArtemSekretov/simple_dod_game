
static void
enemy_bullets_draw(EnemyBulletsDrawContext *context)
{
    EnemyBulletsUpdate *enemy_bullets_update = context->EnemyBulletsUpdateBin;
    EnemyBullets *enemy_bullets              = context->EnemyBulletsBin;
    FrameData *frame_data                    = context->FrameDataBin;

    FrameDataFrameData *frame_data_sheet = FrameDataFrameDataPrt(frame_data);
    FrameDataFrameDataObjectData *object_data_column = FrameDataFrameDataObjectDataPrt(frame_data, frame_data_sheet);

    EnemyBulletsBulletTypes *enemy_bullet_types_sheet = EnemyBulletsBulletTypesPrt(enemy_bullets);

    u8 *enemy_bullet_types_radius_q8         = EnemyBulletsBulletTypesRadiusQ8Prt(enemy_bullets, enemy_bullet_types_sheet);
   
    s32 draw_count = min(kEnemyBulletsUpdateEnemyBulletsMaxInstanceCount, enemy_bullets_update->WaveSpawnCount);
    
    EnemyBulletsUpdateBulletPositions *enemy_bullet_update_positions_sheet = EnemyBulletsUpdateBulletPositionsPrt(enemy_bullets_update);
    v2 *enemy_bullets_positions                                            = (v2 *)EnemyBulletsUpdateBulletPositionsCurrentPositionPrt(enemy_bullets_update, enemy_bullet_update_positions_sheet);
    uint8_t *enemy_bullets_type_index                                      = EnemyBulletsUpdateBulletPositionsTypeIndexPrt(enemy_bullets_update, enemy_bullet_update_positions_sheet);

    for (s32 i = 0; i < draw_count; i++)
    {
        v2 bullet_position   = enemy_bullets_positions[i];
        u8 bullet_type_index = enemy_bullets_type_index[i];

        u8 bullet_radius_q8 = enemy_bullet_types_radius_q8[bullet_type_index];
        f32 bullet_radius   = ((f32)bullet_radius_q8) * kQ8ToFloat;

        if ((fabsf(bullet_position.x) - bullet_radius) > kEnemyInstancesHalfWidth)
        {
            continue;
        }

        if ((fabsf(bullet_position.y) - bullet_radius) > kEnemyInstancesHalfHeight)
        {
            continue;
        }

        FrameDataFrameDataObjectData *object_data = object_data_column + frame_data->FrameDataCount;

        object_data->PositionAndScale[0] = bullet_position.x;
        object_data->PositionAndScale[1] = bullet_position.y;
        object_data->PositionAndScale[2] = bullet_radius * 2.0f;

        object_data->Color[0] = 1.0f;
        object_data->Color[1] = 0.0f;
        object_data->Color[2] = 0.0f;

        frame_data->FrameDataCount = (frame_data->FrameDataCount + 1) % kFrameDataMaxObjectDataCapacity;

    }
}