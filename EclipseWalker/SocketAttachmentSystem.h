#pragma once

#include "GameObject.h"
#include <DirectXMath.h>
#include <string>
#include <vector>

struct SocketAttachmentDesc
{
    GameObject* ParentObject = nullptr;
    GameObject* ChildObject = nullptr;
    std::string SocketName;
    DirectX::XMFLOAT3 LocalPosition = { 0.0f, 0.0f, 0.0f };
    DirectX::XMFLOAT3 LocalRotation = { 0.0f, 0.0f, 0.0f };
    DirectX::XMFLOAT3 LocalScale = { 1.0f, 1.0f, 1.0f };
};

class SocketAttachmentSystem
{
public:
    void Attach(const SocketAttachmentDesc& attachment);
    void Detach(GameObject* childObject);
    void Clear();
    void Update();

private:
    std::vector<SocketAttachmentDesc> mAttachments;
};
