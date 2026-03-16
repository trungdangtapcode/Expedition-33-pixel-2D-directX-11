import re

with open('src/Battle/BattleManager.cpp', 'r', encoding='utf-8') as f:
    text = f.read()

text = text.replace('#include "DelayedAction.h"', '')

old_block = r'''        // Wrap the concrete action in a DelayedAction so the queue pauses
        // for kDefaultDelay seconds after each step\.
        // Pass ownership into DelayedAction; it takes sole responsibility\.
        mQueue\.Enqueue\(std::make_unique<DelayedAction>\(std::move\(finalAction\)\)\);'''

new_block = '''        mQueue.Enqueue(std::move(finalAction));'''

text = re.sub(old_block, new_block, text)

with open('src/Battle/BattleManager.cpp', 'w', encoding='utf-8') as f:
    f.write(text)
