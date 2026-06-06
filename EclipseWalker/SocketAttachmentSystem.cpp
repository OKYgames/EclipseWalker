#include "SocketAttachmentSystem.h"

#include "RenderItem.h"
#include "SkeletalAnimationComponent.h"
#include <algorithm>

using namespace DirectX;

void SocketAttachmentSystem::Attach(const SocketAttachmentDesc& attachment)
{
    if (attachment.ParentObject == nullptr || attachment.ChildObject == nullptr || attachment.SocketName.empty())
    {
        return;
    }

    Detach(attachment.ChildObject);
    mAttachments.push_back(attachment);
}

void SocketAttachmentSystem::Detach(GameObject* childObject)
{
    if (childObject == nullptr)
    {
        return;
    }

    mAttachments.erase(std::remove_if(mAttachments.begin(), mAttachments.end(),
        [&](const SocketAttachmentDesc& attachment)
        {
            return attachment.ChildObject == childObject;
        }),
        mAttachments.end());

    childObject->ClearWorldTransformOverride();
    if (childObject->Ritem != nullptr)
    {
        childObject->Ritem->Visible = true;
    }
}

void SocketAttachmentSystem::Clear()
{
    for (auto& attachment : mAttachments)
    {
        if (attachment.ChildObject != nullptr)
        {
            attachment.ChildObject->ClearWorldTransformOverride();
            if (attachment.ChildObject->Ritem != nullptr)
            {
                attachment.ChildObject->Ritem->Visible = true;
            }
        }
    }

    mAttachments.clear();
}

void SocketAttachmentSystem::Update()
{
    for (auto& attachment : mAttachments)
    {
        if (attachment.ParentObject == nullptr || attachment.ChildObject == nullptr)
        {
            continue;
        }

        if (attachment.ParentObject->Ritem != nullptr &&
            !attachment.ParentObject->Ritem->Visible)
        {
            if (attachment.ChildObject->Ritem != nullptr)
            {
                attachment.ChildObject->Ritem->Visible = false;
            }
            continue;
        }

        auto* skeletalAnimation = attachment.ParentObject->GetSkeletalAnimation();
        if (skeletalAnimation == nullptr || !skeletalAnimation->IsLoaded())
        {
            if (attachment.ChildObject->Ritem != nullptr)
            {
                attachment.ChildObject->Ritem->Visible = false;
            }
            continue;
        }

        XMFLOAT4X4 socketLocalTransform = MathHelper::Identity4x4();
        if (!skeletalAnimation->TryGetSocketLocalTransform(attachment.SocketName, socketLocalTransform))
        {
            if (attachment.ChildObject->Ritem != nullptr)
            {
                attachment.ChildObject->Ritem->Visible = false;
            }
            continue;
        }

        if (attachment.ChildObject->Ritem != nullptr)
        {
            attachment.ChildObject->Ritem->Visible = true;
        }

        XMMATRIX localOffset =
            XMMatrixScaling(attachment.LocalScale.x, attachment.LocalScale.y, attachment.LocalScale.z) *
            XMMatrixRotationRollPitchYaw(attachment.LocalRotation.x, attachment.LocalRotation.y, attachment.LocalRotation.z) *
            XMMatrixTranslation(attachment.LocalPosition.x, attachment.LocalPosition.y, attachment.LocalPosition.z);

        XMMATRIX socketMatrix = XMLoadFloat4x4(&socketLocalTransform);
        XMMATRIX parentWorld = XMLoadFloat4x4(&attachment.ParentObject->World);
        XMMATRIX childWorld = localOffset * socketMatrix * parentWorld;

        attachment.ChildObject->SetWorldTransform(childWorld);
    }
}
