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

    u16 *a_damage_count_prt = CollisionDamageADamageCountPrt(collision_damage_bin);
    CollisionDamageADamage *collision_damage_a_damage_sheet = CollisionDamageADamagePrt(collision_damage_bin);
    v2 *a_damage_position_prt = (v2*)CollisionDamageADamagePositionPrt(collision_damage_bin, collision_damage_a_damage_sheet);
    u16 *a_damage_value_prt = CollisionDamageADamageValuePrt(collision_damage_bin, collision_damage_a_damage_sheet);
    float *a_damage_time_prt = CollisionDamageADamageTimePrt(collision_damage_bin, collision_damage_a_damage_sheet);
    
    u16 *b_damage_count_prt = CollisionDamageBDamageCountPrt(collision_damage_bin);
    CollisionDamageBDamage *collision_damage_b_damage_sheet = CollisionDamageBDamagePrt(collision_damage_bin);
    v2 *b_damage_position_prt = (v2*)CollisionDamageBDamagePositionPrt(collision_damage_bin, collision_damage_b_damage_sheet);
    u16 *b_damage_value_prt = CollisionDamageBDamageValuePrt(collision_damage_bin, collision_damage_b_damage_sheet);
    float *b_damage_time_prt = CollisionDamageBDamageTimePrt(collision_damage_bin, collision_damage_b_damage_sheet);

    //u16 *a_source_instances_count_prt = CollisionSourceInstancesSourceInstancesCountPrt(a_collision_source_instances_bin);
    CollisionSourceInstancesSourceInstances *a_collision_source_instances_source_instances_sheet = CollisionSourceInstancesSourceInstancesPrt(a_collision_source_instances_bin);
    u8 *a_source_instances_source_type_index_prt = CollisionSourceInstancesSourceInstancesSourceTypeIndexPrt(a_collision_source_instances_bin, a_collision_source_instances_source_instances_sheet);
    v2 *a_source_instances_positions_prt = (v2*)CollisionSourceInstancesSourceInstancesPositionsPrt(a_collision_source_instances_bin, a_collision_source_instances_source_instances_sheet);
    //u64 *a_source_instances_enabled_prt = CollisionSourceInstancesSourceInstancesEnabledPrt(a_collision_source_instances_bin);

    //u16 *b_source_instances_count_prt = CollisionSourceInstancesSourceInstancesCountPrt(b_collision_source_instances_bin);
    CollisionSourceInstancesSourceInstances *b_collision_source_instances_source_instances_sheet = CollisionSourceInstancesSourceInstancesPrt(b_collision_source_instances_bin);
    u8 *b_source_instances_source_type_index_prt = CollisionSourceInstancesSourceInstancesSourceTypeIndexPrt(b_collision_source_instances_bin, b_collision_source_instances_source_instances_sheet);
    v2 *b_source_instances_positions_prt = (v2*)CollisionSourceInstancesSourceInstancesPositionsPrt(b_collision_source_instances_bin, b_collision_source_instances_source_instances_sheet);
    //u64 *b_source_instances_enabled_prt = CollisionSourceInstancesSourceInstancesEnabledPrt(b_collision_source_instances_bin);    

    CollisionGridGridRowCount *a_grid_row_count_prt = CollisionGridGridRowCountPrt(a_collision_grid_bin);
    CollisionGridGridRows *a_grid_rows_prt = CollisionGridGridRowsPrt(a_collision_grid_bin);

    CollisionGridGridRowCount *b_grid_row_count_prt = CollisionGridGridRowCountPrt(b_collision_grid_bin);
    CollisionGridGridRows *b_grid_rows_prt = CollisionGridGridRowsPrt(b_collision_grid_bin);

    //u16 *a_radius_source_types_count_prt = CollisionSourceRadiusSourceTypesCountPrt(a_collision_source_radius_bin);
    CollisionSourceRadiusSourceTypes *a_collision_source_radius_source_types_sheet = CollisionSourceRadiusSourceTypesPrt(a_collision_source_radius_bin);
    u8 *a_source_types_radius_q8_prt = CollisionSourceRadiusSourceTypesRadiusQ8Prt(a_collision_source_radius_bin, a_collision_source_radius_source_types_sheet);
    u8 *a_source_types_radius_q4_prt = CollisionSourceRadiusSourceTypesRadiusQ4Prt(a_collision_source_radius_bin, a_collision_source_radius_source_types_sheet);

    u8 *a_collision_source_radius_q = a_source_types_radius_q4_prt ? a_source_types_radius_q4_prt : a_source_types_radius_q8_prt;
    f32 a_radius_multiplier = a_source_types_radius_q4_prt ? kQ4ToFloat : kQ8ToFloat;

    //u16 *b_radius_source_types_count_prt = CollisionSourceRadiusSourceTypesCountPrt(b_collision_source_radius_bin);
    CollisionSourceRadiusSourceTypes *b_collision_source_radius_source_types_sheet = CollisionSourceRadiusSourceTypesPrt(b_collision_source_radius_bin);
    u8 *b_source_types_radius_q8_prt = CollisionSourceRadiusSourceTypesRadiusQ8Prt(b_collision_source_radius_bin, b_collision_source_radius_source_types_sheet);
    u8 *b_source_types_radius_q4_prt = CollisionSourceRadiusSourceTypesRadiusQ4Prt(b_collision_source_radius_bin, b_collision_source_radius_source_types_sheet);

    u8 *b_collision_source_radius_q = b_source_types_radius_q4_prt ? b_source_types_radius_q4_prt : b_source_types_radius_q8_prt;
    f32 b_radius_multiplier = b_source_types_radius_q4_prt ? kQ4ToFloat : kQ8ToFloat;

    //u16 *a_damage_source_types_count_prt = CollisionSourceDamageSourceTypesCountPrt(a_collision_source_damage_bin);
    CollisionSourceDamageSourceTypes *a_collision_source_damage_source_types_sheet = CollisionSourceDamageSourceTypesPrt(a_collision_source_damage_bin);
    u16 *a_source_types_damage_prt = CollisionSourceDamageSourceTypesDamagePrt(a_collision_source_damage_bin, a_collision_source_damage_source_types_sheet);

    //u16 *b_damage_source_types_count_prt = CollisionSourceDamageSourceTypesCountPrt(b_collision_source_damage_bin);
    CollisionSourceDamageSourceTypes *b_collision_source_damage_source_types_sheet = CollisionSourceDamageSourceTypesPrt(b_collision_source_damage_bin);
    u16 *b_source_types_damage_prt = CollisionSourceDamageSourceTypesDamagePrt(b_collision_source_damage_bin, b_collision_source_damage_source_types_sheet);

    float damage_time = *LevelUpdateTimePrt(level_update_bin);

    for (u8 row_grid_index = 0; row_grid_index < kCollisionGridRowCount; row_grid_index++)
    {
        u8 a_row_count = a_grid_row_count_prt->GridRowCount[row_grid_index];
        u8 b_row_count = b_grid_row_count_prt->GridRowCount[row_grid_index];

        for (u8 a_col_index = 0; a_col_index < a_row_count; a_col_index++)
        {
            u8 a_source_instance_index = a_grid_rows_prt->GridRows[(row_grid_index * kCollisionGridColCount) + a_col_index];
            u8 a_source_type_index = a_source_instances_source_type_index_prt[a_source_instance_index];
            v2 a_source_instances_position = a_source_instances_positions_prt[a_source_instance_index];

            u8 a_instance_radius_q = a_collision_source_radius_q[a_source_type_index];
            f32 a_instance_radius = ((f32)a_instance_radius_q) * a_radius_multiplier;

            u16 a_source_damage = a_source_types_damage_prt[a_source_type_index];

            for (u8 b_col_index = 0; b_col_index < b_row_count; b_col_index++)
            {
                u8 b_source_instance_index = b_grid_rows_prt->GridRows[(row_grid_index * kCollisionGridColCount) + b_col_index];
                u8 b_source_type_index = b_source_instances_source_type_index_prt[b_source_instance_index];
                v2 b_source_instances_position = b_source_instances_positions_prt[b_source_instance_index];

                u8 b_instance_radius_q = b_collision_source_radius_q[b_source_type_index];
                f32 b_instance_radius = ((f32)b_instance_radius_q) * b_radius_multiplier;

                u16 b_source_damage = b_source_types_damage_prt[b_source_type_index];

                v2 v_ab = v2_sub(b_source_instances_position, a_source_instances_position);
                f32 v_ab_length = v2_length(v_ab);

                if (v_ab_length < (a_instance_radius + b_instance_radius))
                {
                    v2 v_a_damage = v2_scale(v_ab, a_instance_radius/v_ab_length);
                    v2 v_b_damage = v2_scale(v_ab, b_instance_radius/v_ab_length);

                    v2 a_damage_position = v2_add(a_source_instances_position, v_a_damage);
                    v2 b_damage_position = v2_sub(b_source_instances_position, v_b_damage);

                    u16 a_damage_index = (*a_damage_count_prt) % kCollisionDamageMaxDamageCount;
                    u16 b_damage_index = (*b_damage_count_prt) % kCollisionDamageMaxDamageCount;

                    a_damage_value_prt[a_damage_index] = b_source_damage;
                    b_damage_value_prt[b_damage_index] = a_source_damage;

                    a_damage_time_prt[a_damage_index] = damage_time;
                    b_damage_time_prt[b_damage_index] = damage_time;

                    a_damage_position_prt[a_damage_index] = a_damage_position;
                    b_damage_position_prt[b_damage_index] = b_damage_position;

                    (*a_damage_count_prt)++;
                    (*b_damage_count_prt)++;
                }
            }
        }
    }
}