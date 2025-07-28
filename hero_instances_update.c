static void
hero_instances_update(HeroInstancesContext *context)
{
    HeroInstances *hero_instances = context->Root;

    u64 *hero_instances_live_ptr = HeroInstancesInstancesLivePrt(hero_instances);
    u16 *hero_instances_count_ptr = HeroInstancesHeroInstancesCountPrt(hero_instances);

    *hero_instances_live_ptr = 1;
    *hero_instances_count_ptr = 1;
}