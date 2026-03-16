import re

with open('src/Battle/AttackSkill.cpp', 'r', encoding='utf-8') as f:
    content = f.read()

# Remove the old sequence and replace it with just the move action, which now naturally encapsulates moving and arriving.
old_sequence_1 = r'''    // 2\. Play battle move animation \(concurrent with movement logic\)
    actions\.push_back\(std::make_unique<PlayAnimationAction>\(&caster, CombatantAnim::BattleMove, false\)\);

    // 3\. Move to target's melee range
    actions\.push_back\(std::make_unique<MoveAction>\(&caster, target, MoveAction::TargetType::MeleeRange, mData\.moveDuration\)\);'''

new_sequence_1 = r'''    // 2. Move to target's melee range (automatically manages BattleMove and BattleUnmove inside MoveAction)
    actions.push_back(std::make_unique<MoveAction>(&caster, target, MoveAction::TargetType::MeleeRange, mData.moveDuration));'''
content = re.sub(old_sequence_1, new_sequence_1, content)

old_sequence_2 = r'''    // 6\. Play battle unmove animation \(concurrent with returning logic\)
    actions\.push_back\(std::make_unique<PlayAnimationAction>\(&caster, CombatantAnim::BattleUnmove, false\)\);

    // 7\. Move back to origin
    actions\.push_back\(std::make_unique<MoveAction>\(&caster, nullptr, MoveAction::TargetType::Origin, mData\.returnDuration\)\);'''

new_sequence_2 = r'''    // 6. Move back to origin (automatically manages BattleMove and BattleUnmove inside MoveAction)
    actions.push_back(std::make_unique<MoveAction>(&caster, nullptr, MoveAction::TargetType::Origin, mData.returnDuration));'''
content = re.sub(old_sequence_2, new_sequence_2, content)

# Change FightState delay! Actually, do we need to wait for FightState to finish? 
# To speed it up, let's not wait for the clip to finish, or just remove FightState wait.
content = content.replace('(&caster, CombatantAnim::FightState, true)', '(&caster, CombatantAnim::FightState, false)')

# Same for Attack animation, wait for it so the hit resolves cleanly, but wait...
# the actual speed of 'Attack' clip dictates the damage frame. 

with open('src/Battle/AttackSkill.cpp', 'w', encoding='utf-8') as f:
    f.write(content)
