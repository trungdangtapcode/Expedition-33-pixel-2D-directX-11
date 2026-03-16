import os
import re

with open('src/Battle/AttackSkill.cpp', 'r', encoding='utf-8') as f:
    content = f.read()

# Fix the sequence inside Execute
old_sequence = r'''    // 2\. Move to target's melee range
    actions\.push_back\(std::make_unique<MoveAction>\(&caster, target, MoveAction::TargetType::MeleeRange, 0\.5f\)\);

    // 3\. Play battle move animation
    actions\.push_back\(std::make_unique<PlayAnimationAction>\(&caster, CombatantAnim::BattleMove, true\)\);'''

new_sequence = '''    // 2. Play battle move animation (concurrent with movement logic)
    actions.push_back(std::make_unique<PlayAnimationAction>(&caster, CombatantAnim::BattleMove, false));

    // 3. Move to target's melee range
    actions.push_back(std::make_unique<MoveAction>(&caster, target, MoveAction::TargetType::MeleeRange, mData.moveDuration));'''

content = re.sub(old_sequence, new_sequence, content)

old_return_sequence = r'''    // 6\. Play battle unmove animation \(returning\)
    actions\.push_back\(std::make_unique<PlayAnimationAction>\(&caster, CombatantAnim::BattleUnmove, true\)\);

    // 7\. Move back to origin
    actions\.push_back\(std::make_unique<MoveAction>\(&caster, nullptr, MoveAction::TargetType::Origin, 0\.5f\)\);'''

new_return_sequence = '''    // 6. Play battle unmove animation (concurrent with returning logic)
    actions.push_back(std::make_unique<PlayAnimationAction>(&caster, CombatantAnim::BattleUnmove, false));

    // 7. Move back to origin
    actions.push_back(std::make_unique<MoveAction>(&caster, nullptr, MoveAction::TargetType::Origin, mData.returnDuration));'''

content = re.sub(old_return_sequence, new_return_sequence, content)

with open('src/Battle/AttackSkill.cpp', 'w', encoding='utf-8') as f:
    f.write(content)
