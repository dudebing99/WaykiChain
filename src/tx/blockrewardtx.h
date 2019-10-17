// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2017-2019 The WaykiChain Developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.


#ifndef TX_BLOCK_REWARD_H
#define TX_BLOCK_REWARD_H

#include "tx.h"

class CBlockRewardTx : public CBaseTx {
public:
    uint64_t reward_fees;

public:
    CBlockRewardTx(): CBaseTx(BLOCK_REWARD_TX), reward_fees(0) {}

    CBlockRewardTx(const UnsignedCharArray &accountIn, const uint64_t rewardFeesIn, const int32_t nValidHeightIn):
        CBaseTx(BLOCK_REWARD_TX) {
        if (accountIn.size() > 6) {
            txUid = CPubKey(accountIn);
        } else {
            txUid = CRegID(accountIn);
        }
        reward_fees  = rewardFeesIn;
        valid_height = nValidHeightIn;
    }
    ~CBlockRewardTx() {}

    IMPLEMENT_SERIALIZE(
        READWRITE(VARINT(this->nVersion));
        nVersion = this->nVersion;
        READWRITE(txUid);

        // Do NOT change the order.
        READWRITE(VARINT(reward_fees));
        READWRITE(VARINT(valid_height));)

    TxID ComputeSignatureHash(bool recalculate = false) const {
        if (recalculate || sigHash.IsNull()) {
            CHashWriter ss(SER_GETHASH, 0);
            ss << VARINT(nVersion) << uint8_t(nTxType) << txUid << VARINT(reward_fees) << VARINT(valid_height);
            sigHash = ss.GetHash();
        }

        return sigHash;
    }

    std::shared_ptr<CBaseTx> GetNewInstance() const { return std::make_shared<CBlockRewardTx>(*this); }

    virtual string ToString(CAccountDBCache &accountCache);
    virtual Object ToJson(const CAccountDBCache &accountCache) const;

    bool GetInvolvedKeyIds(CCacheWrapper &cw, set<CKeyID> &keyIds) { return true; }

    virtual bool CheckTx(CTxExecuteContext &context);
    virtual bool ExecuteTx(CTxExecuteContext &context);
};

#endif // TX_BLOCK_REWARD_H
