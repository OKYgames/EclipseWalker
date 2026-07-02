#include "Lantern.h"
#include "DebugConfig.h"

#include <algorithm>

Lantern::Lantern()
{
    ApplyDebugGaugeOverride();
}

void Lantern::AddCharge(float amount)
{
    if (amount <= 0.0f) return;
    mGauge = std::min(mGauge + amount, mMaxGauge);
    ApplyDebugGaugeOverride();
}

bool Lantern::ConsumeCharge(float amount)
{
    if (!CanConsume(amount)) return false;
    mGauge -= amount;
    ApplyDebugGaugeOverride();
    return true;
}

bool Lantern::CanConsume(float amount) const
{
    return amount <= mGauge;
}

void Lantern::ResetGauge()
{
    mGauge = 0.0f;
    ApplyDebugGaugeOverride();
}

bool Lantern::CanUpgrade() const
{
    return mGauge >= mMaxGauge;
}

void Lantern::Upgrade()
{
    if (!CanUpgrade()) return;

    ++mLevel;
    mGauge = 0.0f;
    mMaxGauge += 50.0f;
    ApplyDebugGaugeOverride();
}

void Lantern::SetState(float gauge, float maxGauge, int level)
{
    mMaxGauge = std::max(1.0f, maxGauge);
    mGauge = std::clamp(gauge, 0.0f, mMaxGauge);
    mLevel = std::max(1, level);
    ApplyDebugGaugeOverride();
}

void Lantern::ApplyDebugGaugeOverride()
{
    if (DebugConfig::kDebugFullLanternGauge)
    {
        mGauge = mMaxGauge;
    }
}
