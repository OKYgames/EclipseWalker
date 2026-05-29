#pragma once

#include "d3dUtil.h"
#include "GameTimer.h"
#include <DescriptorHeap.h>
#include <SpriteBatch.h>
#include <SpriteFont.h>
#include <memory>
#include <vector>

class EclipseWalkerGame;

class DamageTextRenderer
{
public:
    enum class Type
    {
        Outgoing,
        Incoming
    };

    explicit DamageTextRenderer(EclipseWalkerGame* game);

    void Initialize();
    void Reset();
    void Update(float dt);
    void Draw();

    void Spawn(const DirectX::XMFLOAT3& worldPosition, float damage, Type type);
    void SpawnOutgoing(const DirectX::XMFLOAT3& worldPosition, float damage);
    void SpawnIncoming(const DirectX::XMFLOAT3& worldPosition, float damage);

private:
    struct DamageText
    {
        DirectX::XMFLOAT3 WorldPosition = { 0.0f, 0.0f, 0.0f };
        int Damage = 0;
        Type TextType = Type::Outgoing;
        float Age = 0.0f;
        float Lifetime = 0.85f;
        float RiseSpeed = 0.85f;
        float Scale = 0.62f;
    };

    bool ProjectToScreen(const DirectX::XMFLOAT3& worldPosition, DirectX::XMFLOAT2& outScreenPosition) const;
    DirectX::XMVECTOR GetColor(Type type, float alpha) const;

private:
    EclipseWalkerGame* mGame = nullptr;
    std::vector<DamageText> mTexts;
    std::unique_ptr<DirectX::DescriptorHeap> mFontHeap;
    std::unique_ptr<DirectX::SpriteBatch> mSpriteBatch;
    std::unique_ptr<DirectX::SpriteFont> mFont;
};
