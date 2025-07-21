static void
wave_update(WaveUpdateContext *context)
{
    EnemyInstancesUpdate *enemy_instances_update = context->EnemyInstancesUpdateBin;
    WaveUpdate *wave_update                      = context->Root;
    GameState *game_state                        = context->GameStateBin;

    wave_update->WaveTime += game_state->TimeDelta;

    if ((enemy_instances_update->WaveState & kEnemyInstancesUpdateWaveSpawnedAll) == 0)
    {
        return;
    }

    if (enemy_instances_update->InstancesLive)
    {
        return;
    }

    wave_update->WaveIndex++;
    wave_update->WaveTime = 0.0f;
}