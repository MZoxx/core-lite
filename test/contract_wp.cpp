#define NO_UEFI

#include "contract_testing.h"


class WolfPackChecker : public WOLFPACK, public WOLFPACK::StateData
{
public:
    void checkClanWeightConsistency()
    {
        uint64 expectedWeight = 0;
        sint64 idx = NULL_INDEX;
        while (true)
        {
            idx = clanRanks.nextElementIndex(idx);
            if (idx == NULL_INDEX) break;
            uint64 rank = clanRanks.value(idx);
            if (rank == 0) expectedWeight += WOLFPACK_RANK_MULTIPLIER_0;
            if (rank == 1) expectedWeight += WOLFPACK_RANK_MULTIPLIER_1;
            if (rank == 2) expectedWeight += WOLFPACK_RANK_MULTIPLIER_2;
            if (rank == 3) expectedWeight += WOLFPACK_RANK_MULTIPLIER_3;
            if (rank == 4) expectedWeight += WOLFPACK_RANK_MULTIPLIER_4;
        }
        EXPECT_EQ(clanWeightedTotal, expectedWeight);
    }
};

static const id adminAddr(1, 2, 3, 4);
static const id user1(10, 20, 30, 40);
static const id user2(11, 21, 31, 41);
static const id user3(12, 22, 32, 42);
static const id depositor(5, 6, 7, 8);


class ContractTestingWP : protected ContractTesting
{
public:
    ContractTestingWP()
    {
        initEmptySpectrum();
        initEmptyUniverse();
        INIT_CONTRACT(WOLFPACK);
        callSystemProcedure(WOLFPACK_CONTRACT_INDEX, INITIALIZE);
    }

    WolfPackChecker* getState()
    {
        return (WolfPackChecker*)contractStates[WOLFPACK_CONTRACT_INDEX];
    }

    void beginEpoch()
    {
        callSystemProcedure(WOLFPACK_CONTRACT_INDEX, BEGIN_EPOCH);
    }

    void endEpoch()
    {
        callSystemProcedure(WOLFPACK_CONTRACT_INDEX, END_EPOCH);
    }

    void beginTick()
    {
        callSystemProcedure(WOLFPACK_CONTRACT_INDEX, BEGIN_TICK);
    }

    void endTick()
    {
        callSystemProcedure(WOLFPACK_CONTRACT_INDEX, END_TICK);
    }

    WOLFPACK::DepositRevenue_output depositRevenue(const id& sender, sint64 amount)
    {
        WOLFPACK::DepositRevenue_input input;
        WOLFPACK::DepositRevenue_output output;
        invokeUserProcedure(WOLFPACK_CONTRACT_INDEX, 1, input, output, sender, amount);
        return output;
    }

    WOLFPACK::AddClanMember_output addClanMember(const id& sender, const id& member, uint64 rank)
    {
        WOLFPACK::AddClanMember_input input{ member, rank };
        WOLFPACK::AddClanMember_output output;
        invokeUserProcedure(WOLFPACK_CONTRACT_INDEX, 2, input, output, sender, 0);
        return output;
    }

    WOLFPACK::RemoveClanMember_output removeClanMember(const id& sender, const id& member)
    {
        WOLFPACK::RemoveClanMember_input input{ member };
        WOLFPACK::RemoveClanMember_output output;
        invokeUserProcedure(WOLFPACK_CONTRACT_INDEX, 3, input, output, sender, 0);
        return output;
    }

    WOLFPACK::SetClanRank_output setClanRank(const id& sender, const id& member, uint64 rank)
    {
        WOLFPACK::SetClanRank_input input{ member, rank };
        WOLFPACK::SetClanRank_output output;
        invokeUserProcedure(WOLFPACK_CONTRACT_INDEX, 4, input, output, sender, 0);
        return output;
    }

    WOLFPACK::SetAdmin_output setAdmin(const id& sender, const id& newAdmin)
    {
        WOLFPACK::SetAdmin_input input{ newAdmin };
        WOLFPACK::SetAdmin_output output;
        invokeUserProcedure(WOLFPACK_CONTRACT_INDEX, 5, input, output, sender, 0);
        return output;
    }

    WOLFPACK::SetExcludeAddress_output setExcludeAddress(const id& sender, uint64 slot, const id& address)
    {
        WOLFPACK::SetExcludeAddress_input input{ slot, address };
        WOLFPACK::SetExcludeAddress_output output;
        invokeUserProcedure(WOLFPACK_CONTRACT_INDEX, 6, input, output, sender, 0);
        return output;
    }

    WOLFPACK::GetStatus_output getStatus()
    {
        WOLFPACK::GetStatus_input input;
        WOLFPACK::GetStatus_output output;
        callFunction(WOLFPACK_CONTRACT_INDEX, 1, input, output);
        return output;
    }

    WOLFPACK::GetHolderInfo_output getHolderInfo(const id& holder)
    {
        WOLFPACK::GetHolderInfo_input input{ holder };
        WOLFPACK::GetHolderInfo_output output;
        callFunction(WOLFPACK_CONTRACT_INDEX, 2, input, output);
        return output;
    }

    WOLFPACK::GetClanMemberInfo_output getClanMemberInfo(const id& member)
    {
        WOLFPACK::GetClanMemberInfo_input input{ member };
        WOLFPACK::GetClanMemberInfo_output output;
        callFunction(WOLFPACK_CONTRACT_INDEX, 3, input, output);
        return output;
    }

    WOLFPACK::GetShareholderInfo_output getShareholderInfo(const id& shareholder)
    {
        WOLFPACK::GetShareholderInfo_input input{ shareholder };
        WOLFPACK::GetShareholderInfo_output output;
        callFunction(WOLFPACK_CONTRACT_INDEX, 4, input, output);
        return output;
    }
};

// ============================================================================
// Initialization
// ============================================================================

TEST(TestWolfPack, Initialization)
{
    ContractTestingWP wp;
    auto* s = wp.getState();

    EXPECT_EQ(s->totalTokensSnapshot, 0ULL);
    EXPECT_EQ(s->holderCount, 0ULL);
    EXPECT_EQ(s->totalSharesSnapshot, 0ULL);
    EXPECT_EQ(s->shareholderCount, 0ULL);
    EXPECT_EQ(s->clanMemberCount, 0ULL);
    EXPECT_EQ(s->clanWeightedTotal, 0ULL);
    EXPECT_EQ(s->pendingRevenue, 0ULL);
    EXPECT_EQ(s->reinvestmentFund, 0ULL);
    EXPECT_EQ(s->totalDistributed, 0ULL);
    EXPECT_EQ(s->totalDeposited, 0ULL);
    EXPECT_EQ(s->lastDistributionEpoch, 0ULL);
    EXPECT_EQ(s->lastPayoutTick, 0ULL);
    EXPECT_TRUE(isZero(s->excludeAddress1));
    EXPECT_TRUE(isZero(s->excludeAddress2));
}

// ============================================================================
// DepositRevenue
// ============================================================================

TEST(TestWolfPack, DepositRevenue)
{
    ContractTestingWP wp;
    increaseEnergy(depositor, 500000);

    auto out = wp.depositRevenue(depositor, 100000);
    EXPECT_EQ(out.returnCode, WOLFPACK_OK);

    auto* s = wp.getState();
    EXPECT_EQ(s->pendingRevenue, 100000ULL);
    EXPECT_EQ(s->totalDeposited, 100000ULL);

    // Second deposit
    out = wp.depositRevenue(depositor, 50000);
    EXPECT_EQ(out.returnCode, WOLFPACK_OK);
    EXPECT_EQ(s->pendingRevenue, 150000ULL);
    EXPECT_EQ(s->totalDeposited, 150000ULL);
}

TEST(TestWolfPack, DepositRevenueZeroFails)
{
    ContractTestingWP wp;
    increaseEnergy(depositor, 500000);

    auto out = wp.depositRevenue(depositor, 0);
    EXPECT_EQ(out.returnCode, WOLFPACK_ERROR_ZERO_AMOUNT);
}

// ============================================================================
// Clan management
// ============================================================================

TEST(TestWolfPack, AddClanMember)
{
    ContractTestingWP wp;

    auto out = wp.addClanMember(adminAddr, user1, 0);
    EXPECT_EQ(out.returnCode, WOLFPACK_OK);

    auto* s = wp.getState();
    EXPECT_EQ(s->clanMemberCount, 1ULL);
    EXPECT_EQ(s->clanWeightedTotal, WOLFPACK_RANK_MULTIPLIER_0);

    out = wp.addClanMember(adminAddr, user2, 3);
    EXPECT_EQ(out.returnCode, WOLFPACK_OK);
    EXPECT_EQ(s->clanMemberCount, 2ULL);
    EXPECT_EQ(s->clanWeightedTotal, WOLFPACK_RANK_MULTIPLIER_0 + WOLFPACK_RANK_MULTIPLIER_3);

    s->checkClanWeightConsistency();
}

TEST(TestWolfPack, AddClanMemberAccessDenied)
{
    ContractTestingWP wp;

    auto out = wp.addClanMember(user1, user2, 0);
    EXPECT_EQ(out.returnCode, WOLFPACK_ERROR_ACCESS_DENIED);
}

TEST(TestWolfPack, AddClanMemberDuplicate)
{
    ContractTestingWP wp;

    wp.addClanMember(adminAddr, user1, 0);
    auto out = wp.addClanMember(adminAddr, user1, 1);
    EXPECT_EQ(out.returnCode, WOLFPACK_ERROR_ALREADY_CLAN_MEMBER);
}

TEST(TestWolfPack, AddClanMemberInvalidRank)
{
    ContractTestingWP wp;

    auto out = wp.addClanMember(adminAddr, user1, 5);
    EXPECT_EQ(out.returnCode, WOLFPACK_ERROR_INVALID_RANK);
}

TEST(TestWolfPack, RemoveClanMember)
{
    ContractTestingWP wp;

    wp.addClanMember(adminAddr, user1, 2);
    auto* s = wp.getState();
    EXPECT_EQ(s->clanMemberCount, 1ULL);
    EXPECT_EQ(s->clanWeightedTotal, WOLFPACK_RANK_MULTIPLIER_2);

    auto out = wp.removeClanMember(adminAddr, user1);
    EXPECT_EQ(out.returnCode, WOLFPACK_OK);
    EXPECT_EQ(s->clanMemberCount, 0ULL);
    EXPECT_EQ(s->clanWeightedTotal, 0ULL);
}

TEST(TestWolfPack, RemoveClanMemberNotFound)
{
    ContractTestingWP wp;

    auto out = wp.removeClanMember(adminAddr, user1);
    EXPECT_EQ(out.returnCode, WOLFPACK_ERROR_NOT_CLAN_MEMBER);
}

TEST(TestWolfPack, SetClanRank)
{
    ContractTestingWP wp;

    wp.addClanMember(adminAddr, user1, 1);
    auto* s = wp.getState();
    EXPECT_EQ(s->clanWeightedTotal, WOLFPACK_RANK_MULTIPLIER_1);

    auto out = wp.setClanRank(adminAddr, user1, 4);
    EXPECT_EQ(out.returnCode, WOLFPACK_OK);
    EXPECT_EQ(s->clanWeightedTotal, WOLFPACK_RANK_MULTIPLIER_4);

    s->checkClanWeightConsistency();
}

TEST(TestWolfPack, SetClanRankAccessDenied)
{
    ContractTestingWP wp;

    wp.addClanMember(adminAddr, user1, 0);
    auto out = wp.setClanRank(user2, user1, 3);
    EXPECT_EQ(out.returnCode, WOLFPACK_ERROR_ACCESS_DENIED);
}

TEST(TestWolfPack, SetClanRankNotMember)
{
    ContractTestingWP wp;

    auto out = wp.setClanRank(adminAddr, user1, 3);
    EXPECT_EQ(out.returnCode, WOLFPACK_ERROR_NOT_CLAN_MEMBER);
}

TEST(TestWolfPack, SetClanRankInvalidRank)
{
    ContractTestingWP wp;

    wp.addClanMember(adminAddr, user1, 0);
    auto out = wp.setClanRank(adminAddr, user1, 5);
    EXPECT_EQ(out.returnCode, WOLFPACK_ERROR_INVALID_RANK);
}

// ============================================================================
// Admin management
// ============================================================================

TEST(TestWolfPack, SetAdmin)
{
    ContractTestingWP wp;
    id newAdmin(50, 60, 70, 80);

    auto out = wp.setAdmin(adminAddr, newAdmin);
    EXPECT_EQ(out.returnCode, WOLFPACK_OK);

    auto* s = wp.getState();
    EXPECT_EQ(s->adminAddress, newAdmin);

    // Old admin should no longer work
    auto out2 = wp.addClanMember(adminAddr, user1, 0);
    EXPECT_EQ(out2.returnCode, WOLFPACK_ERROR_ACCESS_DENIED);

    // New admin works
    auto out3 = wp.addClanMember(newAdmin, user1, 0);
    EXPECT_EQ(out3.returnCode, WOLFPACK_OK);
}

TEST(TestWolfPack, SetAdminAccessDenied)
{
    ContractTestingWP wp;

    auto out = wp.setAdmin(user1, user2);
    EXPECT_EQ(out.returnCode, WOLFPACK_ERROR_ACCESS_DENIED);
}

// ============================================================================
// Exclude addresses
// ============================================================================

TEST(TestWolfPack, SetExcludeAddress)
{
    ContractTestingWP wp;
    id excl1(100, 200, 300, 400);
    id excl2(101, 201, 301, 401);

    auto out = wp.setExcludeAddress(adminAddr, 1, excl1);
    EXPECT_EQ(out.returnCode, WOLFPACK_OK);

    out = wp.setExcludeAddress(adminAddr, 2, excl2);
    EXPECT_EQ(out.returnCode, WOLFPACK_OK);

    auto* s = wp.getState();
    EXPECT_EQ(s->excludeAddress1, excl1);
    EXPECT_EQ(s->excludeAddress2, excl2);

    // Invalid slot
    out = wp.setExcludeAddress(adminAddr, 3, excl1);
    EXPECT_EQ(out.returnCode, WOLFPACK_ERROR_INVALID_RANK);
}

// ============================================================================
// GetStatus function
// ============================================================================

TEST(TestWolfPack, GetStatus)
{
    ContractTestingWP wp;
    increaseEnergy(depositor, 500000);

    wp.depositRevenue(depositor, 100000);

    auto status = wp.getStatus();
    EXPECT_EQ(status.pendingRevenue, 100000ULL);
    EXPECT_EQ(status.totalDeposited, 100000ULL);
    EXPECT_EQ(status.holderCount, 0ULL);
    EXPECT_EQ(status.clanMemberCount, 0ULL);
}

// ============================================================================
// GetClanMemberInfo function
// ============================================================================

TEST(TestWolfPack, GetClanMemberInfo)
{
    ContractTestingWP wp;

    wp.addClanMember(adminAddr, user1, 3);

    auto info = wp.getClanMemberInfo(user1);
    EXPECT_EQ(info.isMember, 1u);
    EXPECT_EQ(info.rank, 3ULL);

    // Non-member
    auto info2 = wp.getClanMemberInfo(user2);
    EXPECT_EQ(info2.isMember, 0u);
}

// ============================================================================
// Revenue split math
// ============================================================================

TEST(TestWolfPack, RevenueSplitMath)
{
    uint64 amount = 1000000ULL;
    uint64 holderShare = amount * WOLFPACK_DISTRIBUTION_PERMILLE_HOLDERS / 1000ULL;
    uint64 shareholderShare = amount * WOLFPACK_DISTRIBUTION_PERMILLE_SHAREHOLDERS / 1000ULL;
    uint64 clanShare = amount * WOLFPACK_DISTRIBUTION_PERMILLE_CLAN / 1000ULL;
    uint64 reinvestShare = amount - holderShare - shareholderShare - clanShare;

    EXPECT_EQ(holderShare, 700000ULL);
    EXPECT_EQ(shareholderShare, 100000ULL);
    EXPECT_EQ(clanShare, 100000ULL);
    EXPECT_EQ(reinvestShare, 100000ULL);
    EXPECT_EQ(holderShare + shareholderShare + clanShare + reinvestShare, amount);
}
