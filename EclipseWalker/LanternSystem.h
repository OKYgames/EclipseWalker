#pragma once

class Lantern;
class Player;

class LanternSystem
{
public:
    float AddPickupCharge(Player* player) const;
    float AddCharge(Player* player, float amount) const;

    bool CanUseLantern(const Player* player) const;
    bool CanTriggerWorldShift(const Player* player) const;
    bool TryConsumeWorldShift(Player* player) const;
    bool ResetGauge(Player* player) const;
    bool TryUpgrade(Player* player) const;

    float GetGauge(const Player* player) const;
    float GetMaxGauge(const Player* player) const;
    float GetGaugeRatio(const Player* player) const;
    int GetLevel(const Player* player) const;

    float GetPickupChargeAmount() const { return mPickupChargeAmount; }
    float GetWorldShiftChargeCost() const { return mWorldShiftChargeCost; }

    void SetPickupChargeAmount(float amount);
    void SetWorldShiftChargeCost(float amount);

private:
    Lantern* GetLantern(Player* player) const;
    const Lantern* GetLantern(const Player* player) const;

private:
    float mPickupChargeAmount = 35.0f;
    float mWorldShiftChargeCost = 250.0f;
};
