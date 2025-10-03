
static void
bullets_draw(BulletsDrawContext *context)
{
    BulletsUpdate *bullets_update = context->BulletsUpdateBin;
    Bullets *bullets              = context->BulletsBin;
    FrameData *frame_data         = context->FrameDataBin;

    FrameDataFrameData *frame_data_sheet = FrameDataFrameDataPrt(frame_data);
    FrameDataFrameDataObjectData *object_data_column = FrameDataFrameDataObjectDataPrt(frame_data, frame_data_sheet);

    u16 *frame_data_count_ptr   = FrameDataFrameDataCountPrt(frame_data);

    BulletsBulletTypes *bullet_types_sheet = BulletsBulletTypesPrt(bullets);

    u8 *bullet_types_radius_q8  = BulletsBulletTypesRadiusQ8Prt(bullets, bullet_types_sheet);
    u32 spawn_count        = *BulletsUpdateSpawnCountPrt(bullets_update);

    s32 draw_count = min(kBulletsUpdateSourceBulletsMaxInstanceCount, spawn_count);
    
    BulletsUpdateBulletPositions *bullet_update_positions_sheet = BulletsUpdateBulletPositionsPrt(bullets_update);
    v2 *bullets_positions                                       = (v2 *)BulletsUpdateBulletPositionsCurrentPositionPrt(bullets_update, bullet_update_positions_sheet);
    uint8_t *bullets_type_index                                 = BulletsUpdateBulletPositionsTypeIndexPrt(bullets_update, bullet_update_positions_sheet);

    for (s32 i = 0; i < draw_count; i++)
    {
        v2 bullet_position   = bullets_positions[i];
        u8 bullet_type_index = bullets_type_index[i];

        u8 bullet_radius_q8 = bullet_types_radius_q8[bullet_type_index];
        f32 bullet_radius   = ((f32)bullet_radius_q8) * kQ8ToFloat;

        if ((fabsf(bullet_position.x) - bullet_radius) > kPlayAreaHalfWidth)
        {
            continue;
        }

        if ((fabsf(bullet_position.y) - bullet_radius) > kPlayAreaHalfHeight)
        {
            continue;
        }

        u16 frame_data_count = (*frame_data_count_ptr) % kFrameDataMaxObjectDataCapacity;

        FrameDataFrameDataObjectData *object_data = object_data_column + frame_data_count;

        object_data->PositionAndScale[0] = bullet_position.x;
        object_data->PositionAndScale[1] = bullet_position.y;
        object_data->PositionAndScale[2] = bullet_radius;

        object_data->Color[0] = 1.0f;
        object_data->Color[1] = 0.0f;
        object_data->Color[2] = 0.0f;

        (*frame_data_count_ptr)++;
    }
}