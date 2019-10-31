// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2017-2019 The WaykiChain Developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.


#include "cointransfertx.h"

#include "main.h"

bool CUCoinTransferTx::CheckTx(CTxExecuteContext &context) {
    CCacheWrapper &cw       = *context.pCw;
    CValidationState &state = *context.pState;

    IMPLEMENT_DISABLE_TX_PRE_STABLE_COIN_RELEASE;
    IMPLEMENT_CHECK_TX_FEE(fee_symbol);
    IMPLEMENT_CHECK_TX_MEMO;
    IMPLEMENT_CHECK_TX_REGID_OR_PUBKEY(txUid.type());

    if (transfers.empty() || transfers.size() > MAX_TRANSFER_SIZE) {
        return state.DoS(100, ERRORMSG("CUCoinTransferTx::CheckTx, transfers is empty or too large count=%d than %d",
            transfers.size(), MAX_TRANSFER_SIZE),
                        REJECT_INVALID, "invalid-transfers");
    }

    for (size_t i = 0; i < transfers.size(); i++) {
        IMPLEMENT_CHECK_TX_REGID_OR_KEYID(transfers[i].to_uid.type());
        auto pSymbolErr = cw.assetCache.CheckTransferCoinSymbol(transfers[i].coin_symbol);
        if (pSymbolErr) {
            return state.DoS(100, ERRORMSG("CUCoinTransferTx::CheckTx, transfers[%d], invalid coin_symbol=%s, %s",
                i, transfers[i].coin_symbol, *pSymbolErr), REJECT_INVALID, "invalid-coin-symbol");
        }

        if (transfers[i].coin_amount < DUST_AMOUNT_THRESHOLD)
            return state.DoS(100, ERRORMSG("CUCoinTransferTx::CheckTx, transfers[%d], dust amount, %llu < %llu",
                i, transfers[i].coin_amount, DUST_AMOUNT_THRESHOLD), REJECT_DUST, "invalid-coin-amount");


        if (!CheckCoinRange(transfers[i].coin_symbol, transfers[i].coin_amount))
            return state.DoS(100,
                ERRORMSG("CUCoinTransferTx::CheckTx, transfers[%d], coin_symbol=%s, coin_amount=%llu out of valid range",
                         i, transfers[i].coin_symbol, transfers[i].coin_amount), REJECT_DUST, "invalid-coin-amount");
    }

    uint64_t minFee;
    if (!GetTxMinFee(nTxType, context.height, fee_symbol, minFee)) { assert(false); /* has been check before */ }

    if (llFees < transfers.size() * minFee) {
        return state.DoS(100, ERRORMSG("CUCoinTransferTx::CheckTx, tx fee too small (height: %d, fee symbol: %s, fee: %llu)",
                         context.height, fee_symbol, llFees), REJECT_INVALID, "bad-tx-fee-toosmall");
    }

    if ((txUid.type() == typeid(CPubKey)) && !txUid.get<CPubKey>().IsFullyValid())
        return state.DoS(100, ERRORMSG("CUCoinTransferTx::CheckTx, public key is invalid"), REJECT_INVALID,
                         "bad-publickey");

    CAccount srcAccount;
    if (!cw.accountCache.GetAccount(txUid, srcAccount))
        return state.DoS(100, ERRORMSG("CUCoinTransferTx::CheckTx, read account failed"), REJECT_INVALID,
                         "bad-getaccount");

    CPubKey pubKey = (txUid.type() == typeid(CPubKey) ? txUid.get<CPubKey>() : srcAccount.owner_pubkey);
    IMPLEMENT_CHECK_TX_SIGNATURE(pubKey);

    return true;
}

bool CUCoinTransferTx::ExecuteTx(CTxExecuteContext &context) {
    CCacheWrapper &cw       = *context.pCw;
    CValidationState &state = *context.pState;

    CAccount srcAccount;
    if (!cw.accountCache.GetAccount(txUid, srcAccount))
        return state.DoS(100, ERRORMSG("CUCoinTransferTx::ExecuteTx, read txUid %s account info error",
                        txUid.ToString()), READ_ACCOUNT_FAIL, "bad-read-accountdb");

    if (!GenerateRegID(context, srcAccount)) {
        return false;
    }

    if (!srcAccount.OperateBalance(fee_symbol, SUB_FREE, llFees)) {
        return state.DoS(100, ERRORMSG("CUCoinTransferTx::ExecuteTx, insufficient coin_amount in txUid %s account",
                        txUid.ToString()), UPDATE_ACCOUNT_FAIL, "insufficient-coin_amount");
    }

    vector<CReceipt> receipts;

    for (size_t i = 0; i < transfers.size(); i++) {
        const auto &transfer = transfers[i];

        if (!srcAccount.OperateBalance(transfer.coin_symbol, SUB_FREE, transfer.coin_amount)) {
            return state.DoS(100, ERRORMSG("CUCoinTransferTx::ExecuteTx, transfers[%d], insufficient coins in txUid %s account",
                            i, txUid.ToString()), UPDATE_ACCOUNT_FAIL, "insufficient-coins");
        }

        uint64_t actualCoinsToSend = transfer.coin_amount;
        if (srcAccount.IsMyUid(transfer.to_uid)) {
            if (!srcAccount.OperateBalance(transfer.coin_symbol, ADD_FREE, actualCoinsToSend)) {
                return state.DoS(100, ERRORMSG("CUCoinTransferTx::ExecuteTx, transfers[%d], failed to add coins in toUid %s account",
                    i, transfer.to_uid.ToDebugString()), UPDATE_ACCOUNT_FAIL, "failed-add-coins");
            }
        } else {
            CAccount desAccount;
            if (!cw.accountCache.GetAccount(transfer.to_uid, desAccount)) { // first involved in transacion
                if (transfer.to_uid.is<CKeyID>()) {
                    desAccount = CAccount(transfer.to_uid.get<CKeyID>());
                } else {
                    return state.DoS(100, ERRORMSG("CUCoinTransferTx::ExecuteTx, get account info failed"),
                                    READ_ACCOUNT_FAIL, "bad-read-accountdb");
                }
            }

            if (!desAccount.OperateBalance(transfer.coin_symbol, ADD_FREE, actualCoinsToSend)) {
                return state.DoS(100, ERRORMSG("CUCoinTransferTx::ExecuteTx, transfers[%d], failed to add coins in toUid %s account",
                    i, transfer.to_uid.ToDebugString()), UPDATE_ACCOUNT_FAIL, "failed-add-coins");
            }

            if (!cw.accountCache.SaveAccount(desAccount))
                return state.DoS(100, ERRORMSG("CUCoinTransferTx::ExecuteTx, write dest addr %s account info error",
                    transfer.to_uid.ToDebugString()), UPDATE_ACCOUNT_FAIL, "bad-read-accountdb");
        }
    }

    if (!cw.accountCache.SaveAccount(srcAccount))
        return state.DoS(100, ERRORMSG("CUCoinTransferTx::ExecuteTx, write source addr %s account info error",
                        txUid.ToString()), UPDATE_ACCOUNT_FAIL, "bad-read-accountdb");

    if (!receipts.empty() && !cw.txReceiptCache.SetTxReceipts(GetHash(), receipts))
        return state.DoS(100, ERRORMSG("CCDPStakeTx::ExecuteTx, set tx receipts failed!! txid=%s",
                        GetHash().ToString()), REJECT_INVALID, "set-tx-receipt-failed");

    return true;
}

string CUCoinTransferTx::ToString(CAccountDBCache &accountCache) {
    string transferStr = "";
    for (const auto &transfer : transfers) {
        if (!transferStr.empty()) transferStr += ",";
        transferStr += strprintf("{%s}", transfer.ToString(accountCache));
    }

    return strprintf(
        "txType=%s, hash=%s, ver=%d, txUid=%s, fee_symbol=%s, llFees=%llu, "
        "valid_height=%d, transfers=[%s], memo=%s",
        GetTxType(nTxType), GetHash().ToString(), nVersion, txUid.ToString(), fee_symbol, llFees,
        valid_height, transferStr, HexStr(memo));
}

Object CUCoinTransferTx::ToJson(const CAccountDBCache &accountCache) const {
    Object result = CBaseTx::ToJson(accountCache);

    Array transferArray;
    for (const auto &transfer : transfers) {
        transferArray.push_back(transfer.ToJson(accountCache));
    }

    result.push_back(Pair("transfers",   transferArray));
    result.push_back(Pair("memo",        memo));

    return result;
}
