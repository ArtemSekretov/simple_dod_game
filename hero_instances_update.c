static void
hero_instances_update(HeroInstancesContext *context)
{
    HeroInstances *hero_instances = context->Root;
    GameState *game_state         = context->GameStateBin;

    v2 world_mouse_position = *((v2 *)GameStateWorldMousePositionPrt(game_state));

    u64 *hero_instances_live_ptr = HeroInstancesInstancesLivePrt(hero_instances);
    u16 *hero_instances_count_ptr = HeroInstancesHeroInstancesCountPrt(hero_instances);

    HeroInstancesHeroInstances *hero_instances_sheet = HeroInstancesHeroInstancesPrt(hero_instances);
    v2 *hero_instances_positions = (v2 *)HeroInstancesHeroInstancesPositionsPrt(hero_instances, hero_instances_sheet);

    hero_instances_positions[0] = world_mouse_position;
    *hero_instances_live_ptr = 1;
    *hero_instances_count_ptr = 1;
}