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

// Return codes
constexpr uint32 WOLFPACK_OK = 0;
constexpr uint32 WOLFPACK_ERROR_ACCESS_DENIED = 1;
constexpr uint32 WOLFPACK_ERROR_INSUFFICIENT_AMOUNT = 2;
constexpr uint32 WOLFPACK_ERROR_NOT_STAKED = 4;
constexpr uint32 WOLFPACK_ERROR_NOT_CLAN_MEMBER = 6;
constexpr uint32 WOLFPACK_ERROR_ALREADY_CLAN_MEMBER = 7;
constexpr uint32 WOLFPACK_ERROR_INVALID_RANK = 8;
constexpr uint32 WOLFPACK_ERROR_ZERO_AMOUNT = 9;

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

        HashMap<id, uint64, WOLFPACK_MAX_STAKERS> stakerAmounts;
        uint64 totalStaked;
        uint64 stakerCount;

        HashMap<id, uint64, WOLFPACK_MAX_CLAN_MEMBERS> clanRanks;
        uint64 clanMemberCount;

        uint64 pendingRevenue;
        uint64 reinvestmentFund;
        uint64 totalDistributed;
        uint64 totalDeposited;

        uint64 lastDistributionEpoch;
        uint64 stakerPool;
        uint64 clanPool;
        uint64 clanWeightedTotal;
    };

    // ======================== INPUT / OUTPUT ========================

    struct DepositRevenue_input { };
    struct DepositRevenue_output { uint32 returnCode; };

    struct Stake_input { };
    struct Stake_output { uint32 returnCode; };
    struct Stake_locals { uint64 existing; };

    struct Unstake_input { };
    struct Unstake_output { uint32 returnCode; uint64 amount; };
    struct Unstake_locals { uint64 staked; };

    struct Distribute_input { };
    struct Distribute_output { uint32 returnCode; uint64 distributed; };
    struct Distribute_locals
    {
        uint64 amount;
        uint64 stakerShare;
        uint64 shareholderShare;
        uint64 clanShare;
        uint64 reinvestShare;
    };

    struct ClaimStakerReward_input { };
    struct ClaimStakerReward_output { uint32 returnCode; uint64 amount; };
    struct ClaimStakerReward_locals
    {
        uint64 userStake;
        uint64 reward;
        Entity entity;
        uint64 balance;
    };

    struct ClaimClanReward_input { };
    struct ClaimClanReward_output { uint32 returnCode; uint64 amount; };
    struct ClaimClanReward_locals
    {
        uint64 rank;
        uint64 multiplier;
        uint64 reward;
        Entity entity;
        uint64 balance;
    };

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

    struct GetStatus_input { };
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

    struct GetStakerInfo_input { id stakerAddress; };
    struct GetStakerInfo_output { uint64 stakedAmount; uint32 isStaked; };
    struct GetStakerInfo_locals { uint64 val; };

    struct GetClanMemberInfo_input { id memberAddress; };
    struct GetClanMemberInfo_output { uint64 rank; uint32 isMember; };
    struct GetClanMemberInfo_locals { uint64 val; };

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

    PUBLIC_FUNCTION_WITH_LOCALS(GetStakerInfo)
    {
        output.isStaked = state.get().stakerAmounts.get(input.stakerAddress, locals.val) ? 1 : 0;
        if (output.isStaked)
        {
            output.stakedAmount = locals.val;
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

    PUBLIC_PROCEDURE_WITH_LOCALS(Stake)
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

        if (state.get().stakerAmounts.get(qpi.invocator(), locals.existing))
        {
            state.mut().stakerAmounts.replace(qpi.invocator(), locals.existing + qpi.invocationReward());
        }
        else
        {
            state.mut().stakerAmounts.set(qpi.invocator(), qpi.invocationReward());
            state.mut().stakerCount = state.get().stakerCount + 1;
        }
        state.mut().totalStaked = state.get().totalStaked + qpi.invocationReward();
        output.returnCode = WOLFPACK_OK;
    }

    PUBLIC_PROCEDURE_WITH_LOCALS(Unstake)
    {
        if (!state.get().stakerAmounts.get(qpi.invocator(), locals.staked))
        {
            output.returnCode = WOLFPACK_ERROR_NOT_STAKED;
            return;
        }

        output.amount = locals.staked;
        state.mut().totalStaked = state.get().totalStaked - locals.staked;
        state.mut().stakerAmounts.removeByKey(qpi.invocator());
        state.mut().stakerCount = state.get().stakerCount - 1;

        qpi.transfer(qpi.invocator(), locals.staked);
        output.returnCode = WOLFPACK_OK;
    }

    PUBLIC_PROCEDURE_WITH_LOCALS(Distribute)
    {
        locals.amount = state.get().pendingRevenue;
        if (locals.amount == 0)
        {
            output.returnCode = WOLFPACK_ERROR_ZERO_AMOUNT;
            return;
        }

        locals.stakerShare = div(locals.amount * WOLFPACK_DISTRIBUTION_PERMILLE_STAKERS, 1000ULL);
        locals.shareholderShare = div(locals.amount * WOLFPACK_DISTRIBUTION_PERMILLE_SHAREHOLDERS, 1000ULL);
        locals.clanShare = div(locals.amount * WOLFPACK_DISTRIBUTION_PERMILLE_CLAN, 1000ULL);
        locals.reinvestShare = locals.amount - locals.stakerShare - locals.shareholderShare - locals.clanShare;

        state.mut().stakerPool = state.get().stakerPool + locals.stakerShare;
        state.mut().clanPool = state.get().clanPool + locals.clanShare;
        state.mut().reinvestmentFund = state.get().reinvestmentFund + locals.reinvestShare;
        state.mut().pendingRevenue = 0;
        state.mut().totalDistributed = state.get().totalDistributed + locals.amount;
        state.mut().lastDistributionEpoch = qpi.epoch();

        qpi.burn(locals.shareholderShare);

        output.distributed = locals.amount;
        output.returnCode = WOLFPACK_OK;
    }

    PUBLIC_PROCEDURE_WITH_LOCALS(ClaimStakerReward)
    {
        if (!state.get().stakerAmounts.get(qpi.invocator(), locals.userStake))
        {
            output.returnCode = WOLFPACK_ERROR_NOT_STAKED;
            return;
        }

        if (state.get().totalStaked == 0 || state.get().stakerPool == 0)
        {
            output.returnCode = WOLFPACK_ERROR_ZERO_AMOUNT;
            return;
        }

        locals.reward = div(state.get().stakerPool * locals.userStake, state.get().totalStaked);
        if (locals.reward == 0)
        {
            output.returnCode = WOLFPACK_ERROR_ZERO_AMOUNT;
            return;
        }

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

    PUBLIC_PROCEDURE_WITH_LOCALS(ClaimClanReward)
    {
        if (!state.get().clanRanks.get(qpi.invocator(), locals.rank))
        {
            output.returnCode = WOLFPACK_ERROR_NOT_CLAN_MEMBER;
            return;
        }

        if (state.get().clanPool == 0 || state.get().clanWeightedTotal == 0)
        {
            output.returnCode = WOLFPACK_ERROR_ZERO_AMOUNT;
            return;
        }

        locals.multiplier = WOLFPACK_RANK_MULTIPLIER_0;
        if (locals.rank == 1) locals.multiplier = WOLFPACK_RANK_MULTIPLIER_1;
        if (locals.rank == 2) locals.multiplier = WOLFPACK_RANK_MULTIPLIER_2;
        if (locals.rank == 3) locals.multiplier = WOLFPACK_RANK_MULTIPLIER_3;
        if (locals.rank == 4) locals.multiplier = WOLFPACK_RANK_MULTIPLIER_4;

        locals.reward = div(state.get().clanPool * locals.multiplier, state.get().clanWeightedTotal);
        if (locals.reward == 0)
        {
            output.returnCode = WOLFPACK_ERROR_ZERO_AMOUNT;
            return;
        }

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

    // ======================== REGISTRATION ========================

    REGISTER_USER_FUNCTIONS_AND_PROCEDURES()
    {
        REGISTER_USER_FUNCTION(GetStatus, 1);
        REGISTER_USER_FUNCTION(GetStakerInfo, 2);
        REGISTER_USER_FUNCTION(GetClanMemberInfo, 3);

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
        state.mut().clanPool = 0;
        state.mut().clanWeightedTotal = 0;
    }

    BEGIN_EPOCH()
    {
    }

    END_EPOCH()
    {
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
