// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2017-2019 The WaykiChain Developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef CONFIGURATION_H_
#define CONFIGURATION_H_

#include "chainparams.h"
#include "commons/arith_uint256.h"
#include "commons/uint256.h"
#include "commons/util.h"
#include "version.h"

#include <map>
#include <memory>
#include <vector>

using namespace std;

class CBlockIndex;
class uint256;
class G_CONFIG_TABLE;

const G_CONFIG_TABLE& IniCfg();

class G_CONFIG_TABLE {
public:
    string GetCoinName() const { return COIN_NAME; }
    const string GetAlertPkey(const NET_TYPE type) const;

    const vector<string> GetInitPubKey(const NET_TYPE type) const;
    uint8_t GetGenesisBlockNonce(const NET_TYPE type) const;
    uint256 GetGenesisBlockHash(const NET_TYPE type) const;
    string GetDelegateSignature(const NET_TYPE type) const;
    const vector<string> GetDelegatePubKey(const NET_TYPE type) const;
    const uint256 GetMerkleRootHash() const;
    vector<uint32_t> GetSeedNodeIP() const;
    uint8_t* GetMagicNumber(const NET_TYPE type) const;
    vector<uint8_t> GetAddressPrefix(const NET_TYPE type, const Base58Type BaseType) const;
    uint32_t GetDefaultPort(const NET_TYPE type) const;
    uint32_t GetRPCPort(const NET_TYPE type) const;
    uint32_t GetStartTimeInit(const NET_TYPE type) const;
    uint32_t GetTotalDelegateNum() const;
    uint32_t GetMaxVoteCandidateNum() const;
    uint64_t GetCoinInitValue() const { return InitialCoin; };

private:
    static string COIN_NAME; /* basecoin name */

    /* initial public key */
    static vector<string> initPubKey[3];

    /* delegate public key */
    static vector<string> delegatePubKey[3];

    /* delegate signature */
    static string delegateSignature[3];

    /* gensis block hash */
    static string genesisBlockHash[3];

    /* alert public key */
    static string AlertPubKey[2];

    /* merkle root hash */
    static string MerkleRootHash;

    /* fund coin initial owner public key */
    static string initFcoinOwnerPubKey[3];

    /* DEX order-matching service's public key */
    static string dexMatchPubKey[3];

    /* txids in stable coin genesis */
    static vector<string> stableCoinGenesisTxid[3];
    /* Peer IP seeds */
    static vector<uint32_t> pnSeed;

    /* Genesis Block Nonce */
    static uint8_t GenesisBlockNonce[3];

    /* Network Magic Number */
    static uint8_t MessageMagicNumber[3][MESSAGE_START_SIZE];

    /* Address Prefix */
    static vector<uint8_t> AddrPrefix[2][MAX_BASE58_TYPES];

    /* P2P Port */
    static uint32_t nP2PPort[3];

    /* RPC Port */
    static uint32_t nRPCPort[2];

    /* Start Time */
    static uint32_t StartTime[3];

    /* Initial Coin */
    static uint64_t InitialCoin;

    /* Default Miner Fee */
    static uint64_t DefaultFee;

    /* Total Delegate Number */
    static uint32_t TotalDelegateNum;

    /* Max Number of Delegate Candidate to Vote for by a single account */
    static uint32_t MaxVoteCandidateNum;
};

inline FeatureForkVersionEnum GetFeatureForkVersion(const int32_t currBlockHeight) {
    if (currBlockHeight >= 0) // TODO: Kevin
        return MAJOR_VER_R2;
    else
        return MAJOR_VER_R1;
}

inline uint32_t GetBlockInterval(const int32_t currBlockHeight) {
    return 10; // TODO: Kevin
}

inline uint32_t GetYearBlockCount(const int32_t currBlockHeight) {
    return 365 /* days/year */ * 24 /* hours/day */ * 60 * 60 / GetBlockInterval(currBlockHeight);
}

inline uint32_t GetDayBlockCount(const int32_t currBlockHeight) {
    return 24 /* hours/day */ * 60 * 60 / GetBlockInterval(currBlockHeight);
}

inline uint32_t GetJumpHeightBySubsidy(const uint8_t targetSubsidyRate) {
    assert(targetSubsidyRate >= FIXED_SUBSIDY_RATE && targetSubsidyRate <= INITIAL_SUBSIDY_RATE);

    static map<uint8_t, uint32_t> subsidyRate2BlockHeight;
    static bool initialized = false;

    if (!initialized) {
        uint32_t jumpHeight        = 0;
        uint32_t featureForkHeight = 0; // TODO: Kevin
        uint32_t yearHeightV1      = SysCfg().NetworkID() == REGTEST_NET ? 500 : 3153600;    // pre-stable coin release
        uint32_t yearHeightV2      = SysCfg().NetworkID() == REGTEST_NET ? 1500 : 10512000;  // stable coin release
        uint32_t actualJumpHeight  = yearHeightV1;
        bool switched              = false;

        for (uint8_t subsidyRate = 5; subsidyRate >= 1; --subsidyRate) {
            subsidyRate2BlockHeight[subsidyRate] = jumpHeight;

            if (!switched) {
                if (jumpHeight + actualJumpHeight > featureForkHeight) {
                    jumpHeight = featureForkHeight + (1.0 - (1.0 * featureForkHeight - jumpHeight) / yearHeightV1) * yearHeightV2;
                    actualJumpHeight = yearHeightV2;
                    switched         = true;
                } else if (jumpHeight + actualJumpHeight == featureForkHeight) {
                    jumpHeight       = jumpHeight + actualJumpHeight;
                    actualJumpHeight = yearHeightV2;
                    switched         = true;
                } else {
                    jumpHeight = jumpHeight + actualJumpHeight;
                }
            } else {
                jumpHeight = jumpHeight + actualJumpHeight;
            }
        }

        initialized = true;
        assert(subsidyRate2BlockHeight.size() == 5);
    }

    // for (const auto& item : subsidyRate2BlockHeight) {
    //     LogPrint("DEUBG", "subsidyRate -> blockHeight: %d -> %u\n", item.first, item.second);
    // }

    return subsidyRate2BlockHeight.at(targetSubsidyRate);
}

inline uint8_t GetSubsidyRate(const int32_t currBlockHeight) {
    for (uint8_t subsidyRate = FIXED_SUBSIDY_RATE; subsidyRate <= INITIAL_SUBSIDY_RATE; ++subsidyRate) {
        if ((uint32_t)currBlockHeight >= GetJumpHeightBySubsidy(subsidyRate))
            return subsidyRate;
    }

    assert(false && "failed to acquire subsidy rate");
    return 0;
}

static const int32_t INIT_BLOCK_VERSION = 1;

/* No amount larger than this (in sawi) is valid */
static const int64_t BASECOIN_MAX_MONEY   = IniCfg().GetCoinInitValue() * COIN;  // 210 million
static const int64_t FUNDCOIN_MAX_MONEY   = BASECOIN_MAX_MONEY * 100;            // 21000 million
static const int64_t STABLECOIN_MAX_MONEY = BASECOIN_MAX_MONEY * 10;             // 2100 million

inline int64_t GetBaseCoinMaxMoney() { return BASECOIN_MAX_MONEY; }
inline bool CheckBaseCoinRange(const int64_t amount) { return (amount >= 0 && amount <= BASECOIN_MAX_MONEY); }
inline bool CheckFundCoinRange(const int64_t amount) { return (amount >= 0 && amount <= FUNDCOIN_MAX_MONEY); }
inline bool CheckStableCoinRange(const int64_t amount) { return (amount >= 0 && amount <= STABLECOIN_MAX_MONEY); }

#endif /* CONFIGURATION_H_ */
