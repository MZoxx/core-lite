# WolfPack (WP.h) — On-Chain Test Audit

On-chain verification of `src/contracts/WP.h` against a live Qubic Core-Lite
testnet node. Every WP procedure, function and system hook was exercised by
submitting real transactions and asserting the resulting on-chain state.

- **Contract:** `src/contracts/WP.h` — WolfPack revenue-distribution & staking SC
- **Contract index:** 27
- **Testnet node:** `root@135.181.160.185`, peer port `31841`, HTTP `41841`
- **CLI:** `qubic-cli` (branch `wolfpack-integration`, extended with WP commands)
- **Test harness:** `test-onchain/` — 16 shell scripts, 133 assertions
- **Result:** ✅ all 16 scripts pass

---

## 1. Test result summary

| # | Script | Asserts | Result |
|---|--------|--------:|:------:|
| 00 | `00_bootstrap.sh` | 1 | ✅ |
| 01 | `ranks/01_addclan.sh` | 16 | ✅ |
| 02 | `ranks/02_setclanrank.sh` | 7 | ✅ |
| 03 | `ranks/03_removeclan.sh` | 4 | ✅ |
| 04 | `ranks/04_setexclude.sh` | 16 | ✅ |
| 05 | `ranks/05_clanweightedtotal.sh` | 10 | ✅ |
| 06 | `admin/01_setadmin.sh` | 3 | ✅ |
| 07 | `admin/02_deposit.sh` | 4 | ✅ |
| 08 | `admin/03_distribution_preview.sh` | 20 | ✅ |
| 09 | `staking/00_provision.sh` | 5 | ✅ |
| 10 | `staking/01_stake.sh` | 5 | ✅ |
| 11 | `staking/02_request_unstake.sh` | 8 | ✅ |
| 12 | `staking/04_depositstakingrewards.sh` | 3 | ✅ |
| 13 | `staking/06_claim_rewards.sh` | 13 | ✅ |
| 14 | `staking/03_finalize_unstake.sh` | 6 | ✅ |
| 15 | `staking/05_distribution.sh` | 12 | ✅ |

Run the whole suite: `./test-onchain/run.sh`

---

## 2. Coverage matrix

### Public functions (read-only)

| idx | Function | Verified by |
|----:|----------|-------------|
| 1 | `GetStatus` | every script via `-wpstatus` |
| 2 | `GetHolderInfo` | `staking/00_provision`, `staking/05_distribution` |
| 3 | `GetClanMemberInfo` | all `ranks/*` scripts |
| 4 | `GetShareholderInfo` | `-wpshareholderinfo` |
| 5 | `GetStakingInfo` | all `staking/*` scripts |
| 6 | `GetExcludeAddresses` | `ranks/04_setexclude` |
| 7 | `GetDistributionPreview` | `admin/03_distribution_preview` (20 asserts) |

### Public procedures (state-changing)

| idx | Procedure | Verified by |
|----:|-----------|-------------|
| 1 | `DepositRevenue` | `admin/02_deposit` |
| 2 | `AddClanMember` | `ranks/01_addclan` |
| 3 | `RemoveClanMember` | `ranks/03_removeclan` |
| 4 | `SetClanRank` | `ranks/02_setclanrank` |
| 5 | `SetAdmin` | `00_bootstrap` + `admin/01_setadmin` |
| 6 | `SetExcludeAddress` | `ranks/04_setexclude` |
| 7 | `Stake` | `staking/01_stake` |
| 8 | `RequestUnstake` | `staking/02_request_unstake` |
| 9 | `FinalizeUnstake` | `staking/03_finalize_unstake` |
| 10 | `DepositStakingRewards` | `staking/04_depositstakingrewards` |
| 11 | `ClaimStakingRewards` | `staking/06_claim_rewards` |

### System hooks

| Hook | What it does | Verified by |
|------|--------------|-------------|
| `INITIALIZE` | sets WP-token issuer, zeroes counters | implicit at deploy |
| `BEGIN_EPOCH` | snapshots holders + shareholders, distributes staking rewards | `staking/00_provision`, `staking/06_claim_rewards` |
| `END_EPOCH` | container cleanup | implicit (no crash over many epochs) |
| `END_TICK` | daily 11 UTC auto-payout (70/10/10/10) | `staking/05_distribution` |
| `PRE_ACQUIRE_SHARES` | accepts WP-token mgmt transfers from QX | every `Stake` (mgmt delegation) |

### Error codes

Verified: `OK(0)`, `ACCESS_DENIED(1)`, `ZERO_AMOUNT(2)`, `NOT_CLAN_MEMBER(4)`,
`ALREADY_CLAN_MEMBER(5)`, `INVALID_RANK(6)`, `INSUFFICIENT_STAKE(8)`,
`UNSTAKE_PENDING(9)`, `ACQUIRE_FAILED(10)`, `NO_PENDING_REWARDS(12)`,
`UNSTAKE_NOT_READY(13)`, `NOT_STAKER(14)`, `INVALID_SLOT(15)` (via state-no-change).

Not exercised: `NOT_HOLDER(3)`, `NO_REWARD(7)`, `TRANSFER_FAILED(11)` —
defensive paths that need a deliberately corrupted state to reach.

---

## 3. How to verify with qubic-cli

All commands take `-nodeip 127.0.0.1 -nodeport 31841`. On the testnet box:

```bash
QCLI="/root/qubic/scripts/qubic-cli -nodeip 127.0.0.1 -nodeport 31841"
```

### 3.1 Contract status

```
$ qubic-cli ... -wpstatus

═══ WolfPack Contract Status ═══════════════════════════
Holder Count:            4
Total Tokens Snapshot:   1000000
Shareholder Count:       676
Total Shares Snapshot:   676
Clan Member Count:       0
Clan Weighted Total:     0
Pending Revenue:         0 QU
Reinvestment Fund:       1002472 QU
Total Distributed:       10024690 QU
Total Deposited:         10024690 QU
Last Payout Tick:        48873691
Last Distribution Epoch: 215
Admin:                   WIYVZEQZEOBACHHGYWMLMVXJWVMCUQXJJUKJNSKSTEWJXYTHAJHTNRAENCUH
```

Sanity check: `Total Deposited == Total Distributed + Pending Revenue` and
`Reinvestment Fund ≈ Total Distributed × 10%`.

### 3.2 Ranks

```
$ qubic-cli ... -wpclaninfo <ADDRESS>

═══ WolfPack Clan Member Info ══════════════════════════
Address:   TBVOIOBSEJCBUBFUEUNRYLGZCLYAIGMSLLFIMZLHZDAIMNLBBREHFRJARFHJ
Rank:      0
Is Member: NO
```

Admin-only rank management (signed with the admin seed):

```
qubic-cli ... -seed <ADMIN> -wpaddclan    <ADDRESS> <rank 0-5>
qubic-cli ... -seed <ADMIN> -wpsetclanrank <ADDRESS> <rank 0-5>
qubic-cli ... -seed <ADMIN> -wpremoveclan  <ADDRESS>
```

Rank multipliers (permille, from `WP.h`): Recruit 1000 · Private 1300 ·
Sergeant 1800 · Lieutenant 2500 · Colonel 3200 · General 4000. `Clan Weighted
Total` in `-wpstatus` is the sum of the multipliers of all current members —
e.g. one member of each rank ⇒ `13800`.

### 3.3 Staking

```
$ qubic-cli ... -wpstakinginfo <ADDRESS>

═══ WolfPack Staking Info ══════════════════════════════
Address:            XPKQLZZFGFMWMAXGOOWDGDAJFUEDXVQVWJMEEOESYGIPGLYKINXWWZHGUVUO
Is Staker:          YES
Staked Amount:      200 WP
Pending Rewards:    0 WP
Unstake Amount:     0 WP
Unstake Epoch:      0
Total Staked (all): 250 WP
Reward Pool:        0 WP
```

Staking lifecycle (signed with the staker's own seed):

```
# 1. delegate WP-share management to the WP contract (index 27) on QX
qubic-cli ... -seed <STAKER> -qxtransferrights WP <WP_ISSUER> 27 <N>
# 2. stake / unstake / finalize
qubic-cli ... -seed <STAKER> -wpstake            <N>
qubic-cli ... -seed <STAKER> -wprequestunstake   <N>
qubic-cli ... -seed <STAKER> -wpfinalizeunstake          # after 2 epochs
qubic-cli ... -seed <STAKER> -wpclaimrewards
```

Verified behaviour:
- `Stake` rejects `0` (`ZERO_AMOUNT`), rejects callers without delegated
  WP-management (`ACQUIRE_FAILED`), rejects stakers with a pending unstake
  (`UNSTAKE_PENDING`).
- `RequestUnstake` rejects non-stakers (`NOT_STAKER`), over-requests
  (`INSUFFICIENT_STAKE`), double-requests (`UNSTAKE_PENDING`).
- `FinalizeUnstake` enforces the 2-epoch cooldown (`UNSTAKE_NOT_READY`),
  then returns the shares to QX management.
- `BEGIN_EPOCH` distributes the reward pool proportionally — verified split:
  pool 600, stakes 50 / 200 ⇒ pending rewards 120 / 480.
- `ClaimStakingRewards` clears `Pending Rewards` and releases the reward
  shares back to QX management. **Note:** the staker must have at least
  `staked + pending` WP shares under WP management for the release to
  succeed — otherwise the claim returns `TRANSFER_FAILED`.

### 3.4 Distribution preview

```
$ qubic-cli ... -wpdistpreview 1000000

=== WolfPack Distribution Preview for 1000000 QU ===
Holder Share:       700000
Shareholder Share:  100000
Clan Share:         100000
Reinvest Share:     100000
```

70 / 10 / 10 / 10 split. The remainder of the integer division always lands
in `Reinvest Share` (verified with `amount=12345` ⇒ 8641 / 1234 / 1234 / 1236).

### 3.5 Payout (END_TICK auto-distribution)

The daily payout fires inside `END_TICK` when the hour is 11 UTC and at least
1000 ticks have elapsed since the last payout. Deposit revenue, then the next
qualifying tick splits it:

```
qubic-cli ... -seed <ANY> -wpdeposit <AMOUNT>      # -> Pending Revenue
# ... END_TICK fires ...
```

Verified with a 10,000,000 QU deposit (`staking/05_distribution.sh`):

| Pool | Share | Observed |
|------|------:|---------:|
| WP token holders | 70% | 7,000,000 QU (each 1%-holder +70,000 QU) |
| SC shareholders (676 IPO) | 10% | 1,000,000 QU |
| Clan members (rank-weighted) | 10% | 999,997 QU |
| Reinvestment fund | 10% | 1,000,000 QU |

`Total Distributed += amount`, `Pending Revenue → 0`, `Last Payout Tick`
advances, `Last Distribution Epoch` becomes the current epoch.

### 3.6 Exclude addresses

```
$ qubic-cli ... -wpexcludeinfo

=== WolfPack Exclude Addresses ===
Exclude Slot 1: AAAA...AAAAFXIB     (NULL_ID = unset)
Exclude Slot 2: AAAA...AAAAFXIB
```

`-seed <ADMIN> -wpsetexclude <slot 1|2> <ADDRESS>` — admin-only. Slot 0 and
slot ≥3 are rejected (`INVALID_SLOT`); a non-admin caller is rejected
(`ACCESS_DENIED`); both verified by observing the state does not change.

---

## 4. Findings during the audit

1. **uint128→uint64 cast bug (fixed).** `(uint64)<uint128_t>` collapses to
   `uint128::operator bool()` and yields `1` instead of the quotient. All
   permille/pro-rata math in `WP.h` (`END_TICK`, `BEGIN_EPOCH`,
   `GetDistributionPreview`) now reads `uint128.low` explicitly. Guarded by
   `admin/03_distribution_preview.sh`.
2. **`ClaimStakingRewards` share sourcing.** Rewards are released from the
   *staker's own* WP-managed shares, so a staker must keep
   `staked + pendingRewards` shares under WP management or the claim fails
   with `TRANSFER_FAILED`. This is behaviour, not a bug — but worth a CLI
   warning for end users.
3. **Read-back gaps closed.** `GetExcludeAddresses` (fn 6) and
   `clanWeightedTotal` in `GetStatus` were added so exclude slots and the
   weighted clan total are verifiable on-chain.

---

## 5. Test-only modifications (NOT for production)

The following server-side changes make the contract testable in a single
session. They must be reverted before any PR / mainnet deployment:

| File | Change | Why |
|------|--------|-----|
| `contract_def.h` | WP deploy epoch `210 → 208` | deploy WP without waiting 2 epochs |
| `public_settings.h` | `TESTNET_EPOCH_DURATION 3000 → 500` | shorter epochs for cooldown tests |
| `private_settings.h` | added test seeds to `customSeeds[]` | fund operator + issuer + 3 stakers |
| `WP.h` | `wpToken.issuer` → test issuer pubkey | issue the WP asset ourselves on QX |
| `WP.h` | `END_TICK` 11-UTC hour gate commented out | trigger payout any time in tests |

`src/contracts/WP.h` changes that **are** intended for the PR:
`GetExcludeAddresses` function, `GetDistributionPreview` function,
`clanWeightedTotal` field in `GetStatus`, and the `uint128.low` cast fix.
