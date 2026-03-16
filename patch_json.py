import re

with open('src/Utils/JsonLoader.h', 'r', encoding='utf-8') as f:
    content = f.read()

skill_data_struct = '''
struct SkillData {
    float moveDuration = 0.5f;
    float returnDuration = 0.5f;
};

inline bool LoadSkillData(const std::string& path, SkillData& out)
{
    std::ifstream file(path);
    if (!file.is_open()) {
        LOG("[JsonLoader] Cannot open skill data file: '%s'", path.c_str());
        return false;
    }
    std::stringstream buffer;
    buffer << file.rdbuf();
    std::string src = buffer.str();

    out.moveDuration = detail::ParseFloat(detail::ValueOf(src, "moveDuration"), 0.5f);
    out.returnDuration = detail::ParseFloat(detail::ValueOf(src, "returnDuration"), 0.5f);

    LOG("[JsonLoader] Loaded SkillData from '%s'.", path.c_str());
    return true;
}
'''

if 'struct SkillData' not in content:
    content = content.replace('} // namespace JsonLoader', skill_data_struct + '\n} // namespace JsonLoader')

with open('src/Utils/JsonLoader.h', 'w', encoding='utf-8') as f:
    f.write(content)
