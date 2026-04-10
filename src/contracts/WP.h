using namespace QPI;

// ============================================================================
// WolfPack (WP) - Revenue Distribution Smart Contract
//
// Revenue split:
//   70% -> WP token holders (proportional to token holdings)
//   10% -> SC shareholders (676 SC shareholders, issuer=NULL_ID)
//   10% -> Active clan members (rank multiplier booster)
//   10% -> Reinvestment fund
//
// WP token holders are snapshotted at BEGIN_EPOCH via AssetPossessionIterator.
// SC shareholders (IPO shares) are also snapshotted at BEGIN_EPOCH separately.
// ============================================================================

// --- Constants ---
constexpr uint64 WOLFPACK_MAX_HOLDERS = 16384;
constexpr uint64 WOLFPACK_MAX_SHAREHOLDERS = 1024;
constexpr uint64 WOLFPACK_MAX_CLAN_MEMBERS = 8192;
constexpr uint64 WOLFPACK_DISTRIBUTION_PERMILLE_HOLDERS = 700;
constexpr uint64 WOLFPACK_DISTRIBUTION_PERMILLE_SHAREHOLDERS = 100;
constexpr uint64 WOLFPACK_DISTRIBUTION_PERMILLE_CLAN = 100;
constexpr uint64 WOLFPACK_DISTRIBUTION_PERMILLE_REINVEST = 100;
constexpr uint64 WOLFPACK_SC_ASSET_NAME = 20567ULL; // "WP" as uint64

// Payout timing
constexpr uint8 WOLFPACK_PAYOUT_HOUR = 11; // 11:00 UTC
constexpr uint64 WOLFPACK_MIN_PAYOUT_INTERVAL_TICKS = 1000; // prevent double-payout in same hour

// Return codes
constexpr uint32 WOLFPACK_OK = 0;
constexpr uint32 WOLFPACK_ERROR_ACCESS_DENIED = 1;
constexpr uint32 WOLFPACK_ERROR_ZERO_AMOUNT = 2;
constexpr uint32 WOLFPACK_ERROR_NOT_HOLDER = 3;
constexpr uint32 WOLFPACK_ERROR_NOT_CLAN_MEMBER = 4;
constexpr uint32 WOLFPACK_ERROR_ALREADY_CLAN_MEMBER = 5;
constexpr uint32 WOLFPACK_ERROR_INVALID_RANK = 6;
constexpr uint32 WOLFPACK_ERROR_NO_REWARD = 7;

// Rank multipliers (in permille: 1000 = 1x, 1200 = 1.2x, etc.)
constexpr uint64 WOLFPACK_RANK_MULTIPLIER_0 = 1000;
constexpr uint64 WOLFPACK_RANK_MULTIPLIER_1 = 1200;
constexpr uint64 WOLFPACK_RANK_MULTIPLIER_2 = 1500;
constexpr uint64 WOLFPACK_RANK_MULTIPLIER_3 = 2000;
constexpr uint64 WOLFPACK_RANK_MULTIPLIER_4 = 3000;
constexpr uint64 WOLFPACK_MAX_RANK = 4;

struct WOLFPACK2
{
};

struct WOLFPACK : public ContractBase
{
    // ======================== STATE ========================
    struct StateData
    {
        id adminAddress;

        // WP token asset reference (external token on QX)
        Asset wpToken;

        // Token holder snapshot (taken at BEGIN_EPOCH) - 70% pool
        HashMap<id, uint64, WOLFPACK_MAX_HOLDERS> holderBalances;
        uint64 totalTokensSnapshot;
        uint64 holderCount;

        // SC shareholder snapshot (taken at BEGIN_EPOCH) - 10% pool
        // These are the 676 IPO shares (issuer=NULL_ID, name="WP")
        HashMap<id, uint64, WOLFPACK_MAX_SHAREHOLDERS> shareholderBalances;
        uint64 totalSharesSnapshot;
        uint64 shareholderCount;

        // Clan system
        HashMap<id, uint64, WOLFPACK_MAX_CLAN_MEMBERS> clanRanks;
        uint64 clanMemberCount;
        uint64 clanWeightedTotal;

        // Revenue tracking
        uint64 pendingRevenue;
        uint64 reinvestmentFund;
        uint64 totalDistributed;
        uint64 totalDeposited;
        uint64 lastDistributionEpoch;
        uint64 lastPayoutTick;

        // Exclude addresses from distribution
        id excludeAddress1;
        id excludeAddress2;
    };

    // ======================== INPUT / OUTPUT ========================

    struct DepositRevenue_input { };
    struct DepositRevenue_output { uint32 returnCode; };

    struct AddClanMember_input { id memberAddress; uint64 rank; };
    struct AddClanMember_output { uint32 returnCode; };

    struct RemoveClanMember_input { id memberAddress; };
    struct RemoveClanMember_output { uint32 returnCode; };
    struct RemoveClanMember_locals { uint64 rank; };

    struct SetClanRank_input { id memberAddress; uint64 rank; };
    struct SetClanRank_output { uint32 returnCode; };
    struct SetClanRank_locals { uint64 oldRank; };

    struct SetAdmin_input { id newAdmin; };
    struct SetAdmin_output { uint32 returnCode; };

    struct SetExcludeAddress_input { uint64 slot; id address; };
    struct SetExcludeAddress_output { uint32 returnCode; };

    struct GetStatus_input { };
    struct GetStatus_output
    {
        uint64 holderCount;
        uint64 totalTokensSnapshot;
        uint64 shareholderCount;
        uint64 totalSharesSnapshot;
        uint64 clanMemberCount;
        uint64 pendingRevenue;
        uint64 reinvestmentFund;
        uint64 totalDistributed;
        uint64 totalDeposited;
        uint64 lastPayoutTick;
        uint64 lastDistributionEpoch;
        id adminAddress;
    };

    struct GetHolderInfo_input { id holderAddress; };
    struct GetHolderInfo_output { uint64 tokenBalance; uint32 isHolder; };
    struct GetHolderInfo_locals { uint64 val; };

    struct GetShareholderInfo_input { id shareholderAddress; };
    struct GetShareholderInfo_output { uint64 shares; uint32 isShareholder; };
    struct GetShareholderInfo_locals { uint64 val; };

    struct GetClanMemberInfo_input { id memberAddress; };
    struct GetClanMemberInfo_output { uint64 rank; uint32 isMember; };
    struct GetClanMemberInfo_locals { uint64 val; };

    // ======================== FUNCTIONS (read-only) ========================

    PUBLIC_FUNCTION(GetStatus)
    {
        output.holderCount = state.get().holderCount;
        output.totalTokensSnapshot = state.get().totalTokensSnapshot;
        output.shareholderCount = state.get().shareholderCount;
        output.totalSharesSnapshot = state.get().totalSharesSnapshot;
        output.clanMemberCount = state.get().clanMemberCount;
        output.pendingRevenue = state.get().pendingRevenue;
        output.reinvestmentFund = state.get().reinvestmentFund;
        output.totalDistributed = state.get().totalDistributed;
        output.totalDeposited = state.get().totalDeposited;
        output.lastPayoutTick = state.get().lastPayoutTick;
        output.lastDistributionEpoch = state.get().lastDistributionEpoch;
        output.adminAddress = state.get().adminAddress;
    }

    PUBLIC_FUNCTION_WITH_LOCALS(GetHolderInfo)
    {
        output.isHolder = state.get().holderBalances.get(input.holderAddress, locals.val) ? 1 : 0;
        if (output.isHolder)
        {
            output.tokenBalance = locals.val;
        }
    }

    PUBLIC_FUNCTION_WITH_LOCALS(GetShareholderInfo)
    {
        output.isShareholder = state.get().shareholderBalances.get(input.shareholderAddress, locals.val) ? 1 : 0;
        if (output.isShareholder)
        {
            output.shares = locals.val;
        }
    }

    PUBLIC_FUNCTION_WITH_LOCALS(GetClanMemberInfo)
    {
        output.isMember = state.get().clanRanks.get(input.memberAddress, locals.val) ? 1 : 0;
        if (output.isMember)
        {
            output.rank = locals.val;
        }
    }

    // ======================== PROCEDURES (state-modifying) ========================

    PUBLIC_PROCEDURE(DepositRevenue)
    {
        if (qpi.invocationReward() == 0)
        {
            output.returnCode = WOLFPACK_ERROR_ZERO_AMOUNT;
            return;
        }
        state.mut().pendingRevenue = state.get().pendingRevenue + qpi.invocationReward();
        state.mut().totalDeposited = state.get().totalDeposited + qpi.invocationReward();
        output.returnCode = WOLFPACK_OK;
    }

    PUBLIC_PROCEDURE(AddClanMember)
    {
        if (qpi.invocator() != state.get().adminAddress)
        {
            output.returnCode = WOLFPACK_ERROR_ACCESS_DENIED;
            return;
        }
        if (state.get().clanRanks.contains(input.memberAddress))
        {
            output.returnCode = WOLFPACK_ERROR_ALREADY_CLAN_MEMBER;
            return;
        }
        if (input.rank > WOLFPACK_MAX_RANK)
        {
            output.returnCode = WOLFPACK_ERROR_INVALID_RANK;
            return;
        }

        state.mut().clanRanks.set(input.memberAddress, input.rank);
        state.mut().clanMemberCount = state.get().clanMemberCount + 1;

        if (input.rank == 0) state.mut().clanWeightedTotal = state.get().clanWeightedTotal + WOLFPACK_RANK_MULTIPLIER_0;
        if (input.rank == 1) state.mut().clanWeightedTotal = state.get().clanWeightedTotal + WOLFPACK_RANK_MULTIPLIER_1;
        if (input.rank == 2) state.mut().clanWeightedTotal = state.get().clanWeightedTotal + WOLFPACK_RANK_MULTIPLIER_2;
        if (input.rank == 3) state.mut().clanWeightedTotal = state.get().clanWeightedTotal + WOLFPACK_RANK_MULTIPLIER_3;
        if (input.rank == 4) state.mut().clanWeightedTotal = state.get().clanWeightedTotal + WOLFPACK_RANK_MULTIPLIER_4;

        output.returnCode = WOLFPACK_OK;
    }

    PUBLIC_PROCEDURE_WITH_LOCALS(RemoveClanMember)
    {
        if (qpi.invocator() != state.get().adminAddress)
        {
            output.returnCode = WOLFPACK_ERROR_ACCESS_DENIED;
            return;
        }
        if (!state.get().clanRanks.get(input.memberAddress, locals.rank))
        {
            output.returnCode = WOLFPACK_ERROR_NOT_CLAN_MEMBER;
            return;
        }

        if (locals.rank == 0) state.mut().clanWeightedTotal = state.get().clanWeightedTotal - WOLFPACK_RANK_MULTIPLIER_0;
        if (locals.rank == 1) state.mut().clanWeightedTotal = state.get().clanWeightedTotal - WOLFPACK_RANK_MULTIPLIER_1;
        if (locals.rank == 2) state.mut().clanWeightedTotal = state.get().clanWeightedTotal - WOLFPACK_RANK_MULTIPLIER_2;
        if (locals.rank == 3) state.mut().clanWeightedTotal = state.get().clanWeightedTotal - WOLFPACK_RANK_MULTIPLIER_3;
        if (locals.rank == 4) state.mut().clanWeightedTotal = state.get().clanWeightedTotal - WOLFPACK_RANK_MULTIPLIER_4;

        state.mut().clanRanks.removeByKey(input.memberAddress);
        state.mut().clanMemberCount = state.get().clanMemberCount - 1;

        output.returnCode = WOLFPACK_OK;
    }

    PUBLIC_PROCEDURE_WITH_LOCALS(SetClanRank)
    {
        if (qpi.invocator() != state.get().adminAddress)
        {
            output.returnCode = WOLFPACK_ERROR_ACCESS_DENIED;
            return;
        }
        if (!state.get().clanRanks.get(input.memberAddress, locals.oldRank))
        {
            output.returnCode = WOLFPACK_ERROR_NOT_CLAN_MEMBER;
            return;
        }
        if (input.rank > WOLFPACK_MAX_RANK)
        {
            output.returnCode = WOLFPACK_ERROR_INVALID_RANK;
            return;
        }

        if (locals.oldRank == 0) state.mut().clanWeightedTotal = state.get().clanWeightedTotal - WOLFPACK_RANK_MULTIPLIER_0;
        if (locals.oldRank == 1) state.mut().clanWeightedTotal = state.get().clanWeightedTotal - WOLFPACK_RANK_MULTIPLIER_1;
        if (locals.oldRank == 2) state.mut().clanWeightedTotal = state.get().clanWeightedTotal - WOLFPACK_RANK_MULTIPLIER_2;
        if (locals.oldRank == 3) state.mut().clanWeightedTotal = state.get().clanWeightedTotal - WOLFPACK_RANK_MULTIPLIER_3;
        if (locals.oldRank == 4) state.mut().clanWeightedTotal = state.get().clanWeightedTotal - WOLFPACK_RANK_MULTIPLIER_4;

        state.mut().clanRanks.replace(input.memberAddress, input.rank);

        if (input.rank == 0) state.mut().clanWeightedTotal = state.get().clanWeightedTotal + WOLFPACK_RANK_MULTIPLIER_0;
        if (input.rank == 1) state.mut().clanWeightedTotal = state.get().clanWeightedTotal + WOLFPACK_RANK_MULTIPLIER_1;
        if (input.rank == 2) state.mut().clanWeightedTotal = state.get().clanWeightedTotal + WOLFPACK_RANK_MULTIPLIER_2;
        if (input.rank == 3) state.mut().clanWeightedTotal = state.get().clanWeightedTotal + WOLFPACK_RANK_MULTIPLIER_3;
        if (input.rank == 4) state.mut().clanWeightedTotal = state.get().clanWeightedTotal + WOLFPACK_RANK_MULTIPLIER_4;

        output.returnCode = WOLFPACK_OK;
    }

    PUBLIC_PROCEDURE(SetAdmin)
    {
        if (qpi.invocator() != state.get().adminAddress)
        {
            output.returnCode = WOLFPACK_ERROR_ACCESS_DENIED;
            return;
        }
        state.mut().adminAddress = input.newAdmin;
        output.returnCode = WOLFPACK_OK;
    }

    PUBLIC_PROCEDURE(SetExcludeAddress)
    {
        if (qpi.invocator() != state.get().adminAddress)
        {
            output.returnCode = WOLFPACK_ERROR_ACCESS_DENIED;
            return;
        }
        if (input.slot == 1)
        {
            state.mut().excludeAddress1 = input.address;
        }
        else if (input.slot == 2)
        {
            state.mut().excludeAddress2 = input.address;
        }
        else
        {
            output.returnCode = WOLFPACK_ERROR_INVALID_RANK;
            return;
        }
        output.returnCode = WOLFPACK_OK;
    }

    // ======================== REGISTRATION ========================

    REGISTER_USER_FUNCTIONS_AND_PROCEDURES()
    {
        REGISTER_USER_FUNCTION(GetStatus, 1);
        REGISTER_USER_FUNCTION(GetHolderInfo, 2);
        REGISTER_USER_FUNCTION(GetClanMemberInfo, 3);
        REGISTER_USER_FUNCTION(GetShareholderInfo, 4);

        REGISTER_USER_PROCEDURE(DepositRevenue, 1);
        REGISTER_USER_PROCEDURE(AddClanMember, 2);
        REGISTER_USER_PROCEDURE(RemoveClanMember, 3);
        REGISTER_USER_PROCEDURE(SetClanRank, 4);
        REGISTER_USER_PROCEDURE(SetAdmin, 5);
        REGISTER_USER_PROCEDURE(SetExcludeAddress, 6);
    }

    // ======================== SYSTEM PROCEDURES ========================

    INITIALIZE()
    {
        state.mut().adminAddress = qpi.originator();

        // WP token (external, issued on QX by MLMWPS...)
        state.mut().wpToken.issuer = ID(
            _M, _L, _M, _W, _P, _S, _Q, _N, _V, _A, _I, _B, _R, _F, _D, _H,
            _W, _C, _K, _S, _F, _O, _V, _U, _A, _Z, _D, _D, _W, _K, _J, _G,
            _C, _L, _R, _S, _Y, _Z, _I, _U, _E, _F, _D, _U, _R, _P, _W, _I,
            _P, _Q, _X, _A, _C, _Y, _O, _E
        );
        state.mut().wpToken.assetName = WOLFPACK_SC_ASSET_NAME; // "WP"

        state.mut().totalTokensSnapshot = 0;
        state.mut().holderCount = 0;
        state.mut().totalSharesSnapshot = 0;
        state.mut().shareholderCount = 0;
        state.mut().clanMemberCount = 0;
        state.mut().clanWeightedTotal = 0;
        state.mut().pendingRevenue = 0;
        state.mut().reinvestmentFund = 0;
        state.mut().totalDistributed = 0;
        state.mut().totalDeposited = 0;
        state.mut().lastDistributionEpoch = 0;
        state.mut().lastPayoutTick = 0;
        state.mut().excludeAddress1 = NULL_ID;
        state.mut().excludeAddress2 = NULL_ID;
    }

    // Snapshot WP token holders AND SC shareholders at the start of each epoch
    struct BEGIN_EPOCH_locals
    {
        AssetPossessionIterator tokenIter;
        AssetPossessionIterator scIter;
        Asset scAsset;
        uint64 balance;
        id holder;
        uint64 existingBalance;
    };
    BEGIN_EPOCH_WITH_LOCALS()
    {
        // ---- Pass 1: WP Token holders (external token, issuer=MLMWPS...) ----
        state.mut().holderBalances.reset();
        state.mut().totalTokensSnapshot = 0;
        state.mut().holderCount = 0;

        if (state.get().wpToken.issuer != NULL_ID)
        {
            for (locals.tokenIter.begin(state.get().wpToken); !locals.tokenIter.reachedEnd(); locals.tokenIter.next())
            {
                if (locals.tokenIter.possessor() == SELF) continue;
                if (state.get().excludeAddress1 != NULL_ID && locals.tokenIter.possessor() == state.get().excludeAddress1) continue;
                if (state.get().excludeAddress2 != NULL_ID && locals.tokenIter.possessor() == state.get().excludeAddress2) continue;

                locals.balance = locals.tokenIter.numberOfPossessedShares();
                locals.holder = locals.tokenIter.possessor();

                if (locals.balance > 0)
                {
                    locals.existingBalance = 0;
                    state.get().holderBalances.get(locals.holder, locals.existingBalance);
                    locals.balance = sadd(locals.existingBalance, locals.balance);

                    if (state.mut().holderBalances.set(locals.holder, locals.balance) != NULL_INDEX)
                    {
                        state.mut().totalTokensSnapshot = sadd(state.get().totalTokensSnapshot, (uint64)locals.tokenIter.numberOfPossessedShares());
                        if (locals.existingBalance == 0)
                        {
                            state.mut().holderCount = state.get().holderCount + 1;
                        }
                    }
                }
            }
        }

        // ---- Pass 2: SC shareholders (IPO shares, issuer=NULL_ID, name="WP") ----
        state.mut().shareholderBalances.reset();
        state.mut().totalSharesSnapshot = 0;
        state.mut().shareholderCount = 0;

        locals.scAsset.issuer = NULL_ID;
        locals.scAsset.assetName = WOLFPACK_SC_ASSET_NAME;

        for (locals.scIter.begin(locals.scAsset); !locals.scIter.reachedEnd(); locals.scIter.next())
        {
            if (locals.scIter.possessor() == SELF) continue;
            if (state.get().excludeAddress1 != NULL_ID && locals.scIter.possessor() == state.get().excludeAddress1) continue;
            if (state.get().excludeAddress2 != NULL_ID && locals.scIter.possessor() == state.get().excludeAddress2) continue;

            locals.balance = locals.scIter.numberOfPossessedShares();
            locals.holder = locals.scIter.possessor();

            if (locals.balance > 0)
            {
                locals.existingBalance = 0;
                state.get().shareholderBalances.get(locals.holder, locals.existingBalance);
                locals.balance = sadd(locals.existingBalance, locals.balance);

                if (state.mut().shareholderBalances.set(locals.holder, locals.balance) != NULL_INDEX)
                {
                    state.mut().totalSharesSnapshot = sadd(state.get().totalSharesSnapshot, (uint64)locals.scIter.numberOfPossessedShares());
                    if (locals.existingBalance == 0)
                    {
                        state.mut().shareholderCount = state.get().shareholderCount + 1;
                    }
                }
            }
        }
    }

    END_EPOCH()
    {
        state.mut().holderBalances.cleanupIfNeeded();
        state.mut().shareholderBalances.cleanupIfNeeded();
        state.mut().clanRanks.cleanupIfNeeded();
    }

    BEGIN_TICK()
    {
    }

    // Auto-payout at 11:00 UTC daily
    struct END_TICK_locals
    {
        uint64 amount;
        uint64 holderShare;
        uint64 shareholderShare;
        uint64 clanShare;
        uint64 reinvestShare;
        sint64 idx;
        id holder;
        uint64 tokens;
        uint64 reward;
        uint64 rank;
        uint64 multiplier;
        Entity entity;
        uint64 contractBalance;
    };
    END_TICK_WITH_LOCALS()
    {
        // Gate: only at hour 11 and enough ticks since last payout
        if (qpi.hour() != WOLFPACK_PAYOUT_HOUR)
        {
            return;
        }
        if (state.get().lastPayoutTick != 0 &&
            qpi.tick() < state.get().lastPayoutTick + WOLFPACK_MIN_PAYOUT_INTERVAL_TICKS)
        {
            return;
        }
        if (state.get().pendingRevenue == 0)
        {
            return;
        }

        // --- Step 1: Split revenue ---
        locals.amount = state.get().pendingRevenue;
        locals.holderShare = div(locals.amount * WOLFPACK_DISTRIBUTION_PERMILLE_HOLDERS, 1000ULL);
        locals.shareholderShare = div(locals.amount * WOLFPACK_DISTRIBUTION_PERMILLE_SHAREHOLDERS, 1000ULL);
        locals.clanShare = div(locals.amount * WOLFPACK_DISTRIBUTION_PERMILLE_CLAN, 1000ULL);
        locals.reinvestShare = locals.amount - locals.holderShare - locals.shareholderShare - locals.clanShare;

        state.mut().pendingRevenue = 0;
        state.mut().totalDistributed = state.get().totalDistributed + locals.amount;
        state.mut().lastDistributionEpoch = qpi.epoch();
        state.mut().lastPayoutTick = qpi.tick();
        state.mut().reinvestmentFund = state.get().reinvestmentFund + locals.reinvestShare;

        qpi.getEntity(SELF, locals.entity);
        locals.contractBalance = locals.entity.incomingAmount - locals.entity.outgoingAmount;

        // --- Step 2: Push 70% to WP token holders ---
        if (locals.holderShare > 0 && state.get().totalTokensSnapshot > 0)
        {
            locals.idx = NULL_INDEX;
            while (true)
            {
                locals.idx = state.get().holderBalances.nextElementIndex(locals.idx);
                if (locals.idx == NULL_INDEX) break;
                locals.holder = state.get().holderBalances.key(locals.idx);
                locals.tokens = state.get().holderBalances.value(locals.idx);
                if (locals.tokens == 0) continue;

                locals.reward = div(locals.holderShare * locals.tokens, state.get().totalTokensSnapshot);
                if (locals.reward == 0) continue;
                if (locals.reward > locals.contractBalance) locals.reward = locals.contractBalance;

                qpi.transfer(locals.holder, locals.reward);
                locals.contractBalance = locals.contractBalance - locals.reward;
                if (locals.contractBalance == 0) break;
            }
        }

        // --- Step 3: Push 10% to SC shareholders ---
        if (locals.shareholderShare > 0 && state.get().totalSharesSnapshot > 0)
        {
            if (locals.contractBalance == 0)
            {
                qpi.getEntity(SELF, locals.entity);
                locals.contractBalance = locals.entity.incomingAmount - locals.entity.outgoingAmount;
            }

            locals.idx = NULL_INDEX;
            while (true)
            {
                locals.idx = state.get().shareholderBalances.nextElementIndex(locals.idx);
                if (locals.idx == NULL_INDEX) break;
                locals.holder = state.get().shareholderBalances.key(locals.idx);
                locals.tokens = state.get().shareholderBalances.value(locals.idx);
                if (locals.tokens == 0) continue;

                locals.reward = div(locals.shareholderShare * locals.tokens, state.get().totalSharesSnapshot);
                if (locals.reward == 0) continue;
                if (locals.reward > locals.contractBalance) locals.reward = locals.contractBalance;

                qpi.transfer(locals.holder, locals.reward);
                locals.contractBalance = locals.contractBalance - locals.reward;
                if (locals.contractBalance == 0) break;
            }
        }

        // --- Step 4: Push 10% to clan members ---
        if (locals.clanShare > 0 && state.get().clanWeightedTotal > 0)
        {
            if (locals.contractBalance == 0)
            {
                qpi.getEntity(SELF, locals.entity);
                locals.contractBalance = locals.entity.incomingAmount - locals.entity.outgoingAmount;
            }

            locals.idx = NULL_INDEX;
            while (true)
            {
                locals.idx = state.get().clanRanks.nextElementIndex(locals.idx);
                if (locals.idx == NULL_INDEX) break;
                locals.holder = state.get().clanRanks.key(locals.idx);
                locals.rank = state.get().clanRanks.value(locals.idx);

                locals.multiplier = WOLFPACK_RANK_MULTIPLIER_0;
                if (locals.rank == 1) locals.multiplier = WOLFPACK_RANK_MULTIPLIER_1;
                if (locals.rank == 2) locals.multiplier = WOLFPACK_RANK_MULTIPLIER_2;
                if (locals.rank == 3) locals.multiplier = WOLFPACK_RANK_MULTIPLIER_3;
                if (locals.rank == 4) locals.multiplier = WOLFPACK_RANK_MULTIPLIER_4;

                locals.reward = div(locals.clanShare * locals.multiplier, state.get().clanWeightedTotal);
                if (locals.reward == 0) continue;
                if (locals.reward > locals.contractBalance) locals.reward = locals.contractBalance;

                qpi.transfer(locals.holder, locals.reward);
                locals.contractBalance = locals.contractBalance - locals.reward;
                if (locals.contractBalance == 0) break;
            }
        }
    }
};
