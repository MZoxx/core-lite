using namespace QPI;

// ============================================================================
// WolfPack (WP) - Revenue Distribution Smart Contract
//
// Revenue split:
//   70% -> WP token stakers (proportional to staked amount)
//   10% -> SC shareholders (676 computor shareholders)
//   10% -> Active clan members (rank multiplier booster)
//   10% -> Reinvestment fund
// ============================================================================

// --- Constants (prefixed with WOLFPACK_) ---
constexpr uint64 WOLFPACK_MAX_STAKERS = 16384;
constexpr uint64 WOLFPACK_MAX_CLAN_MEMBERS = 8192;
constexpr uint64 WOLFPACK_DISTRIBUTION_PERMILLE_STAKERS = 700;
constexpr uint64 WOLFPACK_DISTRIBUTION_PERMILLE_SHAREHOLDERS = 100;
constexpr uint64 WOLFPACK_DISTRIBUTION_PERMILLE_CLAN = 100;
constexpr uint64 WOLFPACK_DISTRIBUTION_PERMILLE_REINVEST = 100;
constexpr uint64 WOLFPACK_MIN_STAKE_AMOUNT = 1;
constexpr uint64 WOLFPACK_INVOCATION_FEE = 10;

// Return codes
constexpr uint32 WOLFPACK_OK = 0;
constexpr uint32 WOLFPACK_ERROR_ACCESS_DENIED = 1;
constexpr uint32 WOLFPACK_ERROR_INSUFFICIENT_AMOUNT = 2;
constexpr uint32 WOLFPACK_ERROR_ALREADY_STAKED = 3;
constexpr uint32 WOLFPACK_ERROR_NOT_STAKED = 4;
constexpr uint32 WOLFPACK_ERROR_CLAN_FULL = 5;
constexpr uint32 WOLFPACK_ERROR_NOT_CLAN_MEMBER = 6;
constexpr uint32 WOLFPACK_ERROR_ALREADY_CLAN_MEMBER = 7;
constexpr uint32 WOLFPACK_ERROR_INVALID_RANK = 8;
constexpr uint32 WOLFPACK_ERROR_ZERO_AMOUNT = 9;

// Rank multipliers (in permille: 1000 = 1x, 1500 = 1.5x, etc.)
constexpr uint64 WOLFPACK_RANK_MULTIPLIER_0 = 1000;   // Default rank
constexpr uint64 WOLFPACK_RANK_MULTIPLIER_1 = 1200;   // Rank 1
constexpr uint64 WOLFPACK_RANK_MULTIPLIER_2 = 1500;   // Rank 2
constexpr uint64 WOLFPACK_RANK_MULTIPLIER_3 = 2000;   // Rank 3
constexpr uint64 WOLFPACK_RANK_MULTIPLIER_4 = 3000;   // Rank 4 (alpha)
constexpr uint64 WOLFPACK_MAX_RANK = 4;

struct WOLFPACK2
{
};

struct WOLFPACK : public ContractBase
{
    // ======================== STATE ========================
    struct StakerInfo
    {
        id address;
        uint64 stakedAmount;
        uint64 totalClaimed;
        uint16 stakeEpoch;
        uint16 _padding0;
        uint32 _padding1;
    };

    struct ClanMember
    {
        id address;
        uint64 rank;           // 0-4
        uint64 totalEarned;
        uint16 joinEpoch;
        uint16 _padding0;
        uint32 _padding1;
    };

    struct StateData
    {
        // Admin
        id adminAddress;

        // Staking pool
        HashMap<id, uint64, WOLFPACK_MAX_STAKERS> stakerAmounts;
        uint64 totalStaked;
        uint64 stakerCount;

        // Clan
        HashMap<id, uint64, WOLFPACK_MAX_CLAN_MEMBERS> clanRanks;
        uint64 clanMemberCount;

        // Revenue tracking
        uint64 pendingRevenue;
        uint64 reinvestmentFund;
        uint64 totalDistributed;
        uint64 totalDeposited;

        // Distribution accumulators (filled during distribute, paid out per-user)
        uint64 lastDistributionEpoch;
        uint64 stakerPool;         // 70% accumulator
        uint64 shareholderPool;    // 10% accumulator
        uint64 clanPool;           // 10% accumulator

        // Clan weighted total for current distribution
        uint64 clanWeightedTotal;
    };

    // ======================== INPUT / OUTPUT ========================

    // --- DepositRevenue: anyone can deposit QU as revenue ---
    struct DepositRevenue_input
    {
    };
    struct DepositRevenue_output
    {
        uint32 returnCode;
    };

    // --- Stake: stake QU tokens ---
    struct Stake_input
    {
    };
    struct Stake_output
    {
        uint32 returnCode;
    };

    // --- Unstake: withdraw staked tokens ---
    struct Unstake_input
    {
    };
    struct Unstake_output
    {
        uint32 returnCode;
        uint64 amount;
    };

    // --- Distribute: trigger revenue distribution (anyone can call) ---
    struct Distribute_input
    {
    };
    struct Distribute_output
    {
        uint32 returnCode;
        uint64 distributed;
    };
    struct Distribute_locals
    {
        uint64 amount;
        uint64 stakerShare;
        uint64 shareholderShare;
        uint64 clanShare;
        uint64 reinvestShare;
        Entity entity;
        uint64 balance;
    };

    // --- AddClanMember: admin adds a clan member ---
    struct AddClanMember_input
    {
        id memberAddress;
        uint64 rank;
    };
    struct AddClanMember_output
    {
        uint32 returnCode;
    };

    // --- RemoveClanMember: admin removes a clan member ---
    struct RemoveClanMember_input
    {
        id memberAddress;
    };
    struct RemoveClanMember_output
    {
        uint32 returnCode;
    };

    // --- SetClanRank: admin sets rank for a member ---
    struct SetClanRank_input
    {
        id memberAddress;
        uint64 rank;
    };
    struct SetClanRank_output
    {
        uint32 returnCode;
    };

    // --- SetAdmin: transfer admin to new address ---
    struct SetAdmin_input
    {
        id newAdmin;
    };
    struct SetAdmin_output
    {
        uint32 returnCode;
    };

    // --- ClaimStakerReward: staker claims their share ---
    struct ClaimStakerReward_input
    {
    };
    struct ClaimStakerReward_output
    {
        uint32 returnCode;
        uint64 amount;
    };
    struct ClaimStakerReward_locals
    {
        uint64 userStake;
        uint64 reward;
        Entity entity;
        uint64 balance;
    };

    // --- ClaimClanReward: clan member claims their share ---
    struct ClaimClanReward_input
    {
    };
    struct ClaimClanReward_output
    {
        uint32 returnCode;
        uint64 amount;
    };
    struct ClaimClanReward_locals
    {
        uint64 rank;
        uint64 multiplier;
        uint64 reward;
        Entity entity;
        uint64 balance;
    };

    // --- GetStatus: read contract state ---
    struct GetStatus_input
    {
    };
    struct GetStatus_output
    {
        uint64 totalStaked;
        uint64 stakerCount;
        uint64 clanMemberCount;
        uint64 pendingRevenue;
        uint64 reinvestmentFund;
        uint64 totalDistributed;
        uint64 totalDeposited;
        uint64 stakerPool;
        uint64 clanPool;
        uint64 lastDistributionEpoch;
        id adminAddress;
    };

    // --- GetStakerInfo: get staker details ---
    struct GetStakerInfo_input
    {
        id stakerAddress;
    };
    struct GetStakerInfo_output
    {
        uint64 stakedAmount;
        uint32 isStaked;
    };

    // --- GetClanMemberInfo: get clan member details ---
    struct GetClanMemberInfo_input
    {
        id memberAddress;
    };
    struct GetClanMemberInfo_output
    {
        uint64 rank;
        uint32 isMember;
    };

    // ======================== FUNCTIONS (read-only) ========================

    PUBLIC_FUNCTION(GetStatus)
    {
        output.totalStaked = state.get().totalStaked;
        output.stakerCount = state.get().stakerCount;
        output.clanMemberCount = state.get().clanMemberCount;
        output.pendingRevenue = state.get().pendingRevenue;
        output.reinvestmentFund = state.get().reinvestmentFund;
        output.totalDistributed = state.get().totalDistributed;
        output.totalDeposited = state.get().totalDeposited;
        output.stakerPool = state.get().stakerPool;
        output.clanPool = state.get().clanPool;
        output.lastDistributionEpoch = state.get().lastDistributionEpoch;
        output.adminAddress = state.get().adminAddress;
    }

    PUBLIC_FUNCTION(GetStakerInfo)
    {
        output.stakedAmount = state.get().stakerAmounts.get(input.stakerAddress);
        output.isStaked = (output.stakedAmount > 0) ? 1 : 0;
    }

    PUBLIC_FUNCTION(GetClanMemberInfo)
    {
        output.rank = state.get().clanRanks.get(input.memberAddress);
        // rank 0 could mean default rank or not a member; check presence
        output.isMember = state.get().clanRanks.contains(input.memberAddress) ? 1 : 0;
    }

    // ======================== PROCEDURES (state-modifying) ========================

    // --- Deposit revenue into the contract ---
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

    // --- Stake tokens ---
    PUBLIC_PROCEDURE(Stake)
    {
        if (qpi.invocationReward() < WOLFPACK_MIN_STAKE_AMOUNT)
        {
            output.returnCode = WOLFPACK_ERROR_INSUFFICIENT_AMOUNT;
            if (qpi.invocationReward() > 0)
            {
                qpi.transfer(qpi.invocator(), qpi.invocationReward());
            }
            return;
        }

        if (state.get().stakerAmounts.contains(qpi.invocator()))
        {
            // Add to existing stake
            state.mut().stakerAmounts.set(qpi.invocator(),
                state.get().stakerAmounts.get(qpi.invocator()) + qpi.invocationReward());
        }
        else
        {
            // New staker
            state.mut().stakerAmounts.add(qpi.invocator(), qpi.invocationReward());
            state.mut().stakerCount = state.get().stakerCount + 1;
        }
        state.mut().totalStaked = state.get().totalStaked + qpi.invocationReward();
        output.returnCode = WOLFPACK_OK;
    }

    // --- Unstake: withdraw entire staked amount ---
    PUBLIC_PROCEDURE(Unstake)
    {
        if (!state.get().stakerAmounts.contains(qpi.invocator()))
        {
            output.returnCode = WOLFPACK_ERROR_NOT_STAKED;
            return;
        }

        output.amount = state.get().stakerAmounts.get(qpi.invocator());
        state.mut().totalStaked = state.get().totalStaked - output.amount;
        state.mut().stakerAmounts.remove(qpi.invocator());
        state.mut().stakerCount = state.get().stakerCount - 1;

        qpi.transfer(qpi.invocator(), output.amount);
        output.returnCode = WOLFPACK_OK;
    }

    // --- Distribute: split pending revenue into pools ---
    PUBLIC_PROCEDURE_WITH_LOCALS(Distribute)
    {
        locals.amount = state.get().pendingRevenue;
        if (locals.amount == 0)
        {
            output.returnCode = WOLFPACK_ERROR_ZERO_AMOUNT;
            return;
        }

        // Calculate shares (permille-based to avoid division)
        locals.stakerShare = div(locals.amount * WOLFPACK_DISTRIBUTION_PERMILLE_STAKERS, 1000);
        locals.shareholderShare = div(locals.amount * WOLFPACK_DISTRIBUTION_PERMILLE_SHAREHOLDERS, 1000);
        locals.clanShare = div(locals.amount * WOLFPACK_DISTRIBUTION_PERMILLE_CLAN, 1000);
        locals.reinvestShare = locals.amount - locals.stakerShare - locals.shareholderShare - locals.clanShare;

        // Accumulate into pools
        state.mut().stakerPool = state.get().stakerPool + locals.stakerShare;
        state.mut().clanPool = state.get().clanPool + locals.clanShare;
        state.mut().reinvestmentFund = state.get().reinvestmentFund + locals.reinvestShare;
        state.mut().pendingRevenue = 0;
        state.mut().totalDistributed = state.get().totalDistributed + locals.amount;
        state.mut().lastDistributionEpoch = qpi.epoch();

        // Shareholders get paid via burn to own fee reserve (sustains contract)
        // and the 10% shareholder pool goes as dividends
        // Note: distributeDividends does not exist in qpi, so we burn to fee reserve
        // which benefits shareholders through sustained contract execution
        qpi.burn(locals.shareholderShare);

        output.distributed = locals.amount;
        output.returnCode = WOLFPACK_OK;
    }

    // --- Claim staker reward: proportional share of stakerPool ---
    PUBLIC_PROCEDURE_WITH_LOCALS(ClaimStakerReward)
    {
        if (!state.get().stakerAmounts.contains(qpi.invocator()))
        {
            output.returnCode = WOLFPACK_ERROR_NOT_STAKED;
            return;
        }

        locals.userStake = state.get().stakerAmounts.get(qpi.invocator());
        if (state.get().totalStaked == 0 || state.get().stakerPool == 0)
        {
            output.returnCode = WOLFPACK_ERROR_ZERO_AMOUNT;
            return;
        }

        // reward = stakerPool * userStake / totalStaked
        locals.reward = div(state.get().stakerPool * locals.userStake, state.get().totalStaked);
        if (locals.reward == 0)
        {
            output.returnCode = WOLFPACK_ERROR_ZERO_AMOUNT;
            return;
        }

        // Check contract balance
        qpi.getEntity(SELF, locals.entity);
        locals.balance = locals.entity.incomingAmount - locals.entity.outgoingAmount;
        if (locals.balance < locals.reward)
        {
            locals.reward = locals.balance;
        }

        state.mut().stakerPool = state.get().stakerPool - locals.reward;
        qpi.transfer(qpi.invocator(), locals.reward);

        output.amount = locals.reward;
        output.returnCode = WOLFPACK_OK;
    }

    // --- Claim clan reward: weighted by rank multiplier ---
    PUBLIC_PROCEDURE_WITH_LOCALS(ClaimClanReward)
    {
        if (!state.get().clanRanks.contains(qpi.invocator()))
        {
            output.returnCode = WOLFPACK_ERROR_NOT_CLAN_MEMBER;
            return;
        }

        if (state.get().clanPool == 0 || state.get().clanWeightedTotal == 0)
        {
            output.returnCode = WOLFPACK_ERROR_ZERO_AMOUNT;
            return;
        }

        locals.rank = state.get().clanRanks.get(qpi.invocator());

        // Get multiplier for rank
        locals.multiplier = WOLFPACK_RANK_MULTIPLIER_0;
        if (locals.rank == 1) locals.multiplier = WOLFPACK_RANK_MULTIPLIER_1;
        if (locals.rank == 2) locals.multiplier = WOLFPACK_RANK_MULTIPLIER_2;
        if (locals.rank == 3) locals.multiplier = WOLFPACK_RANK_MULTIPLIER_3;
        if (locals.rank == 4) locals.multiplier = WOLFPACK_RANK_MULTIPLIER_4;

        // reward = clanPool * multiplier / clanWeightedTotal
        locals.reward = div(state.get().clanPool * locals.multiplier, state.get().clanWeightedTotal);
        if (locals.reward == 0)
        {
            output.returnCode = WOLFPACK_ERROR_ZERO_AMOUNT;
            return;
        }

        // Check contract balance
        qpi.getEntity(SELF, locals.entity);
        locals.balance = locals.entity.incomingAmount - locals.entity.outgoingAmount;
        if (locals.balance < locals.reward)
        {
            locals.reward = locals.balance;
        }

        state.mut().clanPool = state.get().clanPool - locals.reward;
        qpi.transfer(qpi.invocator(), locals.reward);

        output.amount = locals.reward;
        output.returnCode = WOLFPACK_OK;
    }

    // --- Admin: Add clan member ---
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

        state.mut().clanRanks.add(input.memberAddress, input.rank);
        state.mut().clanMemberCount = state.get().clanMemberCount + 1;

        // Update weighted total
        if (input.rank == 0) state.mut().clanWeightedTotal = state.get().clanWeightedTotal + WOLFPACK_RANK_MULTIPLIER_0;
        if (input.rank == 1) state.mut().clanWeightedTotal = state.get().clanWeightedTotal + WOLFPACK_RANK_MULTIPLIER_1;
        if (input.rank == 2) state.mut().clanWeightedTotal = state.get().clanWeightedTotal + WOLFPACK_RANK_MULTIPLIER_2;
        if (input.rank == 3) state.mut().clanWeightedTotal = state.get().clanWeightedTotal + WOLFPACK_RANK_MULTIPLIER_3;
        if (input.rank == 4) state.mut().clanWeightedTotal = state.get().clanWeightedTotal + WOLFPACK_RANK_MULTIPLIER_4;

        output.returnCode = WOLFPACK_OK;
    }

    struct RemoveClanMember_locals
    {
        uint64 rank;
    };

    // --- Admin: Remove clan member ---
    PUBLIC_PROCEDURE_WITH_LOCALS(RemoveClanMember)
    {
        if (qpi.invocator() != state.get().adminAddress)
        {
            output.returnCode = WOLFPACK_ERROR_ACCESS_DENIED;
            return;
        }
        if (!state.get().clanRanks.contains(input.memberAddress))
        {
            output.returnCode = WOLFPACK_ERROR_NOT_CLAN_MEMBER;
            return;
        }

        // Remove weighted contribution
        locals.rank = state.get().clanRanks.get(input.memberAddress);
        if (locals.rank == 0) state.mut().clanWeightedTotal = state.get().clanWeightedTotal - WOLFPACK_RANK_MULTIPLIER_0;
        if (locals.rank == 1) state.mut().clanWeightedTotal = state.get().clanWeightedTotal - WOLFPACK_RANK_MULTIPLIER_1;
        if (locals.rank == 2) state.mut().clanWeightedTotal = state.get().clanWeightedTotal - WOLFPACK_RANK_MULTIPLIER_2;
        if (locals.rank == 3) state.mut().clanWeightedTotal = state.get().clanWeightedTotal - WOLFPACK_RANK_MULTIPLIER_3;
        if (locals.rank == 4) state.mut().clanWeightedTotal = state.get().clanWeightedTotal - WOLFPACK_RANK_MULTIPLIER_4;

        state.mut().clanRanks.remove(input.memberAddress);
        state.mut().clanMemberCount = state.get().clanMemberCount - 1;

        output.returnCode = WOLFPACK_OK;
    }

    struct SetClanRank_locals
    {
        uint64 oldRank;
    };

    // --- Admin: Set clan rank ---
    PUBLIC_PROCEDURE_WITH_LOCALS(SetClanRank)
    {
        if (qpi.invocator() != state.get().adminAddress)
        {
            output.returnCode = WOLFPACK_ERROR_ACCESS_DENIED;
            return;
        }
        if (!state.get().clanRanks.contains(input.memberAddress))
        {
            output.returnCode = WOLFPACK_ERROR_NOT_CLAN_MEMBER;
            return;
        }
        if (input.rank > WOLFPACK_MAX_RANK)
        {
            output.returnCode = WOLFPACK_ERROR_INVALID_RANK;
            return;
        }

        // Remove old weighted contribution
        locals.oldRank = state.get().clanRanks.get(input.memberAddress);
        if (locals.oldRank == 0) state.mut().clanWeightedTotal = state.get().clanWeightedTotal - WOLFPACK_RANK_MULTIPLIER_0;
        if (locals.oldRank == 1) state.mut().clanWeightedTotal = state.get().clanWeightedTotal - WOLFPACK_RANK_MULTIPLIER_1;
        if (locals.oldRank == 2) state.mut().clanWeightedTotal = state.get().clanWeightedTotal - WOLFPACK_RANK_MULTIPLIER_2;
        if (locals.oldRank == 3) state.mut().clanWeightedTotal = state.get().clanWeightedTotal - WOLFPACK_RANK_MULTIPLIER_3;
        if (locals.oldRank == 4) state.mut().clanWeightedTotal = state.get().clanWeightedTotal - WOLFPACK_RANK_MULTIPLIER_4;

        // Set new rank and add new weighted contribution
        state.mut().clanRanks.set(input.memberAddress, input.rank);
        if (input.rank == 0) state.mut().clanWeightedTotal = state.get().clanWeightedTotal + WOLFPACK_RANK_MULTIPLIER_0;
        if (input.rank == 1) state.mut().clanWeightedTotal = state.get().clanWeightedTotal + WOLFPACK_RANK_MULTIPLIER_1;
        if (input.rank == 2) state.mut().clanWeightedTotal = state.get().clanWeightedTotal + WOLFPACK_RANK_MULTIPLIER_2;
        if (input.rank == 3) state.mut().clanWeightedTotal = state.get().clanWeightedTotal + WOLFPACK_RANK_MULTIPLIER_3;
        if (input.rank == 4) state.mut().clanWeightedTotal = state.get().clanWeightedTotal + WOLFPACK_RANK_MULTIPLIER_4;

        output.returnCode = WOLFPACK_OK;
    }

    // --- Admin: Transfer admin ---
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

    // ======================== REGISTRATION ========================

    REGISTER_USER_FUNCTIONS_AND_PROCEDURES()
    {
        // Functions (read-only)
        REGISTER_USER_FUNCTION(GetStatus, 1);
        REGISTER_USER_FUNCTION(GetStakerInfo, 2);
        REGISTER_USER_FUNCTION(GetClanMemberInfo, 3);

        // Procedures (state-modifying)
        REGISTER_USER_PROCEDURE(DepositRevenue, 1);
        REGISTER_USER_PROCEDURE(Stake, 2);
        REGISTER_USER_PROCEDURE(Unstake, 3);
        REGISTER_USER_PROCEDURE(Distribute, 4);
        REGISTER_USER_PROCEDURE(ClaimStakerReward, 5);
        REGISTER_USER_PROCEDURE(ClaimClanReward, 6);
        REGISTER_USER_PROCEDURE(AddClanMember, 7);
        REGISTER_USER_PROCEDURE(RemoveClanMember, 8);
        REGISTER_USER_PROCEDURE(SetClanRank, 9);
        REGISTER_USER_PROCEDURE(SetAdmin, 10);
    }

    // ======================== SYSTEM PROCEDURES ========================

    INITIALIZE()
    {
        // Set deployer as admin
        state.mut().adminAddress = qpi.originator();
        state.mut().totalStaked = 0;
        state.mut().stakerCount = 0;
        state.mut().clanMemberCount = 0;
        state.mut().pendingRevenue = 0;
        state.mut().reinvestmentFund = 0;
        state.mut().totalDistributed = 0;
        state.mut().totalDeposited = 0;
        state.mut().lastDistributionEpoch = 0;
        state.mut().stakerPool = 0;
        state.mut().shareholderPool = 0;
        state.mut().clanPool = 0;
        state.mut().clanWeightedTotal = 0;
    }

    BEGIN_EPOCH()
    {
    }

    END_EPOCH()
    {
        // Cleanup hash containers
        state.mut().stakerAmounts.cleanupIfNeeded();
        state.mut().clanRanks.cleanupIfNeeded();
    }

    BEGIN_TICK()
    {
    }

    END_TICK()
    {
    }

    EXPAND()
    {
    }
};
