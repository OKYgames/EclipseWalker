#include "LanternSystem.h"

#include "Lantern.h"
#include "Player.h"

float LanternSystem::AddPickupCharge(Player* player) const
{
    return AddCharge(player, mPickupChargeAmount);
}

float LanternSystem::AddCharge(Player* player, float amount) const
{
    Lantern* lantern = GetLantern(player);
    if (lantern == nullptr || amount <= 0.0f)
    {
        return 0.0f;
    }

    const float before = lantern->GetGauge();
    lantern->AddCharge(amount);
    return lantern->GetGauge() - before;
}

bool LanternSystem::CanUseLantern(const Player* player) const
{
    return player != nullptr && player->CanUseLantern();
}

bool LanternSystem::CanTriggerWorldShift(const Player* player) const
{
    if (!CanUseLantern(player))
    {
        return false;
    }

    const Lantern* lantern = GetLantern(player);
    if (lantern == nullptr)
    {
        return false;
    }

    if (mWorldShiftChargeCost <= 0.0f)
    {
        return true;
    }

    return lantern->CanConsume(mWorldShiftChargeCost);
}

bool LanternSystem::TryConsumeWorldShift(Player* player) const
{
    if (!CanTriggerWorldShift(player))
    {
        return false;
    }

    if (mWorldShiftChargeCost <= 0.0f)
    {
        return true;
    }

    Lantern* lantern = GetLantern(player);
    return lantern != nullptr && lantern->ConsumeCharge(mWorldShiftChargeCost);
}

bool LanternSystem::ResetGauge(Player* player) const
{
    Lantern* lantern = GetLantern(player);
    if (lantern == nullptr)
    {
        return false;
    }

    lantern->ResetGauge();
    return true;
}

bool LanternSystem::TryUpgrade(Player* player) const
{
    Lantern* lantern = GetLantern(player);
    if (lantern == nullptr || !lantern->CanUpgrade())
    {
        return false;
    }

    lantern->Upgrade();
    return true;
}

float LanternSystem::GetGauge(const Player* player) const
{
    const Lantern* lantern = GetLantern(player);
    return lantern != nullptr ? lantern->GetGauge() : 0.0f;
}

float LanternSystem::GetMaxGauge(const Player* player) const
{
    const Lantern* lantern = GetLantern(player);
    return lantern != nullptr ? lantern->GetMaxGauge() : 0.0f;
}

float LanternSystem::GetGaugeRatio(const Player* player) const
{
    const Lantern* lantern = GetLantern(player);
    return lantern != nullptr ? lantern->GetGaugeRatio() : 0.0f;
}

int LanternSystem::GetLevel(const Player* player) const
{
    const Lantern* lantern = GetLantern(player);
    return lantern != nullptr ? lantern->GetLevel() : 0;
}

void LanternSystem::SetPickupChargeAmount(float amount)
{
    if (amount >= 0.0f)
    {
        mPickupChargeAmount = amount;
    }
}

void LanternSystem::SetWorldShiftChargeCost(float amount)
{
    if (amount >= 0.0f)
    {
        mWorldShiftChargeCost = amount;
    }
}

Lantern* LanternSystem::GetLantern(Player* player) const
{
    return player != nullptr ? player->GetLantern() : nullptr;
}

const Lantern* LanternSystem::GetLantern(const Player* player) const
{
    return player != nullptr ? player->GetLantern() : nullptr;
}
