# Data-Driven Bullet Hell Architecture

## 1. Motivation
Previously, bullet hell dodge mechanics were hardcoded aggressively over explicit variables directly within the `BulletHellAction` C++ file. This caused issues scaling complexity:
- Adding different trajectories (Sine waves, Spirals, Tracking) required dirty boolean flags and sprawling `if-else` cascades directly inside `BulletHellAction::Execute()`.
- Multiple textures could not be drawn concurrently, limiting combat sequence fidelity rigidly to monochrome visual output.
- Every enemy utilized identically the exact same pattern layout, reducing the design dimension of encounter configurations.

The solution was transitioning entirely to a decoupling strategy prioritizing **`IBulletSpawner` Component abstractions** driven natively by an interconnected JSON schema.

## 2. Core Architecture Redesign

### The Target Flow
Encounter (`data/enemies/skeleton_group.json`) 
-> Individual Skill Config (`data/skills/skeleton_archer_attack.json`) 
-> Multi-Spawner Pattern (`data/bullet_patterns/archer_pattern.json`)

By migrating the pattern definition into its own designated `.json` layer rather than binding it to the skill itself, **any individual enemy instance** can execute completely distinct defensive mini-games despite ostensibly casting the same "attack" ability.

### Multi-Renderer Support (`BattleBulletHellRenderer`)
Instead of `mDynamicBulletTex`, the renderer now implements a `std::unordered_map<int, Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>>` caching framework.
- **Loading:** Whenever `BattleManager` hands off its `BulletHellPayload` event, the struct passes down a `std::vector<std::string>` matrix of globally requested textures.
- **Identification:** Each localized `PhysicsBullet` contains its own dedicated `textureIndex` referencing its exact origin art, meaning distinct enemy Spawners flawlessly overlay distinct sprites sequentially during parallel render ticks.

### The Component Framework (`IBulletSpawner`)
The single `BulletHellAction` runtime loop was gutted. Behavior logic was shifted down into dedicated class units implementing the `IBulletSpawner` interface.
Three concrete configurations presently exist in the active engine:
1. `RandomEdgeSpawner.cpp`: Computes outer bound trajectory vectors to randomly spray uniform crystal rain.
2. `SpiralSpawner.cpp`: Maintains isolated rotational trackers (`mCurrentAngle`) independent from explicit framerate deltas to spawn rotating distributions.
3. `SineSpawner.cpp`: Uses `BulletBehavior::Sine` plus `startX/startY` state to add orthogonal wave displacement to a base trajectory.
4. `ShieldWallSpawner.cpp`: Emits timed lane walls with configurable safe gaps for heavier enemies that should test positioning instead of random dodging.

`ShieldWallSpawner` treats `spawnRate` as wall waves per second. Its additional JSON fields are `laneCount`, `gapLaneCount`, `gapMode`, `gapStep`, `wallDirection`, and `lanePadding`.

### Struct Integration (`PhysicsBullet`)
To intercept and allow decoupled Spawners to command intricate trajectory offsets internally without sacrificing performance overhead out of C++ vector copies...
```cpp
struct PhysicsBullet {
    float x, y, vx, vy;
    float radius, angle;
    int textureIndex, damageScaling; 
    
    // Custom trajectory metadata
    BulletBehavior behavior = BulletBehavior::Linear;
    float timeAlive, startX, startY, amplitude, frequency;
};
```
Through native struct-level caching, `BulletHellAction::Execute` iterates without latency while resolving dynamic trigonometric expressions exactly on demand!

## 3. Creating New Patterns

Data design dictates that patterns accept arrays of nested `spawners`, resolving dynamically parallel iterations.

Example `data/bullet_patterns/archer_pattern.json`:
```json
{
    "durationSec": 5.0,
    "boxWidth": 550.0,
    "boxHeight": 250.0,
    "invincibilityDuration": 1.0,
    "spawners": [
        {
            "type": "sine",
            "texturePath": "assets/UI/round_bullet.png",
            "bulletRadius": 10.0,
            "bulletSpeed": 150.0,
            "spawnRate": 10.0,
            "bulletDamageScaling": 1.0,
            "sineAmplitude": 30.0,
            "sineFrequency": 3.5
        },
        {
            "type": "spiral",
            "texturePath": "assets/UI/crystal_bullet.png",
            "bulletRadius": 12.0,
            "bulletSpeed": 80.0,
            "spawnRate": 6.0,
            "bulletDamageScaling": 0.5
        }
    ]
}
```

The hand-rolled foundational parser `JsonLoader::ExtractObjectsFromArray` flawlessly navigates nested bracket depths to deserialize the concurrent architectures out to exactly mirror the JSON designer's request!
