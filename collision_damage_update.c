static void
collision_damage_update(CollisionDamageContext *context)
{
    CollisionDamage *collision_damage_bin = context->Root;
    CollisionGrid *a_collision_grid_bin = context->ACollisionGridBin;
    CollisionSourceInstances *a_collision_source_instances_bin = context->ACollisionSourceInstancesBin;
    CollisionSourceRadius *a_collision_source_radius_bin = context->ACollisionSourceRadiusBin;
    CollisionSourceDamage *a_collision_source_damage_bin = context->ACollisionSourceDamageBin;
    CollisionGrid *b_collision_grid_bin = context->BCollisionGridBin;
    CollisionSourceInstances *b_collision_source_instances_bin = context->BCollisionSourceInstancesBin;
    CollisionSourceRadius *b_collision_source_radius_bin = context->BCollisionSourceRadiusBin;
    CollisionSourceDamage *b_collision_source_damage_bin = context->BCollisionSourceDamageBin;
    LevelUpdate *level_update_bin = context->LevelUpdateBin;

    u16 *damage_events_count_prt = CollisionDamageDamageEventsCountPrt(collision_damage_bin);
    CollisionDamageDamageEvents *collision_damage_damage_events_sheet = CollisionDamageDamageEventsPrt(collision_damage_bin);
    v2 *a_damage_position_prt = (v2*)CollisionDamageDamageEventsAPositionPrt(collision_damage_bin, collision_damage_damage_events_sheet);
    v2 *b_damage_position_prt = (v2*)CollisionDamageDamageEventsBPositionPrt(collision_damage_bin, collision_damage_damage_events_sheet);
    u16 *a_damage_value_prt = CollisionDamageDamageEventsAValuePrt(collision_damage_bin, collision_damage_damage_events_sheet);
    u16 *b_damage_value_prt = CollisionDamageDamageEventsBValuePrt(collision_damage_bin, collision_damage_damage_events_sheet);
    float *damage_time_prt = CollisionDamageDamageEventsTimePrt(collision_damage_bin, collision_damage_damage_events_sheet);
    u8 *a_damage_source_instance_index_prt = CollisionDamageDamageEventsASourceInstanceIndexPrt(collision_damage_bin, collision_damage_damage_events_sheet);
    u8 *b_damage_source_instance_index_prt = CollisionDamageDamageEventsBSourceInstanceIndexPrt(collision_damage_bin, collision_damage_damage_events_sheet);

    CollisionSourceInstancesSourceInstances *a_collision_source_instances_source_instances_sheet = CollisionSourceInstancesSourceInstancesPrt(a_collision_source_instances_bin);
    u8 *a_source_instances_source_type_index_prt = CollisionSourceInstancesSourceInstancesSourceTypeIndexPrt(a_collision_source_instances_bin, a_collision_source_instances_source_instances_sheet);
    v2 *a_source_instances_positions_prt = (v2*)CollisionSourceInstancesSourceInstancesPositionsPrt(a_collision_source_instances_bin, a_collision_source_instances_source_instances_sheet);

    CollisionSourceInstancesSourceInstances *b_collision_source_instances_source_instances_sheet = CollisionSourceInstancesSourceInstancesPrt(b_collision_source_instances_bin);
    u8 *b_source_instances_source_type_index_prt = CollisionSourceInstancesSourceInstancesSourceTypeIndexPrt(b_collision_source_instances_bin, b_collision_source_instances_source_instances_sheet);
    v2 *b_source_instances_positions_prt = (v2*)CollisionSourceInstancesSourceInstancesPositionsPrt(b_collision_source_instances_bin, b_collision_source_instances_source_instances_sheet);

    CollisionGridGridRowCount *a_grid_row_count_prt = CollisionGridGridRowCountPrt(a_collision_grid_bin);
    CollisionGridGridRows *a_grid_rows_prt = CollisionGridGridRowsPrt(a_collision_grid_bin);

    CollisionGridGridRowCount *b_grid_row_count_prt = CollisionGridGridRowCountPrt(b_collision_grid_bin);
    CollisionGridGridRows *b_grid_rows_prt = CollisionGridGridRowsPrt(b_collision_grid_bin);

    CollisionSourceRadiusSourceTypes *a_collision_source_radius_source_types_sheet = CollisionSourceRadiusSourceTypesPrt(a_collision_source_radius_bin);
    u8 *a_source_types_radius_q8_prt = CollisionSourceRadiusSourceTypesRadiusQ8Prt(a_collision_source_radius_bin, a_collision_source_radius_source_types_sheet);
    u8 *a_source_types_radius_q4_prt = CollisionSourceRadiusSourceTypesRadiusQ4Prt(a_collision_source_radius_bin, a_collision_source_radius_source_types_sheet);

    u8 *a_collision_source_radius_q = a_source_types_radius_q4_prt ? a_source_types_radius_q4_prt : a_source_types_radius_q8_prt;
    f32 a_radius_multiplier = a_source_types_radius_q4_prt ? kQ4ToFloat : kQ8ToFloat;

    CollisionSourceRadiusSourceTypes *b_collision_source_radius_source_types_sheet = CollisionSourceRadiusSourceTypesPrt(b_collision_source_radius_bin);
    u8 *b_source_types_radius_q8_prt = CollisionSourceRadiusSourceTypesRadiusQ8Prt(b_collision_source_radius_bin, b_collision_source_radius_source_types_sheet);
    u8 *b_source_types_radius_q4_prt = CollisionSourceRadiusSourceTypesRadiusQ4Prt(b_collision_source_radius_bin, b_collision_source_radius_source_types_sheet);

    u8 *b_collision_source_radius_q = b_source_types_radius_q4_prt ? b_source_types_radius_q4_prt : b_source_types_radius_q8_prt;
    f32 b_radius_multiplier = b_source_types_radius_q4_prt ? kQ4ToFloat : kQ8ToFloat;

    CollisionSourceDamageSourceTypes *a_collision_source_damage_source_types_sheet = CollisionSourceDamageSourceTypesPrt(a_collision_source_damage_bin);
    u16 *a_source_types_damage_prt = CollisionSourceDamageSourceTypesDamagePrt(a_collision_source_damage_bin, a_collision_source_damage_source_types_sheet);

    CollisionSourceDamageSourceTypes *b_collision_source_damage_source_types_sheet = CollisionSourceDamageSourceTypesPrt(b_collision_source_damage_bin);
    u16 *b_source_types_damage_prt = CollisionSourceDamageSourceTypesDamagePrt(b_collision_source_damage_bin, b_collision_source_damage_source_types_sheet);

    f32 damage_time = *LevelUpdateTimePrt(level_update_bin);

    u16 a_source_instances_count = *CollisionSourceInstancesSourceInstancesCountPrt(a_collision_source_instances_bin);
    u16 b_source_instances_count = *CollisionSourceInstancesSourceInstancesCountPrt(b_collision_source_instances_bin);

    u64 *a_source_instances_reset_prt = CollisionSourceInstancesSourceInstancesResetPrt(a_collision_source_instances_bin);
    u64 *b_source_instances_reset_prt = CollisionSourceInstancesSourceInstancesResetPrt(b_collision_source_instances_bin);

    CollisionDamageAccumulatedDamage *collision_damage_accumulated_damage_sheet = CollisionDamageAccumulatedDamagePrt(collision_damage_bin);
    u16 *accumulated_damage_a_value_prt = CollisionDamageAccumulatedDamageAValuePrt(collision_damage_bin, collision_damage_accumulated_damage_sheet);
    u16 *accumulated_damage_b_value_prt = CollisionDamageAccumulatedDamageBValuePrt(collision_damage_bin, collision_damage_accumulated_damage_sheet);

    for (u16 source_instance_index = 0; source_instance_index < a_source_instances_count; source_instance_index++)
    {
        u16 instance_word_index = source_instance_index / 64;
        u16 instance_bit_index = source_instance_index - (instance_word_index * 64);
        
        b32 is_instance_reset = (a_source_instances_reset_prt[instance_word_index] & (1ULL << instance_bit_index)) != 0;

        if (is_instance_reset)
        {
            accumulated_damage_a_value_prt[source_instance_index] = 0;
        }
    }

    for (u16 source_instance_index = 0; source_instance_index < b_source_instances_count; source_instance_index++)
    {
        u16 instance_word_index = source_instance_index / 64;
        u16 instance_bit_index = source_instance_index - (instance_word_index * 64);
        
        b32 is_instance_reset = (b_source_instances_reset_prt[instance_word_index] & (1ULL << instance_bit_index)) != 0;

        if (is_instance_reset)
        {
            accumulated_damage_b_value_prt[source_instance_index] = 0;
        }
    }

    // up to 256(64 * 4) instances two buffers 
    u64 a_instances_processed[4 * 2] = { 0 };
    u64 b_instances_processed[4 * 2] = { 0 };
    u8 instances_processed_index = 0;

    for (u8 row_grid_index = 0; row_grid_index < kCollisionGridRowCount; row_grid_index++)
    {
        u8 a_row_count = a_grid_row_count_prt->GridRowCount[row_grid_index];
        u8 b_row_count = b_grid_row_count_prt->GridRowCount[row_grid_index];

        u8 *a_grid_row = &a_grid_rows_prt->GridRows[row_grid_index * kCollisionGridColCount];
        u8 *b_grid_row = &b_grid_rows_prt->GridRows[row_grid_index * kCollisionGridColCount];

        u8 prev_instances_processed_offset = instances_processed_index * 4;
        u8 next_instances_processed_offset = (instances_processed_index^1) * 4;

        u64 *a_prev_instances_processed = &a_instances_processed[prev_instances_processed_offset];
        u64 *a_next_instances_processed = &a_instances_processed[next_instances_processed_offset];

        u64 *b_prev_instances_processed = &b_instances_processed[prev_instances_processed_offset];
        u64 *b_next_instances_processed = &b_instances_processed[next_instances_processed_offset];

        a_next_instances_processed[0] = 0;
        a_next_instances_processed[1] = 0;
        a_next_instances_processed[2] = 0;
        a_next_instances_processed[3] = 0;

        b_next_instances_processed[0] = 0;
        b_next_instances_processed[1] = 0;
        b_next_instances_processed[2] = 0;
        b_next_instances_processed[3] = 0;

        for (u8 a_col_index = 0; a_col_index < a_row_count; a_col_index++)
        {
            u8 a_source_instance_index = a_grid_row[a_col_index];
            u8 a_source_type_index = a_source_instances_source_type_index_prt[a_source_instance_index];
            v2 a_source_instances_position = a_source_instances_positions_prt[a_source_instance_index];

            u8 a_instance_radius_q = a_collision_source_radius_q[a_source_type_index];
            f32 a_instance_radius = ((f32)a_instance_radius_q) * a_radius_multiplier;

            u16 a_source_damage = a_source_types_damage_prt[a_source_type_index];

            u16 a_instance_word_index = a_source_instance_index / 64;
            u16 a_instance_bit_index = a_source_instance_index - (a_instance_word_index * 64);

            b32 is_a_processed = a_prev_instances_processed[a_instance_word_index] & (1ULL << a_instance_bit_index);

            a_next_instances_processed[a_instance_word_index] |= (1ULL << a_instance_bit_index);

            for (u8 b_col_index = 0; b_col_index < b_row_count; b_col_index++)
            {
                u8 b_source_instance_index = b_grid_row[b_col_index];
                u8 b_source_type_index = b_source_instances_source_type_index_prt[b_source_instance_index];
                v2 b_source_instances_position = b_source_instances_positions_prt[b_source_instance_index];

                u8 b_instance_radius_q = b_collision_source_radius_q[b_source_type_index];
                f32 b_instance_radius = ((f32)b_instance_radius_q) * b_radius_multiplier;

                u16 b_source_damage = b_source_types_damage_prt[b_source_type_index];

                u16 b_instance_word_index = b_source_instance_index / 64;
                u16 b_instance_bit_index = b_source_instance_index - (b_instance_word_index * 64);

                b32 is_b_processed = b_prev_instances_processed[b_instance_word_index] & (1ULL << b_instance_bit_index);

                b_next_instances_processed[b_instance_word_index] |= (1ULL << b_instance_bit_index);

                v2 v_ab = v2_sub(b_source_instances_position, a_source_instances_position);
                f32 v_ab_length = v2_length(v_ab);

                if (v_ab_length < (a_instance_radius + b_instance_radius))
                {
                    b32 is_pair_processed = is_a_processed && is_b_processed;
                    if (is_pair_processed)
                    {
                        v2 v_a_damage = v2_scale(v_ab, a_instance_radius / v_ab_length);
                        v2 v_b_damage = v2_scale(v_ab, b_instance_radius / v_ab_length);

                        v2 a_damage_position = v2_add(a_source_instances_position, v_a_damage);
                        v2 b_damage_position = v2_sub(b_source_instances_position, v_b_damage);

                        u16 damage_index = (*damage_events_count_prt) % kCollisionDamageMaxDamageEventCount;

                        a_damage_value_prt[damage_index] = b_source_damage;
                        b_damage_value_prt[damage_index] = a_source_damage;

                        damage_time_prt[damage_index] = damage_time;

                        a_damage_position_prt[damage_index] = a_damage_position;
                        b_damage_position_prt[damage_index] = b_damage_position;

                        a_damage_source_instance_index_prt[damage_index] = a_source_instance_index;
                        b_damage_source_instance_index_prt[damage_index] = b_source_instance_index;

                        accumulated_damage_a_value_prt[a_source_instance_index] += b_source_damage;
                        accumulated_damage_b_value_prt[b_source_instance_index] += a_source_damage;

                        (*damage_events_count_prt)++;
                    }
                }
            }
        }

        instances_processed_index ^= 1;
    }
}