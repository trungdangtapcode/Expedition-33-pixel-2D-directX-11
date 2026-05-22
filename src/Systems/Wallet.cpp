// ============================================================
// File: Wallet.cpp
// Responsibility: Implement the durable coin balance service.
// ============================================================
#include "Wallet.h"
#include "../Utils/Log.h"

Wallet& Wallet::Get()
{
    static Wallet instance;
    return instance;
}

Wallet::Wallet()
{
    ResetToDefaults();
}

void Wallet::ResetToDefaults()
{
    mCoins = kDefaultCoins;
    LOG("[Wallet] Starter balance ready: %d coins.", mCoins);
}

void Wallet::SetCoins(int coins)
{
    mCoins = (coins < 0) ? 0 : coins;
    LOG("[Wallet] Balance set to %d coins.", mCoins);
}

void Wallet::AddCoins(int amount)
{
    if (amount <= 0) return;
    mCoins += amount;
    LOG("[Wallet] Added %d coins. Balance: %d.", amount, mCoins);
}

bool Wallet::SpendCoins(int amount)
{
    if (amount <= 0) return true;
    if (mCoins < amount) return false;

    mCoins -= amount;
    LOG("[Wallet] Spent %d coins. Balance: %d.", amount, mCoins);
    return true;
}
