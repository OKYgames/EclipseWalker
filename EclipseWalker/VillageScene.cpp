#include "VillageScene.h"

#include "Camera.h"
#include "CharacterVisualFactory.h"
#include "DebugConfig.h"
#include "EclipseWalkerGame.h"
#include "GameObject.h"
#include "MainMenuScene.h"
#include "MathHelper.h"
#include "MeshGeometry.h"
#include "ModelLoader.h"
#include "NetworkManager.h"
#include "Player.h"
#include "RenderItem.h"
#include "ResourceManager.h"
#include "SkeletalAnimationComponent.h"
#include "Stage1Scene.h"
#include "d3dUtil.h"

#include <ResourceUploadBatch.h>
#include <RenderTargetState.h>
#include <algorithm>
#include <cctype>
#include <cmath>
#include <filesystem>
#include <iomanip>
#include <sstream>
#include <unordered_map>

using namespace DirectX;

namespace
{
    constexpr char kVillageMapPath[] = "Models/Village/village.fbx";
    constexpr char kVillageFloorColliderPath[] = "Models/Village/VillageFloorCollider.fbx";
    constexpr char kVillageWallColliderPath[] = "Models/Village/VillageWallCollider.fbx";
    constexpr float kVillageMapScale = 1.0f;
    constexpr float kVillageMinCameraDistance = 22.0f;
    constexpr float kVillageRotationX = 0.0f;
    constexpr float kVillageFloorColliderYOffset = -0.08f;
    constexpr float kVillageSpawnProbeY = 120.0f;
    constexpr float kVillageFallbackSpawnY = 4.0f;
    constexpr float kVillageCloudHeightA = 185.0f;
    constexpr float kVillageCloudHeightB = 235.0f;
    constexpr float kVillagePortalPosX = -0.030413f;
    constexpr float kVillagePortalPosY = 2.308053f;
    constexpr float kVillagePortalPosZ = -40.4005f;
    constexpr float kVillagePortalInteractRange = 2.4f;
    constexpr char kShopKeeperDirectory[] = "Models/Animated/ShopKeeper";
    constexpr char kShopKeeperClipName[] = "ShopKeeperIdle";
    constexpr char kShopKeeperGeometryName[] = "shopKeeperGeo";
    constexpr char kShopKeeperMaterialName[] = "ShopKeeperMat";
    constexpr char kShopKeeperTextureName[] = "ShopKeeperDiffuse";
    constexpr wchar_t kShopKeeperTexturePath[] = L"Textures/ShopKeeper_Diff.dds";
    constexpr float kShopKeeperPosX = -8.71902f;
    constexpr float kShopKeeperPosY = 0.778053f;
    constexpr float kShopKeeperPosZ = -21.6078f;
    constexpr float kShopKeeperTargetHeight = 1.35f;
    constexpr float kUiBaseWidth = 1280.0f;
    constexpr float kUiBaseHeight = 720.0f;
    constexpr float kShopPanelTextureWidth = 1000.0f;
    constexpr float kShopPanelTextureHeight = 1580.0f;
    constexpr float kShopItemRowSpacing = 196.0f;
    constexpr int kShopVisibleRowCount = 5;
    constexpr wchar_t kShopTitleText[] = L"마을 상점";
    constexpr char kShopPanelTextureName[] = "UI_Shop_Panel";
    constexpr char kShopPanelTexturePath[] = "Textures/UI/Shop/shop_panel.dds";
    constexpr char kShopWeaponIconWarriorLv2TextureName[] = "UI_Shop_Weapon_WarriorLv2";
    constexpr char kShopWeaponIconWarriorLv3TextureName[] = "UI_Shop_Weapon_WarriorLv3";
    constexpr char kShopWeaponIconMageLv2TextureName[] = "UI_Shop_Weapon_MageLv2";
    constexpr char kShopWeaponIconMageLv3TextureName[] = "UI_Shop_Weapon_MageLv3";
    constexpr char kShopWeaponIconArcherLv2TextureName[] = "UI_Shop_Weapon_ArcherLv2";
    constexpr char kShopWeaponIconArcherLv3TextureName[] = "UI_Shop_Weapon_ArcherLv3";
    constexpr char kShopPotionIconHpSmallTextureName[] = "UI_Shop_Potion_HpSmall";
    constexpr char kShopPotionIconMpSmallTextureName[] = "UI_Shop_Potion_MpSmall";
    constexpr char kShopPotionIconHpMediumTextureName[] = "UI_Shop_Potion_HpMedium";
    constexpr char kShopPotionIconMpMediumTextureName[] = "UI_Shop_Potion_MpMedium";
    constexpr char kShopPotionIconBattleElixirTextureName[] = "UI_Shop_Potion_BattleElixir";
    constexpr char kShopArmorIconWarriorLv2TextureName[] = "UI_Shop_Armor_WarriorLv2";
    constexpr char kShopArmorIconWarriorLv3TextureName[] = "UI_Shop_Armor_WarriorLv3";
    constexpr char kShopArmorIconMageLv2TextureName[] = "UI_Shop_Armor_MageLv2";
    constexpr char kShopArmorIconMageLv3TextureName[] = "UI_Shop_Armor_MageLv3";
    constexpr char kShopArmorIconArcherLv2TextureName[] = "UI_Shop_Armor_ArcherLv2";
    constexpr char kShopArmorIconArcherLv3TextureName[] = "UI_Shop_Armor_ArcherLv3";
    constexpr char kShopWeaponIconWarriorLv2TexturePath[] = "Textures/UI/Shop/weapon_icon_warrior_lv2.dds";
    constexpr char kShopWeaponIconWarriorLv3TexturePath[] = "Textures/UI/Shop/weapon_icon_warrior_lv3.dds";
    constexpr char kShopWeaponIconMageLv2TexturePath[] = "Textures/UI/Shop/weapon_icon_mage_lv2.dds";
    constexpr char kShopWeaponIconMageLv3TexturePath[] = "Textures/UI/Shop/weapon_icon_mage_lv3.dds";
    constexpr char kShopWeaponIconArcherLv2TexturePath[] = "Textures/UI/Shop/weapon_icon_archer_lv2.dds";
    constexpr char kShopWeaponIconArcherLv3TexturePath[] = "Textures/UI/Shop/weapon_icon_archer_lv3.dds";
    constexpr char kShopPotionIconHpSmallTexturePath[] = "Textures/UI/Shop/potion_icon_hp_small.dds";
    constexpr char kShopPotionIconMpSmallTexturePath[] = "Textures/UI/Shop/potion_icon_mp_small.dds";
    constexpr char kShopPotionIconHpMediumTexturePath[] = "Textures/UI/Shop/potion_icon_hp_medium.dds";
    constexpr char kShopPotionIconMpMediumTexturePath[] = "Textures/UI/Shop/potion_icon_mp_medium.dds";
    constexpr char kShopPotionIconBattleElixirTexturePath[] = "Textures/UI/Shop/potion_icon_battle_elixir.dds";
    constexpr char kShopArmorIconWarriorLv2TexturePath[] = "Textures/UI/Shop/armor_icon_warrior_lv2.dds";
    constexpr char kShopArmorIconWarriorLv3TexturePath[] = "Textures/UI/Shop/armor_icon_warrior_lv3.dds";
    constexpr char kShopArmorIconMageLv2TexturePath[] = "Textures/UI/Shop/armor_icon_mage_lv2.dds";
    constexpr char kShopArmorIconMageLv3TexturePath[] = "Textures/UI/Shop/armor_icon_mage_lv3.dds";
    constexpr char kShopArmorIconArcherLv2TexturePath[] = "Textures/UI/Shop/armor_icon_archer_lv2.dds";
    constexpr char kShopArmorIconArcherLv3TexturePath[] = "Textures/UI/Shop/armor_icon_archer_lv3.dds";

    std::string FindFirstFbxInDirectory(const std::filesystem::path& directory)
    {
        if (!std::filesystem::exists(directory))
        {
            return "";
        }

        for (const auto& entry : std::filesystem::directory_iterator(directory))
        {
            if (!entry.is_regular_file())
            {
                continue;
            }

            std::string extension = entry.path().extension().string();
            std::transform(extension.begin(), extension.end(), extension.begin(),
                [](unsigned char c)
                {
                    return static_cast<char>(std::tolower(c));
                });

            if (extension == ".fbx")
            {
                return entry.path().generic_string();
            }
        }

        return "";
    }

    struct UiRectF
    {
        float left = 0.0f;
        float top = 0.0f;
        float right = 0.0f;
        float bottom = 0.0f;
    };

    struct VillageMaterialBinding
    {
        std::string MaterialName;
        bool HideSubset = false;
    };

    struct VillageResourceCache
    {
        bool Valid = false;
        std::vector<Subset> Subsets;
        std::vector<VillageMaterialBinding> MaterialBindings;
        XMFLOAT3 WorldOffset = { 0.0f, 0.0f, 0.0f };
        float CameraRadius = kVillageMinCameraDistance;
    };

    VillageResourceCache gVillageResourceCache;

    bool HasReusableVillageMapResources(ResourceManager* resources)
    {
        return resources != nullptr &&
            gVillageResourceCache.Valid &&
            resources->mGeometries.find("villageMapGeo") != resources->mGeometries.end();
    }

    std::string ToLowerAscii(std::string value)
    {
        for (char& ch : value)
        {
            if (ch >= 'A' && ch <= 'Z')
            {
                ch = static_cast<char>(ch - 'A' + 'a');
            }
        }

        return value;
    }

    void BackfillVillageDiffuseNamesFromKnownMapping(
        std::vector<ImportedMaterialInfo>& targetInfos)
    {
        if (targetInfos.empty())
        {
            return;
        }

        static const std::unordered_map<std::string, std::string> kDiffuseByMaterialName =
        {
            { "ground", "*0" },
            { "fence_f_1311_2422_2333", "*3" },
            { "gate", "*6" },
            { "material_8", "*9" },
            { "scaffold", "*12" },
            { "material", "*15" },
            { "church", "*18" },
            { "market", "*21" },
            { "home", "*24" },
            { "material_7", "*27" },
            { "tower", "*30" },
            { "stockade", "*33" },
        };

        for (ImportedMaterialInfo& targetInfo : targetInfos)
        {
            if (!targetInfo.DiffuseTextureName.empty() || targetInfo.MaterialName.empty())
            {
                continue;
            }

            const auto it = kDiffuseByMaterialName.find(ToLowerAscii(targetInfo.MaterialName));
            if (it != kDiffuseByMaterialName.end())
            {
                targetInfo.DiffuseTextureName = it->second;
            }
        }
    }

    bool IsPlayerNearVillagePortal(const XMFLOAT3& position)
    {
        const float dx = position.x - kVillagePortalPosX;
        const float dz = position.z - kVillagePortalPosZ;
        return (dx * dx + dz * dz) <= (kVillagePortalInteractRange * kVillagePortalInteractRange);
    }

    bool TryLoadVillageTexture(
        ResourceManager* resources,
        const std::filesystem::path& baseDirectory,
        const std::string& sourceTextureName,
        const std::string& resourceName)
    {
        if (resources == nullptr || sourceTextureName.empty())
        {
            return false;
        }

        if (resources->GetTexture(resourceName) != nullptr)
        {
            return true;
        }

        const bool isEmbeddedReference =
            !sourceTextureName.empty() &&
            sourceTextureName[0] == '*' &&
            sourceTextureName.size() > 1;
        const std::filesystem::path sourcePath(sourceTextureName);
        const std::string stem = sourcePath.stem().string();
        const std::wstring stemWide(stem.begin(), stem.end());
        const std::wstring fileWide = std::wstring(sourceTextureName.begin(), sourceTextureName.end());
        std::wstring embeddedFileWide;
        if (isEmbeddedReference)
        {
            const std::string embeddedIndex = sourceTextureName.substr(1);
            embeddedFileWide = L"embedded_" + std::wstring(embeddedIndex.begin(), embeddedIndex.end()) + L".dds";
        }

        const std::filesystem::path candidates[] =
        {
            isEmbeddedReference ? (baseDirectory / L"village_textures" / embeddedFileWide) : std::filesystem::path(),
            isEmbeddedReference ? (std::filesystem::path(L"Textures/village_textures") / embeddedFileWide) : std::filesystem::path(),
            baseDirectory / fileWide,
            baseDirectory / (stemWide + L".dds"),
            baseDirectory / (stemWide + L".jpg"),
            baseDirectory / (stemWide + L".jpeg"),
            std::filesystem::path(L"Textures") / (stemWide + L".dds"),
            std::filesystem::path(L"Textures") / (stemWide + L".jpg")
        };

        for (const std::filesystem::path& candidate : candidates)
        {
            if (std::filesystem::exists(candidate))
            {
                resources->LoadTexture(resourceName, candidate.wstring());
                return true;
            }
        }

        return false;
    }

    const char* ResolveVillageColliderPath(const char* primaryPath, const char* legacyPath)
    {
        if (std::filesystem::exists(std::filesystem::path(primaryPath)))
        {
            return primaryPath;
        }

        if (legacyPath != nullptr && std::filesystem::exists(std::filesystem::path(legacyPath)))
        {
            return legacyPath;
        }

        return primaryPath;
    }

    bool StartsWithAscii(const std::string& value, const std::string& prefix)
    {
        return value.size() >= prefix.size() &&
            value.compare(0, prefix.size(), prefix) == 0;
    }

    bool IsVillageMaterialName(const std::string& materialName)
    {
        return StartsWithAscii(materialName, "Village_Mat_") ||
            materialName == "VillageFallbackMat" ||
            materialName == "VillageCloudLayerA" ||
            materialName == "VillageCloudLayerB";
    }

    bool IsVillageRenderItem(const RenderItem* renderItem)
    {
        if (renderItem == nullptr)
        {
            return false;
        }

        if (renderItem->Geo != nullptr && renderItem->Geo->Name == "villageMapGeo")
        {
            return true;
        }

        return renderItem->Mat != nullptr && IsVillageMaterialName(renderItem->Mat->Name);
    }

    void ReindexRenderItems(std::vector<std::unique_ptr<RenderItem>>& renderItems)
    {
        for (UINT i = 0; i < renderItems.size(); ++i)
        {
            if (renderItems[i] != nullptr)
            {
                renderItems[i]->ObjCBIndex = i;
                renderItems[i]->NumFramesDirty = gNumFrameResources;
            }
        }
    }

    float WrapUnit(float value)
    {
        const float wrapped = std::fmod(value, 1.0f);
        return wrapped < 0.0f ? wrapped + 1.0f : wrapped;
    }

    float UiRectWidth(const UiRectF& rect)
    {
        return rect.right - rect.left;
    }

    float UiRectHeight(const UiRectF& rect)
    {
        return rect.bottom - rect.top;
    }

    bool IsInsideRect(float x, float y, const UiRectF& rect)
    {
        return x >= rect.left && x <= rect.right &&
            y >= rect.top && y <= rect.bottom;
    }

    DirectX::XMFLOAT2 GetShopPanelSize(const D3D12_VIEWPORT& viewport)
    {
        const float widthScale = viewport.Width / kShopPanelTextureWidth;
        const float heightScale = viewport.Height / kShopPanelTextureHeight;
        const float scale = (std::min)(widthScale * 0.70f, heightScale * 0.94f);
        return { kShopPanelTextureWidth * scale, kShopPanelTextureHeight * scale };
    }

    UiRectF GetShopPanelScreenRect(const D3D12_VIEWPORT& viewport)
    {
        const XMFLOAT2 panelSize = GetShopPanelSize(viewport);
        const float left = (viewport.Width - panelSize.x) * 0.5f;
        const float top = (viewport.Height - panelSize.y) * 0.5f;
        return { left, top, left + panelSize.x, top + panelSize.y };
    }

    UiRectF TransformPanelRect(const UiRectF& localRect, const UiRectF& panelRect)
    {
        const float scaleX = UiRectWidth(panelRect) / kShopPanelTextureWidth;
        const float scaleY = UiRectHeight(panelRect) / kShopPanelTextureHeight;
        return
        {
            panelRect.left + localRect.left * scaleX,
            panelRect.top + localRect.top * scaleY,
            panelRect.left + localRect.right * scaleX,
            panelRect.top + localRect.bottom * scaleY
        };
    }

    UiRectF InsetRect(const UiRectF& rect, float insetLeft, float insetTop, float insetRight, float insetBottom)
    {
        return
        {
            rect.left + insetLeft,
            rect.top + insetTop,
            rect.right - insetRight,
            rect.bottom - insetBottom
        };
    }

    UiRectF OffsetRect(const UiRectF& rect, float offsetX, float offsetY)
    {
        return
        {
            rect.left + offsetX,
            rect.top + offsetY,
            rect.right + offsetX,
            rect.bottom + offsetY
        };
    }

    UiRectF UnionRect(const UiRectF& a, const UiRectF& b)
    {
        return
        {
            (std::min)(a.left, b.left),
            (std::min)(a.top, b.top),
            (std::max)(a.right, b.right),
            (std::max)(a.bottom, b.bottom)
        };
    }

    UiRectF ExpandRect(const UiRectF& rect, float left, float top, float right, float bottom)
    {
        return
        {
            rect.left - left,
            rect.top - top,
            rect.right + right,
            rect.bottom + bottom
        };
    }

    UiRectF GetShopBottomButtonHitRect(const UiRectF& localButtonRect)
    {
        const UiRectF shiftedTextRect = OffsetRect(localButtonRect, 26.0f, -44.0f);
        return ExpandRect(UnionRect(localButtonRect, shiftedTextRect), 20.0f, 28.0f, 20.0f, 18.0f);
    }

    RECT ToRect(const UiRectF& rect)
    {
        return
        {
            static_cast<LONG>(std::lround(rect.left)),
            static_cast<LONG>(std::lround(rect.top)),
            static_cast<LONG>(std::lround(rect.right)),
            static_cast<LONG>(std::lround(rect.bottom))
        };
    }

    DirectX::XMFLOAT2 TransformPanelPoint(float localX, float localY, const UiRectF& panelRect)
    {
        const float scaleX = UiRectWidth(panelRect) / kShopPanelTextureWidth;
        const float scaleY = UiRectHeight(panelRect) / kShopPanelTextureHeight;
        return
        {
            panelRect.left + localX * scaleX,
            panelRect.top + localY * scaleY
        };
    }

    float GetShopTextScale(const UiRectF& panelRect)
    {
        return UiRectHeight(panelRect) / kShopPanelTextureHeight;
    }

    UiRectF GetShopTabRect(VillageScene::ShopCategory category)
    {
        switch (category)
        {
        case VillageScene::ShopCategory::Weapon:
            return { 50.0f, 344.0f, 348.0f, 425.0f };
        case VillageScene::ShopCategory::Potion:
            return { 649.0f, 344.0f, 947.0f, 425.0f };
        case VillageScene::ShopCategory::Armor:
        default:
            return { 350.0f, 344.0f, 648.0f, 425.0f };
        }
    }

    UiRectF GetShopListRect()
    {
        return { 50.0f, 437.0f, 945.0f, 1386.0f };
    }

    UiRectF GetShopScrollTrackRect()
    {
        return { 906.0f, 438.0f, 932.0f, 1380.0f };
    }

    UiRectF GetShopCloseButtonRect()
    {
        return { 487.0f, 1485.0f, 833.0f, 1560.0f };
    }

    UiRectF GetShopScrollResetButtonRect()
    {
        return { 91.0f, 1485.0f, 449.0f, 1560.0f };
    }

    UiRectF GetShopEmptyMessageRect()
    {
        return { 170.0f, 780.0f, 824.0f, 930.0f };
    }

    UiRectF GetShopStatusRect()
    {
        return { 50.0f, 229.0f, 580.0f, 297.0f };
    }

    UiRectF GetShopNameBarRect(int visibleRow)
    {
        const float top = 454.0f + visibleRow * kShopItemRowSpacing;
        return { 285.0f, top, 629.0f, top + 59.0f };
    }

    UiRectF GetShopClassBarRect(int visibleRow)
    {
        const float top = 543.0f + visibleRow * kShopItemRowSpacing;
        return { 286.0f, top, 508.0f, top + 42.0f };
    }

    UiRectF GetShopIconRect(int visibleRow)
    {
        const float top = 454.0f + visibleRow * kShopItemRowSpacing;
        return { 87.0f, top, 257.0f, top + 170.0f };
    }

    UiRectF GetShopPriceRect(int visibleRow)
    {
        const float top = 459.0f + visibleRow * kShopItemRowSpacing;
        return { 671.0f, top, 858.0f, top + 59.0f };
    }

    UiRectF GetShopBuyButtonRect(int visibleRow)
    {
        const float top = 541.0f + visibleRow * kShopItemRowSpacing;
        return { 664.0f, top, 875.0f, top + 71.0f };
    }

    std::wstring FormatGoldAmount(int gold)
    {
        std::wstring digits = std::to_wstring((std::max)(gold, 0));
        for (int insertIndex = static_cast<int>(digits.size()) - 3; insertIndex > 0; insertIndex -= 3)
        {
            digits.insert(static_cast<size_t>(insertIndex), 1, L',');
        }

        return digits;
    }

    std::wstring GetShopCategoryLabel(VillageScene::ShopCategory category)
    {
        switch (category)
        {
        case VillageScene::ShopCategory::Weapon:
            return L"무기";
        case VillageScene::ShopCategory::Potion:
            return L"물약";
        case VillageScene::ShopCategory::Armor:
        default:
            return L"장비";
        }
    }

    PotionQuickSlot GetPotionQuickSlotForShopItem(const VillageScene::ShopItem& item)
    {
        if (item.Category != VillageScene::ShopCategory::Potion)
        {
            return PotionQuickSlot::Empty;
        }

        if (item.IconTextureName == kShopPotionIconHpMediumTextureName)
        {
            return PotionQuickSlot::HpMedium;
        }
        if (item.IconTextureName == kShopPotionIconHpSmallTextureName)
        {
            return PotionQuickSlot::HpSmall;
        }
        if (item.IconTextureName == kShopPotionIconMpMediumTextureName)
        {
            return PotionQuickSlot::MpMedium;
        }
        if (item.IconTextureName == kShopPotionIconMpSmallTextureName)
        {
            return PotionQuickSlot::MpSmall;
        }
        if (item.IconTextureName == kShopPotionIconBattleElixirTextureName)
        {
            return PotionQuickSlot::BattleElixir;
        }

        return PotionQuickSlot::Empty;
    }

    ClassTier GetShopItemTier(const VillageScene::ShopItem& item)
    {
        return static_cast<ClassTier>((std::clamp)(item.RequiredLevel, 1, 3));
    }

    void SetCloudTexTransform(
        RenderItem* renderItem,
        float tileU,
        float tileV,
        float offsetU,
        float offsetV)
    {
        if (renderItem == nullptr)
        {
            return;
        }

        DirectX::XMStoreFloat4x4(
            &renderItem->TexTransform,
            DirectX::XMMatrixScaling(tileU, tileV, 1.0f) *
            DirectX::XMMatrixTranslation(offsetU, offsetV, 0.0f));
        renderItem->NumFramesDirty = gNumFrameResources;
    }
}

VillageScene::VillageScene(EclipseWalkerGame* game)
    : Scene(game)
    , mChatController(game)
{
}

void VillageScene::InitializeShopTextureAssets()
{
    auto* resources = mGame->GetResources();

    auto loadTextureIfMissing = [&](const std::string& textureName, const std::wstring& texturePath)
    {
        if (resources->GetTexture(textureName) != nullptr)
        {
            return;
        }

        if (!std::filesystem::exists(texturePath))
        {
            std::wstring message = L"[VillageScene][Shop] Missing texture: ";
            message += texturePath;
            message += L"\n";
            OutputDebugStringW(message.c_str());
            return;
        }

        try
        {
            resources->LoadTexture(textureName, texturePath);
        }
        catch (const DxException& e)
        {
            std::wstring message = L"[VillageScene][Shop] LoadTexture failed for ";
            message += texturePath;
            message += L"\n";
            message += e.ToString();
            message += L"\n";
            OutputDebugStringW(message.c_str());
        }
        catch (const std::exception& e)
        {
            std::ostringstream oss;
            oss << "[VillageScene][Shop] LoadTexture failed for ";
            oss << std::filesystem::path(texturePath).string();
            oss << "\n";
            oss << e.what() << "\n";
            OutputDebugStringA(oss.str().c_str());
        }
    };

    loadTextureIfMissing(kShopPanelTextureName, std::filesystem::path(kShopPanelTexturePath).wstring());
    loadTextureIfMissing(kShopWeaponIconWarriorLv2TextureName, std::filesystem::path(kShopWeaponIconWarriorLv2TexturePath).wstring());
    loadTextureIfMissing(kShopWeaponIconWarriorLv3TextureName, std::filesystem::path(kShopWeaponIconWarriorLv3TexturePath).wstring());
    loadTextureIfMissing(kShopWeaponIconMageLv2TextureName, std::filesystem::path(kShopWeaponIconMageLv2TexturePath).wstring());
    loadTextureIfMissing(kShopWeaponIconMageLv3TextureName, std::filesystem::path(kShopWeaponIconMageLv3TexturePath).wstring());
    loadTextureIfMissing(kShopWeaponIconArcherLv2TextureName, std::filesystem::path(kShopWeaponIconArcherLv2TexturePath).wstring());
    loadTextureIfMissing(kShopWeaponIconArcherLv3TextureName, std::filesystem::path(kShopWeaponIconArcherLv3TexturePath).wstring());
    loadTextureIfMissing(kShopPotionIconHpSmallTextureName, std::filesystem::path(kShopPotionIconHpSmallTexturePath).wstring());
    loadTextureIfMissing(kShopPotionIconMpSmallTextureName, std::filesystem::path(kShopPotionIconMpSmallTexturePath).wstring());
    loadTextureIfMissing(kShopPotionIconHpMediumTextureName, std::filesystem::path(kShopPotionIconHpMediumTexturePath).wstring());
    loadTextureIfMissing(kShopPotionIconMpMediumTextureName, std::filesystem::path(kShopPotionIconMpMediumTexturePath).wstring());
    loadTextureIfMissing(kShopPotionIconBattleElixirTextureName, std::filesystem::path(kShopPotionIconBattleElixirTexturePath).wstring());
    loadTextureIfMissing(kShopArmorIconWarriorLv2TextureName, std::filesystem::path(kShopArmorIconWarriorLv2TexturePath).wstring());
    loadTextureIfMissing(kShopArmorIconWarriorLv3TextureName, std::filesystem::path(kShopArmorIconWarriorLv3TexturePath).wstring());
    loadTextureIfMissing(kShopArmorIconMageLv2TextureName, std::filesystem::path(kShopArmorIconMageLv2TexturePath).wstring());
    loadTextureIfMissing(kShopArmorIconMageLv3TextureName, std::filesystem::path(kShopArmorIconMageLv3TexturePath).wstring());
    loadTextureIfMissing(kShopArmorIconArcherLv2TextureName, std::filesystem::path(kShopArmorIconArcherLv2TexturePath).wstring());
    loadTextureIfMissing(kShopArmorIconArcherLv3TextureName, std::filesystem::path(kShopArmorIconArcherLv3TexturePath).wstring());
}

void VillageScene::InitializeShopUiResources()
{
    if (mShopFont && mShopTextureBatch && mShopTextBatch && mShopFontHeap && mShopGraphicsMemory)
    {
        return;
    }

    auto* device = mGame->GetDevice();
    auto* cmdQueue = mGame->GetCommandQueue();

    auto logDxFailure = [](const char* stage, const std::wstring& path, const DxException& e)
    {
        std::wstring message = L"[VillageScene][Shop] ";
        message += std::wstring(stage, stage + std::char_traits<char>::length(stage));
        message += L" failed for ";
        message += path;
        message += L"\n";
        message += e.ToString();
        message += L"\n";
        OutputDebugStringW(message.c_str());
    };

    auto logStdFailure = [](const char* stage, const std::wstring& path, const std::exception& e)
    {
        std::ostringstream oss;
        oss << "[VillageScene][Shop] " << stage << " failed for ";
        oss << std::filesystem::path(path).string();
        oss << "\n";
        oss << e.what() << "\n";
        OutputDebugStringA(oss.str().c_str());
    };

    if (!mShopFontHeap)
    {
        mShopFontHeap = std::make_unique<DirectX::DescriptorHeap>(
            device,
            D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV,
            D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE,
            1);
    }

    if (!mShopFont || !mShopTextureBatch || !mShopTextBatch)
    {
        try
        {
            DirectX::ResourceUploadBatch resourceUpload(device);
            resourceUpload.Begin();

            if (!mShopFont)
            {
                mShopFont = std::make_unique<DirectX::SpriteFont>(
                    device,
                    resourceUpload,
                    L"Textures/chat_korean.spritefont",
                    mShopFontHeap->GetCpuHandle(0),
                    mShopFontHeap->GetGpuHandle(0));
            }

            DirectX::RenderTargetState rtState(DXGI_FORMAT_R8G8B8A8_UNORM, DXGI_FORMAT_D24_UNORM_S8_UINT);
            DirectX::SpriteBatchPipelineStateDescription pd(rtState);

            if (!mShopTextureBatch)
            {
                mShopTextureBatch = std::make_unique<DirectX::SpriteBatch>(device, resourceUpload, pd);
            }

            if (!mShopTextBatch)
            {
                mShopTextBatch = std::make_unique<DirectX::SpriteBatch>(device, resourceUpload, pd);
            }

            auto uploadResourcesFinished = resourceUpload.End(cmdQueue);
            uploadResourcesFinished.wait();
        }
        catch (const DxException& e)
        {
            logDxFailure("CreateUiResources", L"Textures/chat_korean.spritefont", e);
            mShopFont.reset();
            mShopTextureBatch.reset();
            mShopTextBatch.reset();
        }
        catch (const std::exception& e)
        {
            logStdFailure("CreateUiResources", L"Textures/chat_korean.spritefont", e);
            mShopFont.reset();
            mShopTextureBatch.reset();
            mShopTextBatch.reset();
        }
    }
}

void VillageScene::InitializeShopData()
{
    if (!mShopItems.empty())
    {
        return;
    }

    mShopItems =
    {
        { 1001, ShopCategory::Weapon, L"수습 병사의 장검", L"전사 전용", PlayerClass::Warrior, 2, 1200, kShopWeaponIconWarriorLv2TextureName, false },
        { 1002, ShopCategory::Weapon, L"숙련 기사 장검", L"전사 전용", PlayerClass::Warrior, 3, 2200, kShopWeaponIconWarriorLv3TextureName, false },
        { 1011, ShopCategory::Weapon, L"수습 비전 지팡이", L"마법사 전용", PlayerClass::Mage, 2, 1200, kShopWeaponIconMageLv2TextureName, false },
        { 1012, ShopCategory::Weapon, L"숙련 별빛 지팡이", L"마법사 전용", PlayerClass::Mage, 3, 2200, kShopWeaponIconMageLv3TextureName, false },
        { 1021, ShopCategory::Weapon, L"수습 사냥 활", L"궁수 전용", PlayerClass::Archer, 2, 1200, kShopWeaponIconArcherLv2TextureName, false },
        { 1022, ShopCategory::Weapon, L"숙련 유격 장궁", L"궁수 전용", PlayerClass::Archer, 3, 2200, kShopWeaponIconArcherLv3TextureName, false },

        { 2001, ShopCategory::Armor, L"수습 수호 사슬갑", L"전사 전용", PlayerClass::Warrior, 2, 1600, kShopArmorIconWarriorLv2TextureName, false },
        { 2002, ShopCategory::Armor, L"숙련 기사 판금갑", L"전사 전용", PlayerClass::Warrior, 3, 2600, kShopArmorIconWarriorLv3TextureName, false },
        { 2011, ShopCategory::Armor, L"수습 마도 예복", L"마법사 전용", PlayerClass::Mage, 2, 1600, kShopArmorIconMageLv2TextureName, false },
        { 2012, ShopCategory::Armor, L"숙련 술법 법의", L"마법사 전용", PlayerClass::Mage, 3, 2600, kShopArmorIconMageLv3TextureName, false },
        { 2021, ShopCategory::Armor, L"수습 추적자 경갑", L"궁수 전용", PlayerClass::Archer, 2, 1600, kShopArmorIconArcherLv2TextureName, false },
        { 2022, ShopCategory::Armor, L"숙련 유격 경갑", L"궁수 전용", PlayerClass::Archer, 3, 2600, kShopArmorIconArcherLv3TextureName, false },

        { 3001, ShopCategory::Potion, L"소형 회복 물약", L"공용", PlayerClass::None, 1, 200, kShopPotionIconHpSmallTextureName, false },
        { 3002, ShopCategory::Potion, L"소형 마력 물약", L"공용", PlayerClass::None, 1, 200, kShopPotionIconMpSmallTextureName, false },
        { 3003, ShopCategory::Potion, L"중형 회복 물약", L"공용", PlayerClass::None, 2, 500, kShopPotionIconHpMediumTextureName, false },
        { 3004, ShopCategory::Potion, L"중형 마력 물약", L"공용", PlayerClass::None, 2, 500, kShopPotionIconMpMediumTextureName, false },
        { 3005, ShopCategory::Potion, L"전투 강화 비약", L"공용", PlayerClass::None, 3, 900, kShopPotionIconBattleElixirTextureName, false }
    };

    RebuildFilteredShopItems();
}

void VillageScene::RebuildFilteredShopItems()
{
    mFilteredShopItemIndices.clear();
    for (size_t i = 0; i < mShopItems.size(); ++i)
    {
        if (mShopItems[i].Category == mSelectedShopCategory)
        {
            mFilteredShopItemIndices.push_back(i);
        }
    }

    ClampShopScroll();
}

void VillageScene::ClampShopScroll()
{
    const int maxFirstVisibleIndex = (std::max)(0, static_cast<int>(mFilteredShopItemIndices.size()) - kShopVisibleRowCount);
    mShopFirstVisibleIndex = std::clamp(mShopFirstVisibleIndex, 0, maxFirstVisibleIndex);
}

void VillageScene::AdjustShopScroll(int rowDelta)
{
    if (rowDelta == 0)
    {
        return;
    }

    mShopFirstVisibleIndex += rowDelta;
    ClampShopScroll();
}

void VillageScene::HandleShopMouseClick(float x, float y)
{
    const auto viewport = mGame->GetScreenViewport();
    const UiRectF panelRect = GetShopPanelScreenRect(viewport);
    if (!IsInsideRect(x, y, panelRect))
    {
        return;
    }

    const struct
    {
        ShopCategory Category;
        UiRectF Rect;
    } kTabs[] =
    {
        { ShopCategory::Weapon, GetShopTabRect(ShopCategory::Weapon) },
        { ShopCategory::Armor, GetShopTabRect(ShopCategory::Armor) },
        { ShopCategory::Potion, GetShopTabRect(ShopCategory::Potion) }
    };

    for (const auto& tab : kTabs)
    {
        if (IsInsideRect(x, y, TransformPanelRect(tab.Rect, panelRect)))
        {
            if (mSelectedShopCategory != tab.Category)
            {
                mSelectedShopCategory = tab.Category;
                mShopFirstVisibleIndex = 0;
                RebuildFilteredShopItems();
            }
            return;
        }
    }

    if (IsInsideRect(x, y, TransformPanelRect(GetShopBottomButtonHitRect(GetShopCloseButtonRect()), panelRect)))
    {
        mShopOpen = false;
        SetShopStatusMessage(L"", { 0.92f, 0.92f, 0.92f, 0.0f }, 0.0f);
        return;
    }

    if (IsInsideRect(x, y, TransformPanelRect(GetShopBottomButtonHitRect(GetShopScrollResetButtonRect()), panelRect)))
    {
        mShopFirstVisibleIndex = 0;
        return;
    }

    for (int visibleRow = 0; visibleRow < kShopVisibleRowCount; ++visibleRow)
    {
        const UiRectF buyButtonRect = TransformPanelRect(GetShopBuyButtonRect(visibleRow), panelRect);
        if (IsInsideRect(x, y, buyButtonRect))
        {
            TryPurchaseVisibleShopItem(visibleRow);
            return;
        }
    }
}

bool VillageScene::TryPurchaseVisibleShopItem(int visibleRow)
{
    const int filteredIndex = mShopFirstVisibleIndex + visibleRow;
    if (filteredIndex < 0 || filteredIndex >= static_cast<int>(mFilteredShopItemIndices.size()))
    {
        return false;
    }

    Player* player = mGame->GetPlayer();
    if (player == nullptr)
    {
        return false;
    }

    ShopItem& item = mShopItems[mFilteredShopItemIndices[filteredIndex]];
    if (item.Purchased)
    {
        SetShopStatusMessage(L"이미 구매한 상품입니다.", { 0.86f, 0.82f, 0.54f, 1.0f });
        return false;
    }

    if (DebugConfig::kEnableBackendConnection && NetworkManager::Get()->IsConnected())
    {
        NetworkManager::Get()->SendShopPurchase(item.ItemId);
        SetShopStatusMessage(L"구매 요청 중입니다.", { 0.70f, 0.82f, 0.96f, 1.0f }, 1.0f);
        return true;
    }

    if (item.AllowedClass != PlayerClass::None && player->GetClassType() != item.AllowedClass)
    {
        SetShopStatusMessage(L"현재 직업으로는 구매할 수 없습니다.", { 0.95f, 0.42f, 0.42f, 1.0f });
        return false;
    }

    if (player->GetLevel() < item.RequiredLevel)
    {
        SetShopStatusMessage(L"레벨이 부족합니다.", { 0.95f, 0.55f, 0.42f, 1.0f });
        return false;
    }

    if (!player->TrySpendGold(item.Price))
    {
        SetShopStatusMessage(L"골드가 부족합니다.", { 0.98f, 0.56f, 0.36f, 1.0f });
        return false;
    }

    item.Purchased = true;
    switch (item.Category)
    {
    case ShopCategory::Weapon:
        mGame->EquipPurchasedWeaponTier(GetShopItemTier(item));
        SetShopStatusMessage(L"무기를 장착했습니다.", { 0.58f, 0.92f, 0.62f, 1.0f });
        break;
    case ShopCategory::Armor:
        mGame->EquipPurchasedArmorTier(GetShopItemTier(item));
        SetShopStatusMessage(L"장비를 장착했습니다.", { 0.58f, 0.92f, 0.62f, 1.0f });
        break;
    case ShopCategory::Potion:
    default:
        player->RegisterPotionPurchase(GetPotionQuickSlotForShopItem(item));
        SetShopStatusMessage(L"구매가 완료되었습니다.", { 0.58f, 0.92f, 0.62f, 1.0f });
        break;
    }

    return true;
}

void VillageScene::SetShopStatusMessage(const std::wstring& message, const XMFLOAT4& color, float durationSeconds)
{
    mShopStatusMessage = message;
    mShopStatusColor = color;
    mShopStatusRemaining = durationSeconds;
}

void VillageScene::TrackOwned(GameObject* object, RenderItem* renderItem)
{
    if (object != nullptr)
    {
        mOwnedObjects.push_back(object);
    }
    if (renderItem != nullptr)
    {
        mOwnedRenderItems.push_back(renderItem);
    }
}

void VillageScene::ReleaseOwnedObjects()
{
    auto& ritems = mGame->GetRitems();
    auto& objs = mGame->GetGameObjects();
    auto* resources = mGame->GetResources();

    const size_t objectCountBefore = objs.size();
    const size_t renderItemCountBefore = ritems.size();
    const size_t geometryCountBefore = resources != nullptr ? resources->mGeometries.size() : 0;
    const size_t textureCountBefore = resources != nullptr ? resources->mTextures.size() : 0;
    const size_t materialCountBefore = resources != nullptr ? resources->mMaterials.size() : 0;

    objs.erase(
        std::remove_if(
            objs.begin(),
            objs.end(),
            [&](const std::unique_ptr<GameObject>& object)
            {
                return std::find(mOwnedObjects.begin(), mOwnedObjects.end(), object.get()) != mOwnedObjects.end() ||
                    (object != nullptr && IsVillageRenderItem(object->Ritem));
            }),
        objs.end());

    ritems.erase(
        std::remove_if(
            ritems.begin(),
            ritems.end(),
            [&](const std::unique_ptr<RenderItem>& renderItem)
            {
                return std::find(mOwnedRenderItems.begin(), mOwnedRenderItems.end(), renderItem.get()) != mOwnedRenderItems.end() ||
                    IsVillageRenderItem(renderItem.get());
            }),
        ritems.end());

    ReindexRenderItems(ritems);

    if (resources != nullptr)
    {
        std::ostringstream log;
        log << "[VillageScene] Released village scene objects. Cached resources kept. objects "
            << objectCountBefore << " -> " << objs.size()
            << ", ritems " << renderItemCountBefore << " -> " << ritems.size()
            << ", geometries " << geometryCountBefore << " -> " << resources->mGeometries.size()
            << ", textures " << textureCountBefore << " -> " << resources->mTextures.size()
            << ", materials " << materialCountBefore << " -> " << resources->mMaterials.size()
            << "\n";
        OutputDebugStringA(log.str().c_str());
    }

    mOwnedObjects.clear();
    mOwnedRenderItems.clear();
}

void VillageScene::LogPlayerPosition(const XMFLOAT3& position)
{
    std::ostringstream log;
    log << "[Debug][PlayerPos] x=" << position.x
        << " y=" << position.y
        << " z=" << position.z << "\n";
    OutputDebugStringA(log.str().c_str());
}

void VillageScene::CreateShopKeeperNpc()
{
    auto* resources = mGame->GetResources();
    auto* device = mGame->GetDevice();
    auto* commandList = mGame->GetCommandList();
    auto& ritems = mGame->GetRitems();
    auto& objects = mGame->GetGameObjects();

    const std::string modelPath = FindFirstFbxInDirectory(kShopKeeperDirectory);
    if (modelPath.empty())
    {
        OutputDebugStringA("[VillageScene] Shop keeper model not found in Models/Animated/ShopKeeper.\n");
        return;
    }

    auto renderItem = std::make_unique<RenderItem>();
    renderItem->World = MathHelper::Identity4x4();
    renderItem->TexTransform = MathHelper::Identity4x4();
    renderItem->ObjCBIndex = static_cast<UINT>(ritems.size());

    auto object = std::make_unique<GameObject>();

    CharacterVisualSpec spec;
    spec.UseSkinned = true;
    spec.ModelPath = modelPath;
    spec.DefaultClipName = kShopKeeperClipName;
    spec.LoadModelAnimations = true;
    spec.GeometryName = kShopKeeperGeometryName;
    spec.MaterialName = kShopKeeperMaterialName;
    spec.DiffuseTextureName = kShopKeeperTextureName;
    spec.DiffuseTexturePath = kShopKeeperTexturePath;
    spec.DiffuseAlbedo = { 1.0f, 1.0f, 1.0f, 1.0f };
    spec.FresnelR0 = { 0.06f, 0.06f, 0.06f };
    spec.Roughness = 0.72f;
    spec.IsToon = true;
    spec.OutlineThickness = 0.012f;
    spec.OutlineColor = { 0.05f, 0.05f, 0.07f, 1.0f };
    spec.TargetHeight = kShopKeeperTargetHeight;
    spec.SpawnPosition = { kShopKeeperPosX, kShopKeeperPosY, kShopKeeperPosZ };
    spec.CenterBoundsXZ = true;
    spec.RotationOffset = { 0.0f, DirectX::XM_PI, 0.0f };
    spec.FallbackMaterialName = "VillageFallbackMat";
    spec.FallbackScale = { 0.45f, 0.75f, 0.45f };

    if (!CharacterVisualFactory::ApplyVisual(
        object.get(),
        renderItem.get(),
        device,
        commandList,
        resources,
        spec))
    {
        OutputDebugStringA("[VillageScene] Failed to create shop keeper visual.\n");
        return;
    }

    if (auto* animation = object->GetSkeletalAnimation())
    {
        animation->Play(kShopKeeperClipName, 0.0f, 1.0f, true);
    }

    TrackOwned(object.get(), renderItem.get());
    ritems.push_back(std::move(renderItem));
    objects.push_back(std::move(object));

    OutputDebugStringA("[VillageScene] Shop keeper NPC created.\n");
}

void VillageScene::Enter()
{
    mBackKeyPressed = false;
    mStage1KeyPressed = false;
    gIsChatInputActive = false;
    gIsLanternUiInputActive = false;

    mGame->LoadSharedGameResources();
    mGame->ResetLights();

    auto* resources = mGame->GetResources();
    auto* device = mGame->GetDevice();
    auto* commandList = mGame->GetCommandList();
    auto& ritems = mGame->GetRitems();
    auto& objects = mGame->GetGameObjects();

    if (std::filesystem::exists(L"Textures/sky_stage2.dds"))
    {
        resources->LoadTexture("sky_village", L"Textures/sky_stage2.dds");
    }
    else if (std::filesystem::exists(L"Textures/sky.dds"))
    {
        resources->LoadTexture("sky_village", L"Textures/sky.dds");
    }

    if (resources->GetMaterial("VillageFallbackMat") == nullptr)
    {
        resources->CreateMaterial(
            "VillageFallbackMat",
            static_cast<int>(resources->mMaterials.size()),
            "white",
            "",
            "",
            "",
            XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f),
            XMFLOAT3(0.05f, 0.05f, 0.05f),
            0.72f);
    }

    if (Material* fallbackMaterial = resources->GetMaterial("VillageFallbackMat"))
    {
        fallbackMaterial->IsToon = 0;
        fallbackMaterial->IsTransparent = 0;
        fallbackMaterial->OutlineThickness = 0.0f;
        fallbackMaterial->NumFramesDirty = gNumFrameResources;
    }

    if (std::filesystem::exists(L"Textures/Sky/FX_CloudAlpha05.dds"))
    {
        resources->LoadTexture("SkyCloudAlpha05", L"Textures/Sky/FX_CloudAlpha05.dds");
    }
    if (std::filesystem::exists(L"Textures/Sky/FX_CloudAlpha08.dds"))
    {
        resources->LoadTexture("SkyCloudAlpha08", L"Textures/Sky/FX_CloudAlpha08.dds");
    }

    auto ensureCloudMaterial = [&](const std::string& materialName, const std::string& diffuseMapName, const XMFLOAT4& albedo)
    {
        if (resources->GetMaterial(materialName) == nullptr)
        {
            resources->CreateMaterial(
                materialName,
                static_cast<int>(resources->mMaterials.size()),
                diffuseMapName,
                "",
                "",
                "",
                albedo,
                XMFLOAT3(0.01f, 0.01f, 0.01f),
                1.0f);
        }

        if (Material* material = resources->GetMaterial(materialName))
        {
            material->DiffuseMapName = diffuseMapName;
            material->DiffuseAlbedo = albedo;
            material->IsToon = 0;
            material->IsTransparent = 2;
            material->OutlineThickness = 0.0f;
            material->NumFramesDirty = gNumFrameResources;
        }
    };

    ensureCloudMaterial("VillageCloudLayerA", "SkyCloudAlpha05", XMFLOAT4(1.12f, 1.06f, 1.08f, 0.34f));
    ensureCloudMaterial("VillageCloudLayerB", "SkyCloudAlpha08", XMFLOAT4(0.94f, 0.90f, 0.92f, 0.16f));
    InitializeShopData();
    InitializeShopTextureAssets();

    auto skyRitem = std::make_unique<RenderItem>();
    XMStoreFloat4x4(&skyRitem->World, XMMatrixScaling(5000.0f, 5000.0f, 5000.0f));
    skyRitem->TexTransform = MathHelper::Identity4x4();
    skyRitem->ObjCBIndex = static_cast<UINT>(ritems.size());
    skyRitem->Mat = resources->GetMaterial("VillageFallbackMat");
    skyRitem->Geo = resources->mGeometries["boxGeo"].get();
    skyRitem->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
    auto& skyArgs = skyRitem->Geo->DrawArgs["box"];
    skyRitem->IndexCount = skyArgs.IndexCount;
    skyRitem->StartIndexLocation = skyArgs.StartIndexLocation;
    skyRitem->BaseVertexLocation = skyArgs.BaseVertexLocation;
    skyRitem->Visible = true;
    skyRitem->IsSkybox = true;
    TrackOwned(nullptr, skyRitem.get());
    ritems.push_back(std::move(skyRitem));

    auto addCloudLayer = [&](const std::string& materialName, float y, float scale, float yaw) -> RenderItem*
    {
        auto renderItem = std::make_unique<RenderItem>();
        renderItem->World = MathHelper::Identity4x4();
        renderItem->TexTransform = MathHelper::Identity4x4();
        renderItem->ObjCBIndex = static_cast<UINT>(ritems.size());
        renderItem->Mat = resources->GetMaterial(materialName);
        renderItem->Geo = resources->mGeometries["quadGeo"].get();
        renderItem->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
        renderItem->CastShadow = false;
        renderItem->Visible = renderItem->Mat != nullptr && renderItem->Geo != nullptr;

        if (renderItem->Geo != nullptr)
        {
            auto& drawArgs = renderItem->Geo->DrawArgs["quad"];
            renderItem->IndexCount = drawArgs.IndexCount;
            renderItem->StartIndexLocation = drawArgs.StartIndexLocation;
            renderItem->BaseVertexLocation = drawArgs.BaseVertexLocation;
        }

        auto object = std::make_unique<GameObject>();
        object->Ritem = renderItem.get();
        object->SetScale(scale, scale, 1.0f);
        object->SetRotation(-DirectX::XM_PIDIV2, yaw, 0.0f);
        object->SetPosition(0.0f, y, 0.0f);
        object->Update();

        RenderItem* rawRenderItem = renderItem.get();
        TrackOwned(object.get(), rawRenderItem);
        ritems.push_back(std::move(renderItem));
        objects.push_back(std::move(object));
        return rawRenderItem;
    };

    mCloudLayerA = addCloudLayer("VillageCloudLayerA", kVillageCloudHeightA, 1800.0f, 0.18f);
    mCloudLayerB = addCloudLayer("VillageCloudLayerB", kVillageCloudHeightB, 1500.0f, -0.31f);
    SetCloudTexTransform(mCloudLayerA, 2.8f, 2.8f, 0.0f, 0.0f);
    SetCloudTexTransform(mCloudLayerB, 2.1f, 2.1f, 0.0f, 0.0f);

    if (!HasReusableVillageMapResources(resources))
    {
        if (!std::filesystem::exists(std::filesystem::path(kVillageMapPath)))
        {
            OutputDebugStringA("[VillageScene] village.fbx not found. Expected Models/Village/village.fbx\n");
            return;
        }

        const std::filesystem::path villagePath = std::filesystem::path(kVillageMapPath);
        const std::filesystem::path villageDir = villagePath.parent_path();
        MapMeshData mapData;
        std::vector<ImportedMaterialInfo> materialInfos;
        if (!ModelLoader::LoadWithMaterialInfos(kVillageMapPath, mapData, materialInfos) ||
            mapData.Vertices.empty() ||
            mapData.Indices.empty())
        {
            OutputDebugStringA("[VillageScene] Failed to load village.fbx mesh/material data.\n");
            return;
        }

        const bool allDiffuseNamesMissing = !materialInfos.empty() &&
            std::all_of(
                materialInfos.begin(),
                materialInfos.end(),
                [](const ImportedMaterialInfo& info)
                {
                    return info.DiffuseTextureName.empty();
                });
        if (allDiffuseNamesMissing)
        {
            BackfillVillageDiffuseNamesFromKnownMapping(materialInfos);
        }
        std::vector<VillageMaterialBinding> materialBindings(materialInfos.size());

        for (size_t i = 0; i < materialInfos.size(); ++i)
        {
            const ImportedMaterialInfo& info = materialInfos[i];
            const std::string textureStem = info.DiffuseTextureName.empty()
                ? ""
                : std::filesystem::path(info.DiffuseTextureName).stem().string();
            std::string textureResourceName = textureStem.empty()
                ? ""
                : "Village_Tex_" + textureStem + "_" + std::to_string(i);

            std::string diffuseMapName = "white";
            if (!textureResourceName.empty() &&
                TryLoadVillageTexture(resources, villageDir, info.DiffuseTextureName, textureResourceName))
            {
                diffuseMapName = textureResourceName;
            }

            const std::string materialName = "Village_Mat_" + std::to_string(i);
            materialBindings[i].MaterialName = materialName;

            if (resources->GetMaterial(materialName) == nullptr)
            {
                resources->CreateMaterial(
                    materialName,
                    static_cast<int>(resources->mMaterials.size()),
                    diffuseMapName,
                    "",
                    "",
                    "",
                    XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f),
                    info.FresnelR0,
                    info.Roughness,
                    info.MetallicFactor);
            }

            if (Material* material = resources->GetMaterial(materialName))
            {
                material->DiffuseMapName = diffuseMapName;
                material->DiffuseAlbedo = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
                material->FresnelR0 = info.FresnelR0;
                material->Roughness = info.Roughness;
                material->MetallicFactor = info.MetallicFactor;
                material->IsToon = 0;
                material->IsTransparent = 0;
                material->OutlineThickness = 0.0f;
                material->NumFramesDirty = gNumFrameResources;
            }
        }

        XMFLOAT3 minBounds = mapData.Vertices.front().Pos;
        XMFLOAT3 maxBounds = mapData.Vertices.front().Pos;
        for (const Vertex& vertex : mapData.Vertices)
        {
            minBounds.x = (std::min)(minBounds.x, vertex.Pos.x);
            minBounds.y = (std::min)(minBounds.y, vertex.Pos.y);
            minBounds.z = (std::min)(minBounds.z, vertex.Pos.z);
            maxBounds.x = (std::max)(maxBounds.x, vertex.Pos.x);
            maxBounds.y = (std::max)(maxBounds.y, vertex.Pos.y);
            maxBounds.z = (std::max)(maxBounds.z, vertex.Pos.z);
        }

        auto villageGeo = std::make_unique<MeshGeometry>();
        villageGeo->Name = "villageMapGeo";

        const UINT vbByteSize = static_cast<UINT>(mapData.Vertices.size() * sizeof(Vertex));
        const UINT ibByteSize = static_cast<UINT>(mapData.Indices.size() * sizeof(std::uint32_t));

        D3DCreateBlob(vbByteSize, &villageGeo->VertexBufferCPU);
        CopyMemory(villageGeo->VertexBufferCPU->GetBufferPointer(), mapData.Vertices.data(), vbByteSize);
        D3DCreateBlob(ibByteSize, &villageGeo->IndexBufferCPU);
        CopyMemory(villageGeo->IndexBufferCPU->GetBufferPointer(), mapData.Indices.data(), ibByteSize);
        villageGeo->VertexBufferGPU = d3dUtil::CreateDefaultBuffer(
            device,
            commandList,
            mapData.Vertices.data(),
            vbByteSize,
            villageGeo->VertexBufferUploader);
        villageGeo->IndexBufferGPU = d3dUtil::CreateDefaultBuffer(
            device,
            commandList,
            mapData.Indices.data(),
            ibByteSize,
            villageGeo->IndexBufferUploader);
        villageGeo->VertexByteStride = sizeof(Vertex);
        villageGeo->VertexBufferByteSize = vbByteSize;
        villageGeo->IndexFormat = DXGI_FORMAT_R32_UINT;
        villageGeo->IndexBufferByteSize = ibByteSize;

        for (const Subset& subset : mapData.Subsets)
        {
            SubmeshGeometry submesh;
            submesh.IndexCount = subset.IndexCount;
            submesh.StartIndexLocation = subset.IndexStart;
            submesh.BaseVertexLocation = 0;
            villageGeo->DrawArgs["subset_" + std::to_string(subset.Id)] = submesh;
        }
        resources->mGeometries[villageGeo->Name] = std::move(villageGeo);

        const XMFLOAT3 sourceCenter =
        {
            (minBounds.x + maxBounds.x) * 0.5f,
            (minBounds.y + maxBounds.y) * 0.5f,
            (minBounds.z + maxBounds.z) * 0.5f
        };
        const XMFLOAT3 sourceExtents =
        {
            (maxBounds.x - minBounds.x) * 0.5f,
            (maxBounds.y - minBounds.y) * 0.5f,
            (maxBounds.z - minBounds.z) * 0.5f
        };

        gVillageResourceCache.Valid = true;
        gVillageResourceCache.Subsets = mapData.Subsets;
        gVillageResourceCache.MaterialBindings = std::move(materialBindings);
        gVillageResourceCache.WorldOffset =
        {
            -sourceCenter.x * kVillageMapScale,
            -minBounds.y * kVillageMapScale,
            -sourceCenter.z * kVillageMapScale
        };
        gVillageResourceCache.CameraRadius = (std::max)(
            kVillageMinCameraDistance,
            (std::max)(sourceExtents.x, sourceExtents.z) * kVillageMapScale * 2.4f);

        OutputDebugStringA("[VillageScene] Cached village map geometry/materials/textures.\n");
    }
    else
    {
        OutputDebugStringA("[VillageScene] Reusing cached village map geometry/materials/textures.\n");
    }

    const auto& villageSubsets = gVillageResourceCache.Subsets;
    const auto& materialBindings = gVillageResourceCache.MaterialBindings;
    const XMFLOAT3 worldOffset = gVillageResourceCache.WorldOffset;

    for (const Subset& subset : villageSubsets)
    {
        const bool isBrokenGroundStrip =
            subset.MaterialIndex == 0 &&
            subset.IndexCount <= 60;
        if (isBrokenGroundStrip)
        {
            continue;
        }

        auto renderItem = std::make_unique<RenderItem>();
        renderItem->World = MathHelper::Identity4x4();
        renderItem->TexTransform = MathHelper::Identity4x4();
        renderItem->Geo = resources->mGeometries["villageMapGeo"].get();
        renderItem->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
        renderItem->ObjCBIndex = static_cast<UINT>(ritems.size());
        renderItem->Visible = true;

        const std::string subsetName = "subset_" + std::to_string(subset.Id);
        auto& drawArgs = renderItem->Geo->DrawArgs[subsetName];
        renderItem->IndexCount = drawArgs.IndexCount;
        renderItem->BaseVertexLocation = drawArgs.BaseVertexLocation;
        renderItem->StartIndexLocation = drawArgs.StartIndexLocation;

        if (subset.MaterialIndex < materialBindings.size())
        {
            renderItem->Mat = resources->GetMaterial(materialBindings[subset.MaterialIndex].MaterialName);
        }
        if (renderItem->Mat == nullptr)
        {
            renderItem->Mat = resources->GetMaterial("VillageFallbackMat");
        }

        auto object = std::make_unique<GameObject>();
        object->Ritem = renderItem.get();
        object->SetScale(kVillageMapScale, kVillageMapScale, kVillageMapScale);
        object->SetRotation(kVillageRotationX, 0.0f, 0.0f);
        object->SetPosition(worldOffset.x, worldOffset.y, worldOffset.z);
        object->Update();

        TrackOwned(object.get(), renderItem.get());
        ritems.push_back(std::move(renderItem));
        objects.push_back(std::move(object));
    }

    mChatController.Initialize();
    InitializeShopUiResources();
    CreateShopKeeperNpc();

    mMapSystem = std::make_unique<MapSystem>();
    const char* floorColliderPath = ResolveVillageColliderPath(
        kVillageFloorColliderPath,
        "Models/Village/VillageFloorCollider.fbx");
    const char* wallColliderPath = ResolveVillageColliderPath(
        kVillageWallColliderPath,
        "Models/Village/VillageWallCollider.fbx");

    if (!mMapSystem->LoadFloorCollider(
        floorColliderPath,
        kVillageMapScale,
        0.0f,
        0.0f,
        0.0f,
        worldOffset.x,
        worldOffset.y + kVillageFloorColliderYOffset,
        worldOffset.z))
    {
        OutputDebugStringA("[VillageScene] Failed to load floor collider.\n");
    }
    if (std::filesystem::exists(std::filesystem::path(wallColliderPath)))
    {
        if (!mMapSystem->LoadWallCollider(
            wallColliderPath,
            kVillageMapScale,
            0.0f,
            0.0f,
            0.0f,
            worldOffset.x,
            worldOffset.y,
            worldOffset.z))
        {
            OutputDebugStringA("[VillageScene] Failed to load wall collider.\n");
        }
    }

    if (Player* player = mGame->GetPlayer())
    {
        XMFLOAT3 playerStartPosition = { 0.0f, kVillageSpawnProbeY, 0.0f };
        if (mMapSystem != nullptr)
        {
            const float floorY = mMapSystem->GetFloorHeight(
                playerStartPosition.x,
                playerStartPosition.z,
                playerStartPosition.y,
                kVillageSpawnProbeY * 2.0f);
            if (floorY > -9000.0f)
            {
                playerStartPosition.y = floorY;
            }
            else
            {
                playerStartPosition.y = kVillageFallbackSpawnY;
            }
        }

        player->SetPosition(
            playerStartPosition.x,
            playerStartPosition.y,
            playerStartPosition.z);
        player->UpdateCamera(mMapSystem.get());

        if (DebugConfig::kEnableBackendConnection)
        {
            player->ForceSendNetworkState();
        }

        XMFLOAT3 portalPosition =
        {
            kVillagePortalPosX,
            kVillagePortalPosY,
            kVillagePortalPosZ
        };

        mPortalEffect = std::make_unique<RedPortalEffect>();
        RedPortalEffect::Settings portalSettings;
        portalSettings.Position = portalPosition;
        portalSettings.PortalWidth = 1.48f;
        portalSettings.PortalHeight = 2.18f;
        portalSettings.SmokeMaxParticles = 156;
        portalSettings.SmokeSpawnRate = 108.0f;
        portalSettings.SmokeLifetimeMin = 1.45f;
        portalSettings.SmokeLifetimeMax = 2.65f;
        portalSettings.SmokeStartScaleMin = 0.42f;
        portalSettings.SmokeStartScaleMax = 0.64f;
        portalSettings.SmokeEndScaleMin = 0.72f;
        portalSettings.SmokeEndScaleMax = 1.02f;
        portalSettings.SmokeBandInnerScale = 0.80f;
        portalSettings.SmokeBandOuterScale = 1.10f;
        portalSettings.SmokeBandVerticalJitter = 0.22f;
        portalSettings.SmokeBandTangentialJitter = 0.42f;
        portalSettings.SmokeAlphaStartMin = 0.72f;
        portalSettings.SmokeAlphaStartMax = 0.96f;
        portalSettings.SparkMaxParticles = 32;
        portalSettings.SparkSpawnRate = 18.0f;
        portalSettings.SparkLifetimeMin = 0.26f;
        portalSettings.SparkLifetimeMax = 0.60f;
        portalSettings.SparkLengthMin = 0.10f;
        portalSettings.SparkLengthMax = 0.26f;
        portalSettings.SparkWidthMin = 0.018f;
        portalSettings.SparkWidthMax = 0.040f;
        portalSettings.SmokeColor = { 1.56f, 0.20f, 0.20f, 0.98f };
        portalSettings.RingInnerColor = { 2.10f, 0.22f, 0.16f, 0.98f };
        portalSettings.CenterColor = { 0.92f, 0.92f, 0.94f, 0.88f };
        portalSettings.SparkColor = { 2.40f, 2.40f, 2.40f, 1.00f };
        mPortalEffect->Init(
            mGame,
            [this](GameObject* object, RenderItem* renderItem)
            {
                TrackOwned(object, renderItem);
            },
            portalSettings);
    }

    mGame->BuildDescriptorHeaps();
}

void VillageScene::Exit()
{
    ReleaseOwnedObjects();
    mCloudLayerA = nullptr;
    mCloudLayerB = nullptr;
    mPortalEffect.reset();
    mGame->ResetLights();
    mMapSystem.reset();
    mChatController.Reset();
    gIsChatInputActive = false;
    gIsLanternUiInputActive = false;
    mShopOpen = false;
    mShopToggleKeyPressed = false;
    mShopMousePressed = false;
    mShopScrollUpKeyPressed = false;
    mShopScrollDownKeyPressed = false;
    mShopFirstVisibleIndex = 0;
    mShopStatusMessage.clear();
    mShopStatusRemaining = 0.0f;
}

void VillageScene::Update(const GameTimer& gt)
{
    const float totalTime = gt.TotalTime();
    SetCloudTexTransform(mCloudLayerA, 2.8f, 2.8f, WrapUnit(totalTime * 0.0055f), WrapUnit(totalTime * 0.0018f));
    SetCloudTexTransform(mCloudLayerB, 2.1f, 2.1f, WrapUnit(totalTime * -0.0032f), WrapUnit(totalTime * 0.0024f));
    if (mPortalEffect)
    {
        mPortalEffect->Update(gt.DeltaTime());
    }

    if (mShopStatusRemaining > 0.0f)
    {
        mShopStatusRemaining = (std::max)(0.0f, mShopStatusRemaining - gt.DeltaTime());
        if (mShopStatusRemaining <= 0.0f)
        {
            mShopStatusMessage.clear();
        }
    }

    if (DebugConfig::kEnableBackendConnection)
    {
        Player* player = mGame->GetPlayer();
        if (player != nullptr)
        {
            for (const PKT_S_GOLD_UPDATE& goldUpdate : NetworkManager::Get()->PopGoldUpdates())
            {
                if (NetworkManager::Get()->m_myPlayerId <= 0 ||
                    goldUpdate.playerId == NetworkManager::Get()->m_myPlayerId)
                {
                    player->SetGold(goldUpdate.gold);
                }
            }

            for (const PKT_S_SHOP_PURCHASE& result : NetworkManager::Get()->PopShopPurchaseResults())
            {
                player->SetGold(result.gold);

                if (result.success)
                {
                    for (ShopItem& item : mShopItems)
                    {
                        if (item.ItemId == result.shopItemId)
                        {
                            item.Purchased = true;
                            break;
                        }
                    }

                    if (result.category == SHOP_CATEGORY_WEAPON)
                    {
                        mGame->EquipPurchasedWeaponTier(static_cast<ClassTier>(std::clamp(result.weaponTier, 1, 3)));
                        SetShopStatusMessage(L"무기를 장착했습니다.", { 0.58f, 0.92f, 0.62f, 1.0f });
                    }
                    else if (result.category == SHOP_CATEGORY_ARMOR)
                    {
                        mGame->EquipPurchasedArmorTier(static_cast<ClassTier>(std::clamp(result.armorTier, 1, 3)));
                        SetShopStatusMessage(L"장비를 장착했습니다.", { 0.58f, 0.92f, 0.62f, 1.0f });
                    }
                    else
                    {
                        player->SetPotionQuickSlotsFromServer(result.potionSlots);
                        SetShopStatusMessage(L"구매가 완료되었습니다.", { 0.58f, 0.92f, 0.62f, 1.0f });
                    }
                }
                else
                {
                    switch (result.reasonCode)
                    {
                    case 2:
                        SetShopStatusMessage(L"현재 직업으로는 구매할 수 없습니다.", { 0.95f, 0.42f, 0.42f, 1.0f });
                        break;
                    case 3:
                        SetShopStatusMessage(L"레벨이 부족합니다.", { 0.95f, 0.55f, 0.42f, 1.0f });
                        break;
                    case 4:
                        SetShopStatusMessage(L"골드가 부족합니다.", { 0.98f, 0.56f, 0.36f, 1.0f });
                        break;
                    case 5:
                        SetShopStatusMessage(L"이미 구매한 상품입니다.", { 0.86f, 0.82f, 0.54f, 1.0f });
                        break;
                    default:
                        SetShopStatusMessage(L"구매할 수 없는 상품입니다.", { 0.95f, 0.42f, 0.42f, 1.0f });
                        break;
                    }
                }
            }
        }
    }

    DirectX::GraphicsMemory::Get(mGame->GetDevice()).Commit(mGame->GetCommandQueue());

    const bool hasFocus = GetForegroundWindow() == mGame->GetMainWindowHandle();
    if (!hasFocus)
    {
        mBackKeyPressed = false;
        mStage1KeyPressed = false;
        mPortalInteractKeyPressed = false;
        mPrintPositionKeyPressed = false;
        mShopToggleKeyPressed = false;
        mShopMousePressed = false;
        mShopScrollUpKeyPressed = false;
        mShopScrollDownKeyPressed = false;
        return;
    }

    if (mShopOpen)
    {
        mChatController.UpdateMessagesOnly();
    }
    else
    {
        mChatController.Update(gt);
    }

    if (auto* uiManager = mGame->GetUIManager())
    {
        if (mShopOpen)
        {
            uiManager->SetChatBoxState(false, mChatController.HasMessages());
        }
        else
        {
            uiManager->SetChatBoxState(
                mChatController.IsChatting(),
                mChatController.HasMessages());
        }
    }

    const bool shopToggleKeyDown =
        mShopFont != nullptr &&
        mShopTextureBatch != nullptr &&
        mShopTextBatch != nullptr &&
        !mChatController.IsChatting() &&
        (GetAsyncKeyState('B') & 0x8000) != 0;
    if (shopToggleKeyDown && !mShopToggleKeyPressed)
    {
        mShopOpen = !mShopOpen;
        if (mShopOpen)
        {
            gIsChatInputActive = false;
            gIsLanternUiInputActive = false;
            mShopFirstVisibleIndex = 0;
            RebuildFilteredShopItems();
            SetShopStatusMessage(L"휠 또는 화살표 키로 스크롤할 수 있습니다.", { 0.70f, 0.82f, 0.96f, 1.0f }, 1.6f);
        }
        else
        {
            SetShopStatusMessage(L"", { 0.92f, 0.92f, 0.92f, 0.0f }, 0.0f);
        }
    }
    mShopToggleKeyPressed = shopToggleKeyDown;

    if (mShopOpen)
    {
        const bool escDown = (GetAsyncKeyState(VK_ESCAPE) & 0x8000) != 0;
        if (escDown && !mBackKeyPressed)
        {
            mBackKeyPressed = true;
            mShopOpen = false;
            return;
        }
        if (!escDown)
        {
            mBackKeyPressed = false;
        }

        const bool scrollUpDown = (GetAsyncKeyState(VK_UP) & 0x8000) != 0;
        if (scrollUpDown && !mShopScrollUpKeyPressed)
        {
            AdjustShopScroll(-1);
        }
        mShopScrollUpKeyPressed = scrollUpDown;

        const bool scrollDownDown = (GetAsyncKeyState(VK_DOWN) & 0x8000) != 0;
        if (scrollDownDown && !mShopScrollDownKeyPressed)
        {
            AdjustShopScroll(1);
        }
        mShopScrollDownKeyPressed = scrollDownDown;

        const bool leftMouseDown = (GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0;
        if (leftMouseDown && !mShopMousePressed)
        {
            POINT cursor = {};
            if (GetCursorPos(&cursor) && ScreenToClient(mGame->GetMainWindowHandle(), &cursor))
            {
                HandleShopMouseClick(static_cast<float>(cursor.x), static_cast<float>(cursor.y));
            }
        }
        mShopMousePressed = leftMouseDown;

        mStage1KeyPressed = false;
        mPortalInteractKeyPressed = false;
        mPrintPositionKeyPressed = false;
        return;
    }

    if (!mChatController.IsChatting() && (GetAsyncKeyState('V') & 0x8000))
    {
        if (!mStage1KeyPressed)
        {
            mStage1KeyPressed = true;
            mGame->RequestSceneChange(std::make_unique<Stage1Scene>(mGame), L"LOADING STAGE 1");
            return;
        }
    }
    else
    {
        mStage1KeyPressed = false;
    }

    if (!mChatController.IsChatting() && (GetAsyncKeyState(VK_ESCAPE) & 0x8000))
    {
        if (!mBackKeyPressed)
        {
            mBackKeyPressed = true;
            mGame->ChangeScene(std::make_unique<MainMenuScene>(mGame));
            return;
        }
    }
    else
    {
        mBackKeyPressed = false;
    }

    Player* player = mGame->GetPlayer();
    const bool portalInteractKeyDown = !mChatController.IsChatting() && (GetAsyncKeyState('F') & 0x8000) != 0;
    if (player != nullptr && !player->IsDead() && portalInteractKeyDown && !mPortalInteractKeyPressed)
    {
        if (IsPlayerNearVillagePortal(player->GetPosition()))
        {
            if (DebugConfig::kEnableBackendConnection)
            {
                NetworkManager::Get()->SendInteractPortal();
            }
            else
            {
                mGame->RequestSceneChange(std::make_unique<Stage1Scene>(mGame), L"LOADING STAGE 1");
            }
            mPortalInteractKeyPressed = true;
            return;
        }
    }
    mPortalInteractKeyPressed = portalInteractKeyDown;

    const bool printPositionKeyDown = !mChatController.IsChatting() && (GetAsyncKeyState(VK_RETURN) & 0x8000) != 0;
    if (player != nullptr && printPositionKeyDown && !mPrintPositionKeyPressed)
    {
        mPrintPositionKeyPressed = true;
        LogPlayerPosition(player->GetPosition());
    }
    else if (!printPositionKeyDown)
    {
        mPrintPositionKeyPressed = false;
    }

    if (player != nullptr)
    {
        player->Update(gt, mMapSystem.get());
    }
}

void VillageScene::Draw(const GameTimer& gt)
{
    UNREFERENCED_PARAMETER(gt);
    bool showPortalPrompt = false;
    if (!mShopOpen)
    {
        if (Player* player = mGame->GetPlayer())
        {
            showPortalPrompt =
                !player->IsDead() &&
                !mChatController.IsChatting() &&
                IsPlayerNearVillagePortal(player->GetPosition());
        }
    }

    if (auto* uiManager = mGame->GetUIManager())
    {
        uiManager->DrawCooldownOverlay();
    }
    if (mShopOpen)
    {
        DrawShopOverlay();
        mChatController.Draw();
    }
    else
    {
        if (showPortalPrompt)
        {
            mChatController.Draw(true, false, L"[ F ] 스테이지 1 입장");
        }
        else
        {
            mChatController.Draw();
        }
    }
}

void VillageScene::OnCharInput(WPARAM charCode)
{
    mChatController.OnCharInput(charCode);
}

void VillageScene::OnTextInput(const std::wstring& text)
{
    mChatController.OnTextInput(text);
}

void VillageScene::OnCompositionInput(const std::wstring& text, bool isFinal)
{
    mChatController.OnCompositionInput(text, isFinal);
}

void VillageScene::OnMouseWheel(short delta, int x, int y)
{
    if (!mShopOpen)
    {
        return;
    }

    const D3D12_VIEWPORT viewport = mGame->GetScreenViewport();
    const UiRectF panelRect = GetShopPanelScreenRect(viewport);
    const UiRectF listRect = TransformPanelRect(GetShopListRect(), panelRect);
    if (!IsInsideRect(static_cast<float>(x), static_cast<float>(y), listRect))
    {
        return;
    }

    if (delta > 0)
    {
        AdjustShopScroll(-1);
    }
    else if (delta < 0)
    {
        AdjustShopScroll(1);
    }
}

void VillageScene::DrawShopOverlay()
{
    if (!mShopTextureBatch || !mShopTextBatch || !mShopFont)
    {
        return;
    }

    auto* resources = mGame->GetResources();
    auto* cmdList = mGame->GetCommandList();
    auto* device = mGame->GetDevice();
    auto* srvHeap = resources->GetSrvHeap();
    if (resources == nullptr || cmdList == nullptr || device == nullptr || srvHeap == nullptr)
    {
        return;
    }

    auto* panelTexture = resources->GetTexture(kShopPanelTextureName);
    if (panelTexture == nullptr || panelTexture->Resource == nullptr)
    {
        return;
    }

    const UINT descriptorSize = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
    const D3D12_VIEWPORT viewport = mGame->GetScreenViewport();
    const UiRectF panelRect = GetShopPanelScreenRect(viewport);
    const float panelScale = GetShopTextScale(panelRect);
    const float itemIconOffsetX = 6.0f * panelScale;
    const float itemIconOffsetY = -16.0f * panelScale;
    const float itemPrimaryTextOffsetY = -4.0f * panelScale;
    const float itemSecondaryTextOffsetY = -3.0f * panelScale;
    const float itemClassTextExtraOffsetX = -36.0f * panelScale;
    const float itemClassTextExtraOffsetY = -22.0f * panelScale;
    const float itemBuyButtonTextOffsetX = -10.0f * panelScale;
    const float itemBuyButtonTextOffsetY = -9.0f * panelScale;
    const float itemTextRowCorrectionY = -3.0f * panelScale;
    const UiRectF panelTitleRect = TransformPanelRect({ 104.0f, 42.0f, 894.0f, 118.0f }, panelRect);
    const UiRectF currencyRect = TransformPanelRect({ 620.0f, 236.0f, 802.0f, 311.0f }, panelRect);
    const UiRectF statusRect = TransformPanelRect(GetShopStatusRect(), panelRect);

    auto getTextureGpuHandle = [&](const std::string& textureName, D3D12_GPU_DESCRIPTOR_HANDLE& outHandle, XMUINT2& outSize) -> bool
    {
        Texture* texture = resources->GetTexture(textureName);
        const int textureIndex = resources->GetTextureIndex(textureName);
        if (texture == nullptr || texture->Resource == nullptr || textureIndex < 0)
        {
            return false;
        }

        outHandle = srvHeap->GetGPUDescriptorHandleForHeapStart();
        outHandle.ptr += static_cast<UINT64>(textureIndex) * descriptorSize;

        const D3D12_RESOURCE_DESC desc = texture->Resource->GetDesc();
        outSize = XMUINT2(static_cast<UINT>(desc.Width), static_cast<UINT>(desc.Height));
        return true;
    };

    auto drawTextureRect = [&](const std::string& textureName, const UiRectF& destRect, FXMVECTOR color, const RECT* sourceRect = nullptr)
    {
        D3D12_GPU_DESCRIPTOR_HANDLE handle = {};
        XMUINT2 textureSize = {};
        if (!getTextureGpuHandle(textureName, handle, textureSize))
        {
            return;
        }

        const RECT destination = ToRect(destRect);
        mShopTextureBatch->Draw(handle, textureSize, destination, sourceRect, color);
    };

    auto drawCenteredText = [&](const std::wstring& text, const UiRectF& rect, FXMVECTOR color, float scale)
    {
        const XMVECTOR textSize = mShopFont->MeasureString(text.c_str());
        const float textWidth = XMVectorGetX(textSize) * scale;
        const float textHeight = XMVectorGetY(textSize) * scale;
        const float x = rect.left + (UiRectWidth(rect) - textWidth) * 0.5f;
        const float y = rect.top + (UiRectHeight(rect) - textHeight) * 0.5f;
        mShopFont->DrawString(mShopTextBatch.get(), text.c_str(), XMFLOAT2(x, y), color, 0.0f, XMFLOAT2(0.0f, 0.0f), scale);
    };

    auto getTextHeight = [&](const std::wstring& text, float scale)
    {
        const XMVECTOR textSize = mShopFont->MeasureString(text.c_str());
        return XMVectorGetY(textSize) * scale;
    };

    auto fitTextScale = [&](const std::wstring& text, const UiRectF& rect, float desiredScale, float horizontalPadding, float verticalPadding)
    {
        const XMVECTOR textSize = mShopFont->MeasureString(text.c_str());
        const float textWidth = XMVectorGetX(textSize);
        const float textHeight = XMVectorGetY(textSize);
        if (textWidth <= 0.0f || textHeight <= 0.0f)
        {
            return desiredScale;
        }

        const float maxWidth = (std::max)(1.0f, UiRectWidth(rect) - horizontalPadding * 2.0f);
        const float maxHeight = (std::max)(1.0f, UiRectHeight(rect) - verticalPadding * 2.0f);
        return (std::min)(desiredScale, (std::min)(maxWidth / textWidth, maxHeight / textHeight));
    };

    auto drawLeftAlignedTextInRect = [&](const std::wstring& text, const UiRectF& rect, float leftPadding, FXMVECTOR color, float scale)
    {
        const float x = rect.left + leftPadding;
        const float y = rect.top + (UiRectHeight(rect) - getTextHeight(text, scale)) * 0.5f;
        mShopFont->DrawString(mShopTextBatch.get(), text.c_str(), XMFLOAT2(x, y), color, 0.0f, XMFLOAT2(0.0f, 0.0f), scale);
    };

    auto drawRightAlignedText = [&](const std::wstring& text, float rightX, float y, FXMVECTOR color, float scale)
    {
        const XMVECTOR textSize = mShopFont->MeasureString(text.c_str());
        const float textWidth = XMVectorGetX(textSize) * scale;
        mShopFont->DrawString(mShopTextBatch.get(), text.c_str(), XMFLOAT2(rightX - textWidth, y), color, 0.0f, XMFLOAT2(0.0f, 0.0f), scale);
    };

    auto drawRightAlignedTextInRect = [&](const std::wstring& text, const UiRectF& rect, float rightPadding, FXMVECTOR color, float scale)
    {
        const float y = rect.top + (UiRectHeight(rect) - getTextHeight(text, scale)) * 0.5f;
        drawRightAlignedText(text, rect.right - rightPadding, y, color, scale);
    };

    ID3D12DescriptorHeap* textureHeaps[] = { srvHeap };
    cmdList->SetDescriptorHeaps(1, textureHeaps);
    mShopTextureBatch->SetViewport(viewport);
    mShopTextureBatch->Begin(cmdList);

    drawTextureRect(
        kShopPanelTextureName,
        panelRect,
        DirectX::Colors::White);

    for (int visibleRow = 0; visibleRow < kShopVisibleRowCount; ++visibleRow)
    {
        const int filteredIndex = mShopFirstVisibleIndex + visibleRow;
        if (filteredIndex < 0 || filteredIndex >= static_cast<int>(mFilteredShopItemIndices.size()))
        {
            continue;
        }

        const ShopItem& item = mShopItems[mFilteredShopItemIndices[filteredIndex]];
        if (!item.IconTextureName.empty())
        {
            float iconOffsetY = itemIconOffsetY;
            if (visibleRow == 3 || visibleRow == 4)
            {
                iconOffsetY -= 8.0f * panelScale;
            }
            if (visibleRow == 4)
            {
                iconOffsetY -= 5.0f * panelScale;
            }

            const UiRectF iconRect = OffsetRect(
                InsetRect(
                    TransformPanelRect(GetShopIconRect(visibleRow), panelRect),
                    16.0f * panelScale,
                    16.0f * panelScale,
                    16.0f * panelScale,
                    16.0f * panelScale),
                itemIconOffsetX,
                iconOffsetY);
            drawTextureRect(item.IconTextureName, iconRect, DirectX::Colors::White);
        }
    }

    const int totalItemCount = static_cast<int>(mFilteredShopItemIndices.size());
    const int visibleItemCount = (std::min)(kShopVisibleRowCount, totalItemCount);
    if (totalItemCount > visibleItemCount && visibleItemCount > 0)
    {
        const UiRectF scrollTrackRect = TransformPanelRect(GetShopScrollTrackRect(), panelRect);
        const float trackHeight = UiRectHeight(scrollTrackRect);
        const float visibleRatio = static_cast<float>(visibleItemCount) / static_cast<float>(totalItemCount);
        const float thumbHeight = std::clamp(
            trackHeight * visibleRatio * 2.2f,
            42.0f * panelScale,
            620.0f * panelScale);
        const float maxScroll = static_cast<float>((std::max)(1, totalItemCount - visibleItemCount));
        const float scrollRatio = static_cast<float>(mShopFirstVisibleIndex) / maxScroll;
        const float thumbTopPadding = 18.0f * panelScale;
        const float thumbBottomPadding = 36.0f * panelScale;
        const float thumbTravelTop = scrollTrackRect.top + thumbTopPadding;
        const float thumbTravelHeight = (std::max)(0.0f, trackHeight - thumbTopPadding - thumbBottomPadding);
        const float thumbTop = thumbTravelTop + (std::max)(0.0f, thumbTravelHeight - thumbHeight) * scrollRatio;
        const UiRectF thumbRect =
        {
            scrollTrackRect.left - 3.0f * panelScale,
            thumbTop,
            scrollTrackRect.right - 16.0f * panelScale,
            thumbTop + thumbHeight
        };
        const UiRectF thumbHighlight =
        {
            thumbRect.left + 2.0f * panelScale,
            thumbRect.top + 4.0f * panelScale,
            thumbRect.right - 2.0f * panelScale,
            thumbRect.bottom - 4.0f * panelScale
        };

        drawTextureRect("white", thumbRect, XMVECTORF32{ 0.18f, 0.18f, 0.17f, 0.78f });
        drawTextureRect("white", thumbHighlight, XMVECTORF32{ 0.62f, 0.60f, 0.53f, 0.58f });
    }

    mShopTextureBatch->End();

    ID3D12DescriptorHeap* fontHeaps[] = { mShopFontHeap->Heap() };
    cmdList->SetDescriptorHeaps(1, fontHeaps);
    mShopTextBatch->SetViewport(viewport);
    mShopTextBatch->Begin(cmdList);

    drawCenteredText(kShopTitleText, panelTitleRect, DirectX::Colors::White, 1.08f * panelScale);
    drawCenteredText(GetShopCategoryLabel(mSelectedShopCategory), TransformPanelRect({ 398.0f, 118.0f, 600.0f, 178.0f }, panelRect), DirectX::Colors::LightSteelBlue, 0.74f * panelScale);

    if (Player* player = mGame->GetPlayer())
    {
        const std::wstring goldText = FormatGoldAmount(player->GetGold()) + L" G";
        drawRightAlignedTextInRect(
            goldText,
            currencyRect,
            10.0f * panelScale,
            XMVECTORF32{ 1.0f, 0.88f, 0.38f, 1.0f },
            fitTextScale(goldText, currencyRect, 1.0f * panelScale, 8.0f * panelScale, 4.0f * panelScale));
    }

    if (!mShopStatusMessage.empty())
    {
        drawCenteredText(
            mShopStatusMessage,
            statusRect,
            XMLoadFloat4(&mShopStatusColor),
            fitTextScale(mShopStatusMessage, statusRect, 0.78f * panelScale, 6.0f * panelScale, 2.0f * panelScale));
    }

    if (mFilteredShopItemIndices.empty())
    {
        drawCenteredText(
            GetShopCategoryLabel(mSelectedShopCategory) + L" 상품 준비 중",
            TransformPanelRect(GetShopEmptyMessageRect(), panelRect),
            DirectX::Colors::Gainsboro,
            0.66f * panelScale);
    }

    for (int visibleRow = 0; visibleRow < kShopVisibleRowCount; ++visibleRow)
    {
        const int filteredIndex = mShopFirstVisibleIndex + visibleRow;
        if (filteredIndex < 0 || filteredIndex >= static_cast<int>(mFilteredShopItemIndices.size()))
        {
            continue;
        }

        const ShopItem& item = mShopItems[mFilteredShopItemIndices[filteredIndex]];
        const float rowTextOffsetY = static_cast<float>(visibleRow) * itemTextRowCorrectionY;
        const UiRectF nameBarRect = OffsetRect(TransformPanelRect(GetShopNameBarRect(visibleRow), panelRect), 0.0f, itemPrimaryTextOffsetY + rowTextOffsetY);
        const UiRectF classBarRect = OffsetRect(TransformPanelRect(GetShopClassBarRect(visibleRow), panelRect), itemClassTextExtraOffsetX, itemSecondaryTextOffsetY + itemClassTextExtraOffsetY + rowTextOffsetY);
        const UiRectF priceRect = OffsetRect(TransformPanelRect(GetShopPriceRect(visibleRow), panelRect), 0.0f, itemPrimaryTextOffsetY + rowTextOffsetY);
        const UiRectF buyButtonRect = OffsetRect(TransformPanelRect(GetShopBuyButtonRect(visibleRow), panelRect), 0.0f, itemSecondaryTextOffsetY + rowTextOffsetY);

        const std::wstring levelLabel = L"Lv." + std::to_wstring(item.RequiredLevel);
        const std::wstring priceLabel = FormatGoldAmount(item.Price) + L" G";
        const UiRectF itemNameTextRect =
        {
            nameBarRect.left,
            nameBarRect.top,
            nameBarRect.right - 72.0f * panelScale,
            nameBarRect.bottom
        };
        const UiRectF itemLevelTextRect =
        {
            nameBarRect.right - 72.0f * panelScale,
            nameBarRect.top,
            nameBarRect.right,
            nameBarRect.bottom
        };

        std::wstring buttonLabel = L"구매";
        XMVECTORF32 buttonColor = { 0.84f, 0.90f, 1.0f, 1.0f };

        if (item.Purchased)
        {
            buttonLabel = L"보유";
            buttonColor = XMVECTORF32{ 0.70f, 0.92f, 0.74f, 1.0f };
        }
        else if (Player* player = mGame->GetPlayer())
        {
            if (item.AllowedClass != PlayerClass::None && player->GetClassType() != item.AllowedClass)
            {
                buttonLabel = L"잠김";
                buttonColor = XMVECTORF32{ 0.80f, 0.68f, 0.68f, 1.0f };
            }
            else if (player->GetLevel() < item.RequiredLevel)
            {
                buttonLabel = L"잠김";
                buttonColor = XMVECTORF32{ 0.82f, 0.74f, 0.62f, 1.0f };
            }
            else if (!player->HasGold(item.Price))
            {
                buttonLabel = L"부족";
                buttonColor = XMVECTORF32{ 0.95f, 0.70f, 0.42f, 1.0f };
            }
        }

        drawLeftAlignedTextInRect(
            item.Name,
            itemNameTextRect,
            19.0f * panelScale,
            DirectX::Colors::White,
            fitTextScale(item.Name, itemNameTextRect, 0.82f * panelScale, 18.0f * panelScale, 5.0f * panelScale));
        drawRightAlignedTextInRect(
            levelLabel,
            itemLevelTextRect,
            12.0f * panelScale,
            XMVECTORF32{ 0.82f, 0.82f, 0.82f, 1.0f },
            fitTextScale(levelLabel, itemLevelTextRect, 0.58f * panelScale, 8.0f * panelScale, 6.0f * panelScale));
        drawCenteredText(
            item.ClassRestriction,
            classBarRect,
            XMVECTORF32{ 0.72f, 0.72f, 0.74f, 1.0f },
            fitTextScale(item.ClassRestriction, classBarRect, 0.54f * panelScale, 8.0f * panelScale, 4.0f * panelScale));
        drawRightAlignedTextInRect(
            priceLabel,
            priceRect,
            14.0f * panelScale,
            XMVECTORF32{ 1.0f, 0.86f, 0.42f, 1.0f },
            fitTextScale(priceLabel, priceRect, 0.60f * panelScale, 12.0f * panelScale, 6.0f * panelScale));
        drawCenteredText(
            buttonLabel,
            OffsetRect(buyButtonRect, itemBuyButtonTextOffsetX, itemBuyButtonTextOffsetY),
            buttonColor,
            fitTextScale(buttonLabel, buyButtonRect, 0.90f * panelScale, 8.0f * panelScale, 4.0f * panelScale));
    }

    drawCenteredText(L"맨 위로", OffsetRect(TransformPanelRect(GetShopScrollResetButtonRect(), panelRect), 26.0f * panelScale, -44.0f * panelScale), XMVECTORF32{ 0.84f, 0.89f, 0.98f, 1.0f }, 1.44f * panelScale);
    drawCenteredText(L"닫기", OffsetRect(TransformPanelRect(GetShopCloseButtonRect(), panelRect), 26.0f * panelScale, -44.0f * panelScale), XMVECTORF32{ 0.86f, 0.86f, 0.90f, 1.0f }, 1.44f * panelScale);

    mShopTextBatch->End();
}
