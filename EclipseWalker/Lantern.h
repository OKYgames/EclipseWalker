#pragma once

class Lantern
{
public:
    void AddCharge(float amount);
    bool ConsumeCharge(float amount);
    bool CanConsume(float amount) const;

    bool CanUpgrade() const;
    void Upgrade();

    float GetGauge() const { return mGauge; }
    float GetMaxGauge() const { return mMaxGauge; }
    int GetLevel() const { return mLevel; }
    float GetGaugeRatio() const { return (mMaxGauge > 0.0f) ? (mGauge / mMaxGauge) : 0.0f; }

private:
    float mGauge = 0.0f;
    float mMaxGauge = 100.0f;
    int mLevel = 1;
};
