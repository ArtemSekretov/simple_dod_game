static void
level_update(LevelUpdateContext *context)
{
    EnemyInstances *enemy_instances = context->EnemyInstancesBin;
    LevelUpdate *level_update       = context->Root;
    GameState *game_state           = context->GameStateBin;

    u8 *level_index_ptr  = LevelUpdateIndexPrt(level_update);
    f32 *level_time_ptr  = LevelUpdateTimePrt(level_update);
    u32 *level_state_ptr = LevelUpdateStatePrt(level_update);

    f32 time_delta       = *GameStateTimeDeltaPrt(game_state);
    u32 game_state_state = *GameStateStatePrt(game_state);

    u32 enemy_instances_wave_state = *EnemyInstancesWaveStatePrt(enemy_instances);

    if (game_state_state & kGameStateStateReset)
    {
        *level_index_ptr = 0;
        *level_time_ptr  = 0.0f;
        *level_state_ptr |= kLevelUpdateStateReset;
        return;
    }

    *level_time_ptr += time_delta;
    *level_state_ptr &= ~kLevelUpdateStateReset;

    if ((enemy_instances_wave_state & kEnemyInstancesAllWavesComplete) == 0)
    {
        return;
    }

    (*level_index_ptr)++;
    *level_time_ptr = 0.0f;
    *level_state_ptr |= kLevelUpdateStateReset;
}