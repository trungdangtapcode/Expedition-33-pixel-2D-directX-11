// ============================================================
// File: Wallet.h
// Responsibility: Own the player's durable coin balance.
//
// Design:
//   Wallet is a Meyers singleton for the same reason Inventory is one:
//   currency survives battles, menus, overworld traversal, and save/load.
//
// Save/load:
//   SaveManager serializes the integer balance as a top-level save field.
//   Loading replaces the balance instead of adding to it.
//
// Contract:
//   - Coins are always clamped to zero or above.
//   - AddCoins(amount <= 0) is a no-op.
//   - SpendCoins(amount) returns false without changing state when the
//     wallet cannot afford the request.
// ============================================================
#pragma once

class Wallet
{
public:
    static Wallet& Get();

    int GetCoins() const { return mCoins; }
    int GetDefaultCoins() const { return kDefaultCoins; }

    void ResetToDefaults();
    void SetCoins(int coins);
    void AddCoins(int amount);
    bool SpendCoins(int amount);

private:
    Wallet();

    Wallet(const Wallet&) = delete;
    Wallet& operator=(const Wallet&) = delete;

    static constexpr int kDefaultCoins = 60;
    int mCoins = 0;
};
