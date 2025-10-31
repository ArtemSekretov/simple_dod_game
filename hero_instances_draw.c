static void
hero_instances_draw(HeroInstancesDrawContext *context)
{
    HeroInstances *hero_instances = context->HeroInstancesBin;
    FrameData *frame_data         = context->FrameDataBin;

    FrameDataFrameData *frame_data_sheet = FrameDataFrameDataPrt(frame_data);
    FrameDataFrameDataObjectData *object_data_column = FrameDataFrameDataObjectDataPrt(frame_data, frame_data_sheet);

    u16 *frame_data_count_ptr = FrameDataFrameDataCountPrt(frame_data);
    u16 frame_data_capacity   = *FrameDataFrameDataCapacityPrt(frame_data);

    u64 hero_instances_live  = *HeroInstancesInstancesLivePrt(hero_instances);
    u16 hero_instances_count    = *HeroInstancesHeroInstancesCountPrt(hero_instances);
    u16 hero_instances_capacity = *HeroInstancesHeroInstancesCapacityPrt(hero_instances);

    hero_instances_count = min(hero_instances_count, hero_instances_capacity);

    HeroInstancesHeroInstances *hero_instances_sheet = HeroInstancesHeroInstancesPrt(hero_instances);
    v2 *hero_instances_positions                     = (v2 *)HeroInstancesHeroInstancesPositionsPrt(hero_instances, hero_instances_sheet);
    u8 *hero_instances_hero_index                    = HeroInstancesHeroInstancesHeroTypeIndexPrt(hero_instances, hero_instances_sheet);

    HeroInstancesHeroTypes *hero_types_sheet = HeroInstancesHeroTypesPrt(hero_instances);
    u8 *hero_radius_q4 = HeroInstancesHeroTypesRadiusQ4Prt(hero_instances, hero_types_sheet);

    for (u8 wave_instance_index = 0; wave_instance_index < hero_instances_count; wave_instance_index++)
    {
        if ((hero_instances_live & (1ULL << wave_instance_index)) == 0)
        {
            continue;
        }

        u8 flat_hero_variation_index = hero_instances_hero_index[wave_instance_index];

        u8 radius_q4 = hero_radius_q4[flat_hero_variation_index];
        f32 radius = ((f32)radius_q4) * kQ4ToFloat;

        v2 hero_instance_position = hero_instances_positions[wave_instance_index];

        u16 frame_data_count = (*frame_data_count_ptr) % frame_data_capacity;
        FrameDataFrameDataObjectData *object_data = object_data_column + frame_data_count;

        object_data->PositionAndScale[0] = hero_instance_position.x;
        object_data->PositionAndScale[1] = hero_instance_position.y;
        object_data->PositionAndScale[2] = radius;

        object_data->MaterialIndex = 5;

        (*frame_data_count_ptr)++;
    }
}