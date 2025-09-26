static void
collision_grid_update(CollisionGridContext *context)
{
    CollisionGrid *collision_grid = context->Root;
    CollisionSourceInstances *collision_source_instances_bin = context->CollisionSourceInstancesBin;
    CollisionSourceTypes *collision_source_types_bin = context->CollisionSourceTypesBin;

    u16 collision_source_instances_count = *CollisionSourceInstancesSourceInstancesCountPrt(collision_source_instances_bin);
    u64* collision_source_instances_enabled = CollisionSourceInstancesSourceInstancesEnabledPrt(collision_source_instances_bin);

    CollisionSourceInstancesSourceInstances *collision_source_instances_sheet = CollisionSourceInstancesSourceInstancesPrt(collision_source_instances_bin);
    u8 *collision_source_types = CollisionSourceInstancesSourceInstancesSourceTypeIndexPrt(collision_source_instances_bin, collision_source_instances_sheet);
    v2 *collision_source_positions = (v2*)CollisionSourceInstancesSourceInstancesPositionsPrt(collision_source_instances_bin, collision_source_instances_sheet);

    CollisionSourceTypesSourceTypes *collision_source_types_sheet = CollisionSourceTypesSourceTypesPrt(collision_source_types_bin);
    u8 *collision_source_types_radius_q8 = CollisionSourceTypesSourceTypesRadiusQ8Prt(collision_source_types_bin, collision_source_types_sheet);
    u8 *collision_source_types_radius_q4 = CollisionSourceTypesSourceTypesRadiusQ4Prt(collision_source_types_bin, collision_source_types_sheet);

    u8 *collision_source_types_radius_q = collision_source_types_radius_q4 ? collision_source_types_radius_q4 : collision_source_types_radius_q8;
    f32 radius_multiplier = collision_source_types_radius_q4 ? kQ4ToFloat : kQ8ToFloat;

    CollisionGridGridRowCount *collision_grid_row_count = CollisionGridGridRowCountPrt(collision_grid);
    CollisionGridGridRows *collision_grid_rows = CollisionGridGridRowsPrt(collision_grid);

    memset(collision_grid_row_count, 0, sizeof(CollisionGridGridRowCount));
    memset(collision_grid_rows, 0, sizeof(CollisionGridGridRows));

    for (u16 instance_index = 0; instance_index < collision_source_instances_count; instance_index++)
    {
        if (collision_source_instances_enabled)
        {
            u16 instance_word_index = instance_index / 64;
            u16 instance_bit_index = instance_index - (instance_word_index * 64);

            if ((collision_source_instances_enabled[instance_word_index] & (1ULL << instance_bit_index)) == 0)
            {
                continue;
            }
        }

        v2 instance_position = collision_source_positions[instance_index];
        u8 instance_type = collision_source_types[instance_index];
        u8 instance_radius_q = collision_source_types_radius_q[instance_type];
        f32 instance_radius = ((f32)instance_radius_q) * radius_multiplier;

        s32 top = clamp_s32(0, (s32)((0.5f + ((instance_position.y + instance_radius) / kPlayAreaHeight)) * kCollisionGridRowCount), kCollisionGridRowCount - 1);
        s32 bottom = clamp_s32(0, (s32)((0.5f + ((instance_position.y - instance_radius) / kPlayAreaHeight)) * kCollisionGridRowCount), kCollisionGridRowCount - 1);

        if ((instance_position.y + instance_radius) < -kPlayAreaHalfHeight)
        {
            continue;
        }
        if ((instance_position.y - instance_radius) > kPlayAreaHalfHeight)
        {
            continue;
        }
        if ((instance_position.x + instance_radius) < -kPlayAreaHalfWidth)
        {
            continue;
        }
        if ((instance_position.x - instance_radius) > kPlayAreaHalfWidth)
        {
            continue;
        }

        for (s32 y = top; y >= bottom; y--)
        {
            s32 row_index = (kCollisionGridRowCount - 1) - y;
            u8 row_instance_index = collision_grid_row_count->GridRowCount[row_index];
            collision_grid_row_count->GridRowCount[row_index]++;

            collision_grid_rows->GridRows[(row_index * kCollisionGridColCount) + row_instance_index] = (u8)instance_index;
        }
    }
}