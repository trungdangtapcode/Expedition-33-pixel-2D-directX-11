
import sys
import re

def patch_file():
    with open("src/UI/TurnQueueUI.cpp", "r", encoding="utf-8") as f:
        content = f.read()

    new_text = """// The core constraint: Items in a queue should strictly slide UP (index decreases) 
        // to take the place of finished actions. An action should NEVER slide DOWN.
        int bestOldIdx = -1;
        int minDistance = 9999;

        for (int oldIdx = 0; oldIdx < (int)mNodes.size(); ++oldIdx) {
            auto& oldNode = mNodes[oldIdx];
            if (oldNode.battler == b && !oldNode.matched) {
                // oldIdx >= i means either staying in place or shifting UP the visual queue 
                if (oldIdx >= i) {
                    int dist = oldIdx - i;
                    if (dist < minDistance) {
                        minDistance = dist;
                        bestOldIdx = oldIdx;
                    }
                }
            }
        }

        bool found = false;
        if (bestOldIdx != -1) {
            auto& oldNode = mNodes[bestOldIdx];
            node.currentX = oldNode.currentX;
            node.currentY = oldNode.currentY;
            node.currentScale = oldNode.currentScale;
            oldNode.matched = true;
            found = true;
        }

        // Initial setup for completely new items coming from the bottom        
        if (!found) {
            node.currentX = mConfig.startX + mConfig.slideOffsetX;
            node.currentY = layoutY + 150.0f;
            node.currentScale = mConfig.normalScale;
        }"""

    content = re.sub(r"// Find existing match by prioritizing.*?node\.currentScale = mConfig\.normalScale;\n        }", new_text, content, flags=re.DOTALL)
    
    with open("src/UI/TurnQueueUI.cpp", "w", encoding="utf-8") as f:
        f.write(content)

if __name__ == "__main__":
    patch_file()

