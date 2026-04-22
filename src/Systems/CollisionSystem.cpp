// ============================================================
// File: CollisionSystem.cpp
// Responsibility: Implements all interface intersection methods using squared-distance math.
// ============================================================
#include "CollisionSystem.h"
#include <cmath>

// ------------------------------------------------------------
// Function: CheckCircleVsCircle
// Purpose:
//   Determines whether two circles overlap. Uses the squared distance
//   instead of true distance to avoid std::sqrt() cost.
// Why: Performance optimization for many checks per frame.
// ------------------------------------------------------------
bool CollisionSystem::CheckCircleVsCircle(const CircleCollider& a, const CircleCollider& b) const {
    float dx = a.center.x - b.center.x;
    float dy = a.center.y - b.center.y;
    float distanceSq = dx * dx + dy * dy;
    
    // We add radii together, then square the sum to compare
    // squared distances and skip sqrt().
    float r = a.radius + b.radius;
    float rSq = r * r;
    return distanceSq <= rSq;
}

// ------------------------------------------------------------
// Function: CheckAABBvsAABB
// Purpose:
//   Standard AABB overlap testing. Checks if minimum bounds are less
//   than maximum bounds in all axes.
// ------------------------------------------------------------
bool CollisionSystem::CheckAABBvsAABB(const AABBCollider& a, const AABBCollider& b) const {
    // Overlapping condition on both axes X and Y
    bool overlapX = (a.minPoint.x <= b.maxPoint.x) && (a.maxPoint.x >= b.minPoint.x);
    bool overlapY = (a.minPoint.y <= b.maxPoint.y) && (a.maxPoint.y >= b.minPoint.y);
    return overlapX && overlapY;
}

// ------------------------------------------------------------
// Function: CheckPointInCircle
// Purpose:
//   Check if a single 2D point lies within a circle.
// ------------------------------------------------------------
bool CollisionSystem::CheckPointInCircle(const DirectX::XMFLOAT2& point, const CircleCollider& circle) const {
    float dx = point.x - circle.center.x;
    float dy = point.y - circle.center.y;
    float distanceSq = dx * dx + dy * dy;
    
    float rSq = circle.radius * circle.radius;
    return distanceSq <= rSq;
}
