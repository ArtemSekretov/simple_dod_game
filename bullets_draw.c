
static void
bullets_draw(BulletsDrawContext *context)
{
    BulletsUpdate *bullets_update = context->BulletsUpdateBin;
    Bullets *bullets              = context->BulletsBin;
    FrameData *frame_data         = context->FrameDataBin;

    FrameDataFrameData *frame_data_sheet = FrameDataFrameDataPrt(frame_data);
    FrameDataFrameDataObjectData *object_data_column = FrameDataFrameDataObjectDataPrt(frame_data, frame_data_sheet);

    u16 *frame_data_count_ptr = FrameDataFrameDataCountPrt(frame_data);
    u16 frame_data_capacity   = *FrameDataFrameDataCapacityPrt(frame_data);

    BulletsBulletTypes *bullet_types_sheet = BulletsBulletTypesPrt(bullets);

    u8 *bullet_types_radius_q8    = BulletsBulletTypesRadiusQ8Prt(bullets, bullet_types_sheet);

    u16 bullet_positions_count         = *BulletsUpdateBulletPositionsCountPrt(bullets_update);
    u16 bullet_positions_capacity      = *BulletsUpdateBulletPositionsCapacityPrt(bullets_update);

    u16 update_count = min(bullet_positions_count, bullet_positions_capacity);

    BulletsUpdateBulletPositions *bullet_update_positions_sheet = BulletsUpdateBulletPositionsPrt(bullets_update);
    v2 *bullets_positions                                       = (v2 *)BulletsUpdateBulletPositionsCurrentPositionPrt(bullets_update, bullet_update_positions_sheet);
    uint8_t *bullets_type_index                                 = BulletsUpdateBulletPositionsTypeIndexPrt(bullets_update, bullet_update_positions_sheet);

    BulletsUpdateInstancesLive *instances_live_prt = BulletsUpdateInstancesLivePrt(bullets_update);

    for (u32 bullet_instance_index = 0; bullet_instance_index < update_count; bullet_instance_index++)
    {
        u32 bullet_instance_word_index = bullet_instance_index / 64;
        u32 bullet_instance_bit_index = bullet_instance_index - (bullet_instance_word_index * 64);

        if ((instances_live_prt->InstancesLive[bullet_instance_word_index] & (1ULL << bullet_instance_bit_index)) == 0)
        {
            continue;
        }

        v2 bullet_position   = bullets_positions[bullet_instance_index];
        u8 bullet_type_index = bullets_type_index[bullet_instance_index];

        u8 bullet_radius_q8 = bullet_types_radius_q8[bullet_type_index];
        f32 bullet_radius   = ((f32)bullet_radius_q8) * kQ8ToFloat;

        u16 frame_data_count = (*frame_data_count_ptr) % frame_data_capacity;

        FrameDataFrameDataObjectData *object_data = object_data_column + frame_data_count;

        object_data->PositionAndScale[0] = bullet_position.x;
        object_data->PositionAndScale[1] = bullet_position.y;
        object_data->PositionAndScale[2] = bullet_radius;

        object_data->MaterialIndex = 1;

        (*frame_data_count_ptr)++;
    }
}