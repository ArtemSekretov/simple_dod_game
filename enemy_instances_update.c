
v2  g_enemy_instances_positions[64];
u64 g_enemy_instances_live;

void
enemy_instances_update(f64 time)
{
    g_enemy_instances_live = 1;
    g_enemy_instances_positions[0] = V2(-cosf((f32)time) + sinf((f32)time), sinf((f32)time) + cosf((f32)time));
}
