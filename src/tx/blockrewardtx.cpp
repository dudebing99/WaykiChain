// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2017-2019 The WaykiChain Developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.


#include "blockrewardtx.h"

#include "entities/receipt.h"
#include "main.h"

bool CBlockRewardTx::CheckTx(CTxExecuteContext &context) { return true; }

bool CBlockRewardTx::ExecuteTx(CTxExecuteContext &context) {
    CCacheWrapper &cw       = *context.pCw;
    CValidationState &state = *context.pState;

    CAccount account;
    if (!cw.accountCache.GetAccount(txUid, account)) {
        return state.DoS(100, ERRORMSG("CBlockRewardTx::ExecuteTx, read source addr %s account info error",
            txUid.ToString()), UPDATE_ACCOUNT_FAIL, "bad-read-accountdb");
    }

    if (0 == context.index) {
        // When the reward transaction is immature, should NOT update account's balances.
    } else if (-1 == context.index) {
        // When the reward transaction is mature, update account's balances, i.e, assign the reward value to
        // the target account.
        if (!account.OperateBalance(SYMB::WICC, ADD_FREE, reward_fees)) {
            return state.DoS(100, ERRORMSG("CBlockRewardTx::ExecuteTx, opeate account failed"), UPDATE_ACCOUNT_FAIL,
                             "operate-account-failed");
        }

        CReceipt receipt(nullId, txUid, SYMB::WICC, reward_fees, ReceiptCode::BLOCK_REWORD_TO_MINER);
        if (!cw.txReceiptCache.SetTxReceipts(GetHash(), {receipt})) {
            return state.DoS(100, ERRORMSG("CBlockRewardTx::ExecuteTx, set tx receipts failed!! txid=%s",
                            GetHash().ToString()), REJECT_INVALID, "set-tx-receipt-failed");
        }
    } else {
        return ERRORMSG("CBlockRewardTx::ExecuteTx, invalid index");
    }

    if (!cw.accountCache.SetAccount(CUserID(account.keyid), account)) {
        return state.DoS(100, ERRORMSG("CBlockRewardTx::ExecuteTx, write secure account info error"),
                         UPDATE_ACCOUNT_FAIL, "bad-save-accountdb");
    }

    return true;
}

string CBlockRewardTx::ToString(CAccountDBCache &accountCache) {
    CKeyID keyId;
    accountCache.GetKeyId(txUid, keyId);

    return strprintf("txType=%s, hash=%s, ver=%d, account=%s, keyId=%s, reward=%ld", GetTxType(nTxType),
                     GetHash().ToString(), nVersion, txUid.ToString(), keyId.GetHex(), reward_fees);
}

Object CBlockRewardTx::ToJson(const CAccountDBCache &accountCache) const {
    Object result;
    CKeyID keyId;
    accountCache.GetKeyId(txUid, keyId);

    result.push_back(Pair("txid",           GetHash().GetHex()));
    result.push_back(Pair("tx_type",        GetTxType(nTxType)));
    result.push_back(Pair("ver",            nVersion));
    result.push_back(Pair("tx_uid",         txUid.ToString()));
    result.push_back(Pair("to_addr",        keyId.ToAddress()));
    result.push_back(Pair("valid_height",   valid_height));
    result.push_back(Pair("reward_fees",    reward_fees));

    return result;
}