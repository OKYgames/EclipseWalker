#pragma once

#include "GameTimer.h"
#include <DescriptorHeap.h>
#include <DirectXMath.h>
#include <SpriteBatch.h>
#include <SpriteFont.h>
#include <functional>
#include <memory>
#include <vector>

class DamageTextRenderer;
class EclipseWalkerGame;
class GameObject;
class MapSystem;
class Monster;
class Player;
struct RenderItem;

class Stage2BossController
{
public:
    using TrackOwnedCallback = std::function<void(GameObject*, RenderItem*)>;

    static constexpr int BossHpLayerCount = 200;
    static constexpr int BossDamageLayersPerHit = 10;

    void Initialize(
        EclipseWalkerGame* game,
        MapSystem* mapSystem,
        DamageTextRenderer* damageTextRenderer,
        const TrackOwnedCallback& trackOwned);
    void InitializeHealthText();
    void Reset();
    void Update(const GameTimer& gt, Player* player);
    void Draw();

    Monster* GetBoss() const { return mBoss; }
    int GetCurrentHealthLayer() const;
    void ApplyServerSync(int state, float x, float y, float z, float rotY);
    void ApplyServerHit(int remainHp, bool isDead);
    void ApplyServerPattern(int patternType, float x, float y, float z, float radius, float delay, int damage);

    static DirectX::XMFLOAT3 GetBossAnchorPosition();
    static DirectX::XMFLOAT3 GetBossSpawnPosition();
    static DirectX::XMFLOAT3 GetPlayerStartPosition();

private:
    void BuildBoss();
    void BuildBossPatternIndicator();
    void ShowBossPatternRadiusIndicator(const DirectX::XMFLOAT3& center);
    void UpdateBossPatternIndicator(float dt);
    void UpdateBossPattern150Damage(Player* player, float dt);
    void ApplyBossPattern150Damage(Player* player);
    void UpdateBossPatternTriggers(Player* player, int currentBossLayer);
    void TriggerBossPattern150(Player* player);
    void UpdateBossHealthUi(Player* player, int currentBossLayer);
    void DrawBossHealthText();
    int CalculateBossHealthLayer(float currentHp, float maxHp) const;
    bool ShouldShowBossHealth(Player* player) const;
    void TrackOwned(GameObject* object, RenderItem* renderItem) const;

private:
    EclipseWalkerGame* mGame = nullptr;
    MapSystem* mMapSystem = nullptr;
    DamageTextRenderer* mDamageTextRenderer = nullptr;
    TrackOwnedCallback mTrackOwned;

    Monster* mBoss = nullptr;
    GameObject* mBossPatternRadiusObj = nullptr;
    GameObject* mBossPatternRadiusRingObj = nullptr;

    bool mShowBossHealthText = false;
    bool mBossPattern150Triggered = false;
    bool mBossPattern150DamagePending = false;
    int mBossHealthTextLayer = 0;
    float mBossPatternRadiusTimer = 0.0f;
    float mBossPattern150DamageTimer = 0.0f;
    DirectX::XMFLOAT3 mBossPattern150DamageCenter = { 0.0f, 0.0f, 0.0f };

    std::unique_ptr<DirectX::DescriptorHeap> mBossHealthTextHeap;
    std::unique_ptr<DirectX::SpriteBatch> mBossHealthTextBatch;
    std::unique_ptr<DirectX::SpriteFont> mBossHealthTextFont;
};
