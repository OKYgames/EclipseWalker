#pragma once
#include "AudioManager.h"
#include "Player.h"
#include <functional>
#include <vector>

namespace ArcherAnimationTiming
{
    // 0.0f = animation start, 1.0f = animation end.
    inline constexpr float kSkillESoundProgress = 0.1f;
    inline constexpr float kSkillEArrowFallStartProgress = 0.8f;
    inline constexpr float kSkillEArrowFallDurationSeconds = 0.6f;
    inline constexpr float kSkillEHitProgress = 0.8f;
    inline constexpr float kSkillESoundStopDelaySeconds = 1.0f;

    inline float DelayFromProgress(float animationDuration, float progress)
    {
        const float clampedProgress = progress < 0.0f ? 0.0f : (progress > 1.0f ? 1.0f : progress);
        return animationDuration > 0.0f ? animationDuration * clampedProgress : 0.0f;
    }
}

class EclipseWalkerGame;
class GameObject;
struct RenderItem;

class Archer : public Player
{
public:
    Archer();
    ~Archer() override;

    PlayerClass GetClassType() const override { return PlayerClass::Archer; }

    bool Skill1() override;
    bool Skill2() override;
    float GetBasicAttackSpeedMultiplier() const override;
    float GetSkillEffectIntensityMultiplier() const override;
    bool HasAttackSpeedBuff() const override;
    float GetAttackSpeedBuffRemaining() const override;
    void FireBasicArrow(EclipseWalkerGame* game, const DirectX::XMFLOAT3& origin, float rotY, float travelDistance);
    using ArrowCollisionCallback = std::function<bool(
        const DirectX::XMFLOAT3& previousPosition,
        const DirectX::XMFLOAT3& currentPosition,
        float rotY)>;
    void UpdateArrows(float dt, const ArrowCollisionCallback& collisionCallback = {});
    void HideArrows();

protected:
    void UpdateMeshForTier() override;
    void OnDashStarted() override;
    void OnBasicAttackStarted(int attackVariant) override;
    void OnSkillAttackStarted(int skillIndex) override;

private:
    enum class ArrowTrailType
    {
        NormalArrowTrail,
        BuffedArrowTrail
    };

    struct ArrowProjectile
    {
        GameObject* Object = nullptr;
        RenderItem* Ritem = nullptr;
        DirectX::XMFLOAT3 StartPosition = { 0.0f, 0.0f, 0.0f };
        DirectX::XMFLOAT3 PreviousPosition = { 0.0f, 0.0f, 0.0f };
        DirectX::XMFLOAT3 Direction = { 0.0f, 0.0f, 1.0f };
        float RotY = 0.0f;
        float Age = 0.0f;
        float Delay = 0.0f;
        float LifeTime = 0.0f;
        float TravelDistance = 0.0f;
        bool Buffed = false;
        ArrowTrailType TrailType = ArrowTrailType::NormalArrowTrail;
        bool Active = false;
    };

    bool EnsureArrowResources(EclipseWalkerGame* game);
    void SetArrowTrailType(ArrowProjectile& projectile, ArrowTrailType trailType);
    DirectX::XMFLOAT4 GetArrowTrailColorMultiplier(ArrowTrailType trailType) const;
    void UpdateClassState(float dt) override;
    float GetSkillAttackLockDuration(int skillIndex) const override;
    void StopArrowRainSound();
    void StopWindImbuementLoopSound();

    std::vector<ArrowProjectile> mArrowProjectiles;
    float mWindImbuementTimer = 0.0f;
    float mFootstepTimer = 0.0f;
    int mNextFootstepVariant = 1;
    bool mWasWalkingOnGround = false;
    float mArrowRainSoundTimer = 0.0f;
    bool mArrowRainSoundPending = false;
    float mArrowRainSoundStopTimer = 0.0f;
    bool mArrowRainSoundStopPending = false;
    AudioManager::ClipHandle mArrowRainSoundHandle = AudioManager::InvalidClipHandle;
    AudioManager::ClipHandle mWindImbuementLoopHandle = AudioManager::InvalidClipHandle;
};
