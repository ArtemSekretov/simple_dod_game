static void
wave_update(WaveUpdateContext *context)
{
    EnemyInstancesUpdate *enemy_instances_update = context->EnemyInstancesUpdateBin;
    WaveUpdate *wave_update                      = context->Root;
    GameState *game_state                        = context->GameStateBin;

    u8 *wave_index_ptr = WaveUpdateWaveIndexPrt(wave_update);
    f32 *wave_time_ptr = WaveUpdateWaveTimePrt(wave_update);

    f32 time_delta = *GameStateTimeDeltaPrt(game_state);

    u64 enemy_instances_live = *EnemyInstancesUpdateInstancesLivePrt(enemy_instances_update);
    u32 wave_state           = *EnemyInstancesUpdateWaveStatePrt(enemy_instances_update);

    *wave_time_ptr += time_delta;

    if ((wave_state & kEnemyInstancesUpdateWaveSpawnedAll) == 0)
    {
        return;
    }

    if (enemy_instances_live)
    {
        return;
    }

    (*wave_index_ptr)++;
    *wave_time_ptr = 0.0f;
}