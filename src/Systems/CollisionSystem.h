// ============================================================
// File: CollisionSystem.h
// Responsibility: Concrete implementation of ICollisionSystem.
//
// Implements 2D intersection routines for circles and AABBs.
// Performs standard Euclidean distance queries for circles
// and min/max interval-overlap queries for AABBs.
//
// Performance implications:
//   Distance checks use squared Euclidean distance to avoid a
//   costly square-root (sqrt) operation.
// ============================================================
#pragma once
#include "ICollisionSystem.h"

class CollisionSystem : public ICollisionSystem {
public:
    CollisionSystem() = default;
    ~CollisionSystem() override = default;

    bool CheckCircleVsCircle(const CircleCollider& a, const CircleCollider& b) const override;
    bool CheckAABBvsAABB(const AABBCollider& a, const AABBCollider& b) const override;
    bool CheckPointInCircle(const DirectX::XMFLOAT2& point, const CircleCollider& circle) const override;
};
