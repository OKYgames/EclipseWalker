#pragma once
#include "AudioManager.h"
#include "Player.h"
#include <functional>
#include <vector>

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
    void StopWindImbuementLoopSound();

    std::vector<ArrowProjectile> mArrowProjectiles;
    float mWindImbuementTimer = 0.0f;
    float mFootstepTimer = 0.0f;
    int mNextFootstepVariant = 1;
    bool mWasWalkingOnGround = false;
    AudioManager::ClipHandle mWindImbuementLoopHandle = AudioManager::InvalidClipHandle;
};
