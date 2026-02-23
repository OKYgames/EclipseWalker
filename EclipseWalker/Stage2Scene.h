#pragma once
#include "Scene.h"
#include "MapSystem.h"
#include <vector>
#include <memory>

class Stage2Scene : public Scene
{
public:
    Stage2Scene(EclipseWalkerGame* game) : Scene(game) {}

    virtual void Enter() override;
    virtual void Exit() override;
    virtual void Update(const GameTimer& gt) override;
    virtual void Draw(const GameTimer& gt) override;
};