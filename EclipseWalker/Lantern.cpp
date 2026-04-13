#include "Lantern.h"
#include <algorithm>

void Lantern::AddCharge(float amount)
{
    if (amount <= 0.0f) return;
    mGauge = std::min(mGauge + amount, mMaxGauge);
}

bool Lantern::ConsumeCharge(float amount)
{
    if (!CanConsume(amount)) return false;
    mGauge -= amount;
    return true;
}

bool Lantern::CanConsume(float amount) const
{
    return amount <= mGauge;
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
}
