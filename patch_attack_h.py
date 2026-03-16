import os

with open('src/Battle/AttackSkill.h', 'r', encoding='utf-8') as f:
    content = f.read()

includes = '#include "../Utils/JsonLoader.h"\n'
if '#include "../Utils/JsonLoader.h"' not in content:
    content = content.replace('#include "ISkill.h"', '#include "ISkill.h"\n' + includes)

class_mod = '''class AttackSkill : public ISkill
{
private:
    JsonLoader::SkillData mData;
public:
    AttackSkill(const JsonLoader::SkillData& data) : mData(data) {}'''

if 'JsonLoader::SkillData mData;' not in content:
    content = content.replace('class AttackSkill : public ISkill\n{', class_mod)
    content = content.replace('class AttackSkill : public ISkill\r\n{', class_mod)

content = content.replace('public:\n    const char* GetName()', '    const char* GetName()')
content = content.replace('public:\r\n    const char* GetName()', '    const char* GetName()')

with open('src/Battle/AttackSkill.h', 'w', encoding='utf-8') as f:
    f.write(content)
