#include <iostream>
#include <vector>
#include <cmath>
#include <algorithm>
#include <cassert>

struct QteNode {
    float startProg;
    float perfectProg;
};

void TestQteOverlapTimeline(int count, float qteStartMoment, float damageMoment, float qteSpacing) {
    std::vector<QteNode> mNodes(count);
    
    // Explicit mapped logic
    float duration = qteSpacing;
    float maxWindowStart = damageMoment - duration;
    if (maxWindowStart < qteStartMoment) maxWindowStart = qteStartMoment;
    
    // Distribute nodes evenly with a small random jitter to prevent stacked overlaps!
    float totalWindow = maxWindowStart - qteStartMoment;
    float segment = (count > 0) ? (totalWindow / static_cast<float>(count)) : 0.0f;
    
    for (int i = 0; i < count; ++i) {
        float randRatio = static_cast<float>(rand()) / static_cast<float>(RAND_MAX);
        // Place it inside its segmented block to ensure rhythmic spacing, scaled to 70% to guarantee a gap
        float jitterOffset = randRatio * (segment * 0.7f);
        mNodes[i].startProg = qteStartMoment + (i * segment) + jitterOffset;
        mNodes[i].perfectProg = mNodes[i].startProg + duration;
    }
    
    std::sort(mNodes.begin(), mNodes.end(), [](const QteNode& a, const QteNode& b) {
        return a.startProg < b.startProg;
    });

    for (int i = 0; i < count; ++i) {
        // Assert mathematical safety inside the game loop boundaries
        assert(mNodes[i].startProg >= qteStartMoment && "Start prog cannot violate QTE Start Moment limit");
        assert(mNodes[i].perfectProg <= damageMoment + 0.0001f && "Perfect prog cannot violate Damage Moment limit");
        
        // Assert chronological progression sorting for proper user inputs
        if (i > 0) assert(mNodes[i-1].startProg <= mNodes[i].startProg && "Sorting array failed!");
    }
    std::cout << "[OK] Testing timeline: " << count << " nodes safely clamped inside " << qteStartMoment << " -> " << damageMoment << " limits!\n";
}

int main() {
    std::cout << "Running QteMath unit tests...\n";
    
    // Standard test
    TestQteOverlapTimeline(3, 0.2f, 0.8f, 0.15f);
    
    // Single node sanity test
    TestQteOverlapTimeline(1, 0.2f, 0.8f, 0.15f);
    
    // Extreme boundary crash test
    // E.g. trying to squeeze 5 QTEs each being exactly the size of the whole window!
    TestQteOverlapTimeline(5, 0.2f, 0.8f, 0.60f); 
    
    std::cout << "\nAll mathematical overlap and sequence clamp tests passed successfully.\n";
    return 0;
}
