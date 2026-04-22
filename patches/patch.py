import os

with open('src/States/BattleState.cpp', 'r', encoding='utf-8') as f:
    content = f.read()

includes = '#include "../Battle/BattleEvents.h"\n#include "../Events/EventManager.h"\n'
if '#include "../Battle/BattleEvents.h"' not in content:
    content = content.replace('#include "BattleState.h"', '#include "BattleState.h"\n' + includes)

on_enter_mod = "\n    mPlayAnimListener = EventManager::Get().Subscribe(\"battler_play_anim\", [this](const EventData& e){ OnPlayAnim(e); });\n    mIsAnimDoneListener = EventManager::Get().Subscribe(\"battler_is_anim_done\", [this](const EventData& e){ OnIsAnimDone(e); });\n    mMoveOffsetListener = EventManager::Get().Subscribe(\"battler_set_offset\", [this](const EventData& e){ OnMoveOffset(e); });\n    mGetWorldPosListener = EventManager::Get().Subscribe(\"battler_get_world_pos\", [this](const EventData& e){ OnGetWorldPos(e); });\n    mGetOffsetListener = EventManager::Get().Subscribe(\"battler_get_offset\", [this](const EventData& e){ OnGetOffset(e); });\n"

if 'mPlayAnimListener =' not in content:
    content = content.replace('mBattleRenderer.Initialize(', on_enter_mod + '\n    mBattleRenderer.Initialize(')

on_exit_mod = "    EventManager::Get().Unsubscribe(\"battler_play_anim\", mPlayAnimListener);\n    EventManager::Get().Unsubscribe(\"battler_is_anim_done\", mIsAnimDoneListener);\n    EventManager::Get().Unsubscribe(\"battler_set_offset\", mMoveOffsetListener);\n    EventManager::Get().Unsubscribe(\"battler_get_world_pos\", mGetWorldPosListener);\n    EventManager::Get().Unsubscribe(\"battler_get_offset\", mGetOffsetListener);\n"

if 'EventManager::Get().Unsubscribe' not in content:
    content = content.replace('mBattleRenderer.Shutdown();', on_exit_mod + '    mBattleRenderer.Shutdown();')

methods = """
bool BattleState::GetBattlerSlot(IBattler* target, int& outSlot, bool& outIsPlayer) const
{
    if (!target) return false;
    const auto& players = mBattle.GetAllPlayers();
    for (int i = 0; i < (int)players.size() && i < BattleRenderer::kMaxSlots; ++i) {
        if (players[i] == target) {
            outSlot = i; outIsPlayer = true; return true;
        }
    }
    const auto& enemies = mBattle.GetAllEnemies();
    for (int i = 0; i < (int)enemies.size() && i < BattleRenderer::kMaxSlots; ++i) {
        if (enemies[i] == target) {
            outSlot = i; outIsPlayer = false; return true;
        }
    }
    return false;
}

void BattleState::OnPlayAnim(const EventData& e)
{
    auto p = static_cast<PlayAnimPayload*>(e.payload);
    int slot; bool isPlayer;
    if (GetBattlerSlot(p->target, slot, isPlayer)) {
        if (isPlayer) mBattleRenderer.PlayPlayerClip(slot, p->animType);
        else mBattleRenderer.PlayEnemyClip(slot, p->animType);
    }
}

void BattleState::OnIsAnimDone(const EventData& e)
{
    auto p = static_cast<IsAnimDonePayload*>(e.payload);
    int slot; bool isPlayer;
    if (GetBattlerSlot(p->target, slot, isPlayer)) {
        if (isPlayer) p->isDone = mBattleRenderer.IsPlayerClipDone(slot);
        else p->isDone = mBattleRenderer.IsEnemyClipDone(slot);
    }
}

void BattleState::OnMoveOffset(const EventData& e)
{
    auto p = static_cast<MoveOffsetPayload*>(e.payload);
    int slot; bool isPlayer;
    if (GetBattlerSlot(p->target, slot, isPlayer)) {
        if (isPlayer) mBattleRenderer.SetPlayerDrawOffset(slot, p->offsetX, p->offsetY);
        else mBattleRenderer.SetEnemyDrawOffset(slot, p->offsetX, p->offsetY);
    }
}

void BattleState::OnGetWorldPos(const EventData& e)
{
    auto p = static_cast<GetWorldPosPayload*>(e.payload);
    int slot; bool isPlayer;
    if (GetBattlerSlot(p->target, slot, isPlayer)) {
        if (isPlayer) mBattleRenderer.GetPlayerSlotPos(slot, p->x, p->y);
        else mBattleRenderer.GetEnemySlotPos(slot, p->x, p->y);
    }
}

void BattleState::OnGetOffset(const EventData& e)
{
    auto p = static_cast<MoveOffsetPayload*>(e.payload);
    int slot; bool isPlayer;
    if (GetBattlerSlot(p->target, slot, isPlayer)) {
        if (isPlayer) {
            auto offset = mBattleRenderer.GetPlayerDrawOffset(slot);
            p->offsetX = offset.x; p->offsetY = offset.y;
        } else {
            auto offset = mBattleRenderer.GetEnemyDrawOffset(slot);
            p->offsetX = offset.x; p->offsetY = offset.y;
        }
    }
}
"""

if 'bool BattleState::GetBattlerSlot' not in content:
    content += methods

with open('src/States/BattleState.cpp', 'w', encoding='utf-8') as f:
    f.write(content)
