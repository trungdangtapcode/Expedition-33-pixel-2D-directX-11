import os

for path in ['src/Battle/EnemyCombatant.cpp', 'src/Battle/PlayerCombatant.cpp']:
    with open(path, 'r', encoding='utf-8') as f:
        c = f.read()
    
    old_str = '''            if (!JsonLoader::LoadSkillData(attackJsonPath, attackData)) {
                LOG("[EnemyCombatant] WARNING: Failed to load attack data '%s'. Using fallback defaults.", attackJsonPath.c_str());
            }
            return std::make_unique<AttackSkill>(attackData);'''
    
    new_str = '''            if (!JsonLoader::LoadSkillData(attackJsonPath, attackData)) {
                LOG("[EnemyCombatant] WARNING: Failed to load attack data '%s'. Using fallback defaults.", attackJsonPath.c_str());
            }
            LOG("[EnemyCombatant] LOADED %s move=%.2f ret=%.2f dmg=%.2f", attackJsonPath.c_str(), attackData.moveDuration, attackData.returnDuration, attackData.damageTakenOccurMoment);
            return std::make_unique<AttackSkill>(attackData);'''
            
    c = c.replace(old_str, new_str)
    
    old_str2 = '''            if (!JsonLoader::LoadSkillData(attackJsonPath, attackData)) {
                LOG("[PlayerCombatant] WARNING: Failed to load attack data '%s'. Using fallback defaults.", attackJsonPath.c_str());
            }
            return std::make_unique<AttackSkill>(attackData);'''
            
    new_str2 = '''            if (!JsonLoader::LoadSkillData(attackJsonPath, attackData)) {
                LOG("[PlayerCombatant] WARNING: Failed to load attack data '%s'. Using fallback defaults.", attackJsonPath.c_str());
            }
            LOG("[PlayerCombatant] LOADED %s move=%.2f ret=%.2f dmg=%.2f", attackJsonPath.c_str(), attackData.moveDuration, attackData.returnDuration, attackData.damageTakenOccurMoment);
            return std::make_unique<AttackSkill>(attackData);'''
            
    c = c.replace(old_str2, new_str2)
    
    with open(path, 'w', encoding='utf-8') as f:
        f.write(c)
