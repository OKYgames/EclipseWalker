#pragma once
#include "GameTimer.h"

class EclipseWalkerGame;

class Scene
{
protected:
    EclipseWalkerGame* mGame; 

public:
    Scene(EclipseWalkerGame* game) : mGame(game) {}
    virtual ~Scene() {}

    virtual void Enter() = 0;   // 씬이 시작될 때 (텍스처 로드)
    virtual void Exit() = 0;    // 씬이 끝날 때 (메모리 해제)
    virtual void Update(const GameTimer& gt) = 0;
    virtual void Draw(const GameTimer& gt) = 0;
};