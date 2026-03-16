import os

files_to_update = ['src/Battle/PlayerCombatant.cpp', 'src/Battle/EnemyCombatant.cpp']

for file_path in files_to_update:
    if os.path.exists(file_path):
        with open(file_path, 'r', encoding='utf-8') as f:
            content = f.read()

        if '#include "../Utils/JsonLoader.h"' not in content:
            content = '#include "../Utils/JsonLoader.h"\n' + content

        # Replace AttackSkill() with the data-driven config version
        old_str = r'std::make_unique<AttackSkill>()'
        
        replacement = '''[]() {
            JsonLoader::SkillData attackData;
            JsonLoader::LoadSkillData("data/skills/attack.json", attackData);
            return std::make_unique<AttackSkill>(attackData);
        }()'''
        
        content = content.replace(old_str, replacement)
        
        with open(file_path, 'w', encoding='utf-8') as f:
            f.write(content)
