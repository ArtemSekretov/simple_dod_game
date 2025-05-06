
void
enemy_instances_draw(FrameData *frame_data)
{
    u64 enemy_instances_live = g_enemy_instances_live;
    f32 radius = 0.5f;

    s32 array_count = ArrayCount(g_enemy_instances_positions);
   
    Assert(array_count == kMaxInstancesPerWave);

    FrameDataFrameData *frame_data_sheet = FrameDataFrameDataPrt(frame_data);
    FrameDataFrameDataObjectData *object_data_column = FrameDataFrameDataObjectDataPrt(frame_data, frame_data_sheet);

    for (s32 i = 0; i < array_count; i++)
    {
        if ((enemy_instances_live & (1ULL << i)) == 0)
        {
            continue;
        }

        v2 enemy_instance_position = g_enemy_instances_positions[i];

        u16 object_index = frame_data->FrameDataCount;
        object_index = object_index % kMaxObjectDataCapacity;

        FrameDataFrameDataObjectData *object_data = object_data_column + object_index;

        object_data->PositionAndScale[0] = enemy_instance_position.x;
        object_data->PositionAndScale[1] = enemy_instance_position.y;
        object_data->PositionAndScale[2] = radius;

        object_data->Color[0] = 1.0f;
        object_data->Color[1] = 0.0f;
        object_data->Color[2] = 0.0f;

        frame_data->FrameDataCount += 1;

    }
}