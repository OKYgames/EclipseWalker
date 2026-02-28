#pragma once
#include "Scene.h"

class MainMenuScene : public Scene
{
public:
    MainMenuScene(EclipseWalkerGame* game) : Scene(game) {}

    virtual void Enter() override;
    virtual void Exit() override;
    virtual void Update(const GameTimer& gt) override;
    virtual void Draw(const GameTimer& gt) override;
};