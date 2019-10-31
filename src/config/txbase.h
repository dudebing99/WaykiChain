// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2017-2019 The WaykiChain Developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef CONFIG_TXBASE_H
#define CONFIG_TXBASE_H

#include "const.h"

#include <unordered_map>
#include <unordered_set>
#include <string>
#include <cstdint>
#include <tuple>

using namespace std;

static const int32_t INIT_TX_VERSION = 1;

enum TxType : uint8_t {
    NULL_TX = 0,  //!< NULL_TX

    BLOCK_REWARD_TX     = 1,  //!< Miner Block Reward Tx
    ACCOUNT_REGISTER_TX = 2,  //!< Account Registration Tx
    DELEGATE_VOTE_TX    = 3,  //!< Vote Delegate Tx
    LCONTRACT_INVOKE_TX = 4,  //!< LuaVM Contract Invocation Tx
    LCONTRACT_DEPLOY_TX = 5,  //!< LuaVM Contract Deployment Tx

    UCOIN_TRANSFER_TX  = 11,  //!< Universal Coin Transfer Tx
    UCOIN_TRANSFER_MTX = 12,  //!< Multisig Tx
    UCOIN_STAKE_TX     = 13,  //!< Stake Fund Coin Tx in order to become a price feeder

    UCONTRACT_DEPLOY_TX = 21,  //!< universal VM contract deployment
    UCONTRACT_INVOKE_TX = 22,  //!< universal VM contract invocation

    ASSET_ISSUE_TX  = 31,  //!< a user issues onchain asset
    ASSET_UPDATE_TX = 32,  //!< a user update onchain asset
};

struct TxTypeHash {
    size_t operator()(const TxType &type) const noexcept { return std::hash<uint8_t>{}(type); }
};

static const unordered_set<string> kFeeSymbolSet = {
    SYMB::WICC,
    SYMB::WUSD
};

inline string GetFeeSymbolSetStr() {
    string ret = "";
    for (auto symbol : kFeeSymbolSet) {
        if (ret.empty()) {
            ret = symbol;
        } else {
            ret += "|" + symbol;
        }
    }
    return ret;
}

/**
 * TxTypeKey -> {   TxTypeName,
 *                  InterimPeriodTxFees(WICC), EffectivePeriodTxFees(WICC),
 *                  InterimPeriodTxFees(WUSD), EffectivePeriodTxFees(WUSD)
 *              }
 *
 * Fees are boosted by COIN=10^8
 */
static const unordered_map<TxType, std::tuple<string, uint64_t, uint64_t, uint64_t, uint64_t>, TxTypeHash> kTxFeeTable = {
/* tx type                                   tx type name               V1:WICC     V2:WICC    V1:WUSD     V2:WUSD           */
{ NULL_TX,                  std::make_tuple("NULL_TX",                  0,          0,          0,          0           ) },

{ BLOCK_REWARD_TX,          std::make_tuple("BLOCK_REWARD_TX",          0,          0,          0,          0           ) }, //deprecated

{ ACCOUNT_REGISTER_TX,      std::make_tuple("ACCOUNT_REGISTER_TX",      0,          0.1*COIN,   0.1*COIN,   0.1*COIN    ) }, //deprecated
{ LCONTRACT_DEPLOY_TX,      std::make_tuple("LCONTRACT_DEPLOY_TX",      1*COIN,     1*COIN,     1*COIN,     1*COIN      ) },
{ LCONTRACT_INVOKE_TX,      std::make_tuple("LCONTRACT_INVOKE_TX",      0,          0.01*COIN,  0.01*COIN,  0.01*COIN   ) }, //min fee
{ DELEGATE_VOTE_TX,         std::make_tuple("DELEGATE_VOTE_TX",         0,          0.01*COIN,  0.01*COIN,  0.01*COIN   ) },

{ UCOIN_TRANSFER_MTX,       std::make_tuple("UCOIN_TRANSFER_MTX",       0,          0.1*COIN,   0.1*COIN,   0.1*COIN    ) },
{ UCOIN_STAKE_TX,           std::make_tuple("UCOIN_STAKE_TX",           0,          0.01*COIN,  0.01*COIN,  0.01*COIN   ) },

{ ASSET_ISSUE_TX,           std::make_tuple("ASSET_ISSUE_TX",           0,          0.01*COIN,  0.01*COIN,  0.01*COIN   ) }, //plus 550 WICC
{ ASSET_UPDATE_TX,          std::make_tuple("ASSET_UPDATE_TX",          0,          0.01*COIN,  0.01*COIN,  0.01*COIN   ) }, //plus 110 WICC
{ UCOIN_TRANSFER_TX,        std::make_tuple("UCOIN_TRANSFER_TX",        0,          0.001*COIN, 0.001*COIN, 0.001*COIN  ) },

{ UCONTRACT_DEPLOY_TX,      std::make_tuple("UCONTRACT_DEPLOY_TX",      0,          1*COIN,     1*COIN,     1*COIN      ) },
{ UCONTRACT_INVOKE_TX,      std::make_tuple("UCONTRACT_INVOKE_TX",      0,          0.01*COIN,  0.01*COIN,  0.01*COIN   ) }, //min fee

};

#endif