// ============================================================
// File: ObjectiveDirector.h
// Responsibility: Resolve the current chapter objective from
//                 durable story flags and data-authored waypoints.
//
// Architecture:
//   ObjectiveDirector is read-only from the overworld's point of view.
//   It does not mutate GameProgress, push states, or talk to renderers.
//   It only converts saved flags plus player position into display text.
//
// Lifetime:
//   Created in  -> OverworldState::OnEnter()
//   Destroyed in -> OverworldState::OnExit()
// ============================================================
#pragma once

#include <string>
#include <vector>

struct ObjectiveView
{
    bool active = false;
    std::string id;
    std::string title;
    std::string body;
    std::string waypointHint;
    float waypointX = 0.0f;
    float waypointY = 0.0f;
    bool hasWaypoint = false;
};

class ObjectiveDirector
{
public:
    bool Initialize(const std::string& path);
    ObjectiveView Resolve(float playerX, float playerY) const;

private:
    struct ObjectiveStage
    {
        std::string id;
        std::string titleKey;
        std::string titleFallback;
        std::string bodyKey;
        std::string bodyFallback;
        std::string waypointLabelKey;
        std::string waypointLabelFallback;
        std::string arrivalHintKey;
        std::string arrivalHintFallback;
        std::vector<std::string> requiresFlags;
        std::vector<std::string> blockedByFlags;
        float waypointX = 0.0f;
        float waypointY = 0.0f;
        float arrivalDistanceUnits = 0.0f;
        bool hasWaypoint = false;
    };

    bool LoadFromSource(const std::string& src, const std::string& path);
    bool RequirementsMet(const ObjectiveStage& stage) const;
    std::string BuildWaypointHint(
        const ObjectiveStage& stage,
        float playerX,
        float playerY) const;
    std::string ResolveDirectionKey(float dx, float dy) const;

    std::vector<ObjectiveStage> mStages;
    float mDistanceUnitsPerMeter = 64.0f;
    float mArrivalDistanceUnits = 96.0f;
};
