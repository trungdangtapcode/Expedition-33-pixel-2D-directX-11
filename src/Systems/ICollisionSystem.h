// ============================================================
// File: ICollisionSystem.h
// Responsibility: Defines the interface for all 2D collision algorithms.
//
// This is an interface (ICollisionSystem) used to decouple the raw math
// from game logic. By passing the interface around, game states don't
// need to depend on the concrete implementation.
//
// Common mistakes:
//   1. Relying exclusively on AABB without checking radius for entities
//      that naturally fit a circle, resulting in "corner" collisions that
//      feel unnatural.
// ============================================================
#pragma once
#include <DirectXMath.h>

struct CircleCollider {
    DirectX::XMFLOAT2 center;
    float radius;
};

struct AABBCollider {
    DirectX::XMFLOAT2 minPoint;
    DirectX::XMFLOAT2 maxPoint;
};

class ICollisionSystem {
public:
    virtual ~ICollisionSystem() = default;

    // ------------------------------------------------------------
    // CheckCircleVsCircle
    // Purpose: Detect overlap between two circular bounds using Euclidean distance.
    // ------------------------------------------------------------
    virtual bool CheckCircleVsCircle(const CircleCollider& a, const CircleCollider& b) const = 0;

    // ------------------------------------------------------------
    // CheckAABBvsAABB
    // Purpose: Detect overlap between two Axis-Aligned Bounding Boxes.
    // ------------------------------------------------------------
    virtual bool CheckAABBvsAABB(const AABBCollider& a, const AABBCollider& b) const = 0;

    // ------------------------------------------------------------
    // CheckPointInCircle
    // Purpose: Detect if a single 2D point is contained within a circle.
    // ------------------------------------------------------------
    virtual bool CheckPointInCircle(const DirectX::XMFLOAT2& point, const CircleCollider& circle) const = 0;
};
