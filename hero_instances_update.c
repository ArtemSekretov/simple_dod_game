static void
hero_instances_update(HeroInstancesContext *context)
{
    HeroInstances *hero_instances = context->Root;
    GameState *game_state         = context->GameStateBin;
    CollisionInstancesDamage *collision_instances_damage_bin = context->CollisionInstancesDamageBin;

    HeroInstancesHeroTypes *hero_instances_hero_types_sheet = HeroInstancesHeroTypesPrt(hero_instances);
    u16 *hero_types_health_prt = HeroInstancesHeroTypesHealthPrt(hero_instances, hero_instances_hero_types_sheet);

    CollisionInstancesDamageInstances *collision_instances_damage_instances_sheet = CollisionInstancesDamageInstancesPrt(collision_instances_damage_bin);
    u16 *instances_damage_prt = CollisionInstancesDamageInstancesDamagePrt(collision_instances_damage_bin, collision_instances_damage_instances_sheet);

    v2 world_mouse_position = *((v2 *)GameStateWorldMousePositionPrt(game_state));
    u32 game_state_state = *GameStateStatePrt(game_state);

    u64 *hero_instances_live_ptr  = HeroInstancesInstancesLivePrt(hero_instances);
    u64 *hero_instances_reset_prt = HeroInstancesInstancesResetPrt(hero_instances);
    u16 *hero_instances_count_ptr = HeroInstancesHeroInstancesCountPrt(hero_instances);

    HeroInstancesHeroInstances *hero_instances_sheet = HeroInstancesHeroInstancesPrt(hero_instances);
    v2 *hero_instances_positions = (v2 *)HeroInstancesHeroInstancesPositionsPrt(hero_instances, hero_instances_sheet);

    u8 hero_instance_index = 0;
    u8 hero_type_index = 0;

    if ((game_state_state & kGameStateReset))
    {
        *hero_instances_live_ptr  |= 1ULL << hero_instance_index;
        *hero_instances_reset_prt |= 1ULL << hero_instance_index;
        *hero_instances_count_ptr = 1;

        return;
    }

    *hero_instances_reset_prt &= ~(1ULL << hero_instance_index);

    u16 damage = instances_damage_prt[hero_instance_index];
    u16 health = hero_types_health_prt[hero_type_index];

    if (damage > health)
    {
        *hero_instances_live_ptr &= ~(1ULL << hero_instance_index);
    }

    hero_instances_positions[hero_instance_index] = world_mouse_position;
}