# WolfPack (WP) Smart Contract

**Contract file:** `src/contracts/WP.h`
**Network:** Qubic (Core Lite Testnet)
**Status:** Pre-launch — not yet live on mainnet

---

## Overview

The WolfPack contract has two core features:

1. **Revenue Distribution** — incoming QU is split and paid out daily to WP token holders, SC shareholders, and clan members
2. **WP Token Staking** — users stake WP tokens to earn a fixed weekly reward from the staking pool

---

## Revenue Distribution

Revenue sent to the contract via `DepositRevenue` is held as `pendingRevenue` and paid out automatically every day at **11:00 UTC** (via `END_TICK`).

### Split

| Pool | Share | Recipients |
|------|-------|-----------|
| Token Holders | 70% | WP token holders — proportional to balance |
| SC Shareholders | 10% | 676 IPO shares (issuer = NULL_ID) — proportional to shares |
| Clan Members | 10% | Active clan members — weighted by rank multiplier |
| Reinvestment Fund | 10% | Held inside the contract |

### Snapshots

Holder and shareholder balances are **snapshotted at `BEGIN_EPOCH`** via `AssetPossessionIterator`. Payouts use the snapshot — not live balances.

### Payout Timing

- Trigger: `END_TICK` when `qpi.hour() == 11` (UTC)
- Minimum interval: 1000 ticks between payouts (prevents double-payout)
- Only triggers if `pendingRevenue > 0`

---

## Staking

### How It Works

1. User calls `QX.TransferShareManagementRights` → hands WP shares to the WP contract
2. User calls `Stake(numberOfShares)` → shares are recorded as staked
3. At every `BEGIN_EPOCH`, staking rewards are distributed proportionally to all stakers
4. User calls `ClaimStakingRewards` → WP tokens sent back to wallet (100 QU QX fee)

### Unstaking

1. Call `RequestUnstake(numberOfShares)` → tokens stop earning, locked for 2 epochs
2. Wait **2 epochs**
3. Call `FinalizeUnstake` → tokens returned to QX management (100 QU fee)

> ⚠️ You cannot stake while an unstake request is pending.

### Numbers

| Parameter | Value |
|-----------|-------|
| Reward per epoch | 1,923,076 WP (~100M / 52 epochs) |
| Unstake delay | 2 epochs |
| QX fee (claim/unstake) | 100 QU |
| Max stakers | 16,384 |

---

## Clan System

Clan members receive 10% of revenue, weighted by rank multiplier.

| Rank | Name | Multiplier |
|------|------|-----------|
| 0 | Recruit | 1.0× |
| 1 | Private | 1.3× |
| 2 | Sergeant | 1.8× |
| 3 | Lieutenant | 2.5× |
| 4 | Colonel | 3.2× |
| 5 | General | 4.0× |

Clan members are managed by the admin via `AddClanMember`, `RemoveClanMember`, `SetClanRank`.

---

## Contract Functions

### Read-only (Functions)

| Function | Description |
|----------|-------------|
| `GetStatus` | Contract overview: holder count, pending revenue, last payout, etc. |
| `GetHolderInfo(address)` | WP token balance in snapshot |
| `GetShareholderInfo(address)` | SC share balance in snapshot |
| `GetClanMemberInfo(address)` | Clan rank |
| `GetStakingInfo(address)` | Staked amount, pending rewards, unstake status |

### State-modifying (Procedures)

| Procedure | Description |
|-----------|-------------|
| `DepositRevenue` | Send QU to the contract revenue pool |
| `Stake(numberOfShares)` | Stake WP tokens (management rights must be transferred first via QX) |
| `RequestUnstake(numberOfShares)` | Begin 2-epoch unstake cooldown |
| `FinalizeUnstake` | Complete unstake after cooldown (100 QU fee) |
| `ClaimStakingRewards` | Claim accumulated staking rewards (100 QU fee) |
| `DepositStakingRewards(numberOfShares)` | Add WP tokens to the staking reward pool |
| `AddClanMember(address, rank)` | Admin only |
| `RemoveClanMember(address)` | Admin only |
| `SetClanRank(address, rank)` | Admin only |
| `SetAdmin(address)` | Transfer admin (bootstrap: free when admin = NULL_ID) |
| `SetExcludeAddress(slot, address)` | Exclude address from distributions (slot 1 or 2) |

---

## Token

| Parameter | Value |
|-----------|-------|
| Asset name | `WP` |
| Issuer | `MLMWPSQNVAIBR FDHWCKSFOVUAZDDWKJGCLRSYZIUEFDURPWIPQXACYOE` |
| Max supply | 100,000,000 WP |
| IPO shares | 676 (issuer = NULL_ID) |

---

## Admin Setup

`INITIALIZE()` leaves `adminAddress = NULL_ID`.
The first call to `SetAdmin(newAdmin)` from **any address** sets the admin (bootstrap).
After that, only the current admin can change it.

---

## Error Codes

| Code | Name |
|------|------|
| 0 | OK |
| 1 | ACCESS_DENIED |
| 2 | ZERO_AMOUNT |
| 3 | NOT_HOLDER |
| 4 | NOT_CLAN_MEMBER |
| 5 | ALREADY_CLAN_MEMBER |
| 6 | INVALID_RANK |
| 7 | NO_REWARD |
| 8 | INSUFFICIENT_STAKE |
| 9 | UNSTAKE_PENDING |
| 10 | ACQUIRE_FAILED |
| 11 | TRANSFER_FAILED |
| 12 | NO_PENDING_REWARDS |
| 13 | UNSTAKE_NOT_READY |
| 14 | NOT_STAKER |
| 15 | INVALID_SLOT |
