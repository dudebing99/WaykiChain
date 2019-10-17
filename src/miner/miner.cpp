// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2017-2019 The WaykiChain Developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "miner.h"

#include "init.h"
#include "main.h"
#include "net.h"
#include "wallet/wallet.h"
#include "tx/tx.h"
#include "persistence/txdb.h"
#include "persistence/contractdb.h"
#include "persistence/cachewrapper.h"

#include <algorithm>
#include <boost/circular_buffer.hpp>

extern CWallet *pWalletMain;
extern void SetMinerStatus(bool bStatus);
//////////////////////////////////////////////////////////////////////////////
//
// CoinMiner
//

uint64_t nLastBlockTx   = 0;
uint64_t nLastBlockSize = 0;

MinedBlockInfo miningBlockInfo;
boost::circular_buffer<MinedBlockInfo> minedBlocks(MAX_MINED_BLOCK_COUNT);
CCriticalSection csMinedBlocks;

// base on the lastest 50 blocks
uint32_t GetElementForBurn(CBlockIndex *pIndex) {
    if (!pIndex) {
        return INIT_FUEL_RATES;
    }
    int32_t nBlock = SysCfg().GetArg("-blocksizeforburn", DEFAULT_BURN_BLOCK_SIZE);
    if (nBlock * 2 >= pIndex->height - 1) {
        return INIT_FUEL_RATES;
    }

    uint64_t nTotalStep   = 0;
    uint64_t nAverateStep = 0;
    uint32_t newFuelRate  = 0;
    CBlockIndex *pTemp    = pIndex;
    for (int32_t i = 0; i < nBlock; ++i) {
        nTotalStep += pTemp->nFuel / pTemp->nFuelRate * 100;
        pTemp = pTemp->pprev;
    }

    nAverateStep = nTotalStep / nBlock;
    if (nAverateStep < MAX_BLOCK_RUN_STEP * 0.75) {
        newFuelRate = pIndex->nFuelRate * 0.9;
    } else if (nAverateStep > MAX_BLOCK_RUN_STEP * 0.85) {
        newFuelRate = pIndex->nFuelRate * 1.1;
    } else {
        newFuelRate = pIndex->nFuelRate;
    }

    if (newFuelRate < MIN_FUEL_RATES) {
        newFuelRate = MIN_FUEL_RATES;
    }

    LogPrint("fuel", "preFuelRate=%d fuelRate=%d, height=%d\n", pIndex->nFuelRate, newFuelRate, pIndex->height);
    return newFuelRate;
}

// Sort transactions by priority and fee to decide priority orders to process transactions.
void GetPriorityTx(int32_t height, set<TxPriority> &txPriorities, const int32_t nFuelRate) {
    static TokenSymbol feeSymbol;
    static uint64_t fee    = 0;
    static uint32_t txSize = 0;
    static double feePerKb = 0;
    static double priority = 0;

    for (map<uint256, CTxMemPoolEntry>::iterator mi = mempool.memPoolTxs.begin(); mi != mempool.memPoolTxs.end(); ++mi) {
        CBaseTx *pBaseTx = mi->second.GetTransaction().get();
        if (!pBaseTx->IsBlockRewardTx() && pCdMan->pTxCache->HaveTx(pBaseTx->GetHash()) == uint256()) {
            feeSymbol = std::get<0>(mi->second.GetFees());
            fee       = std::get<1>(mi->second.GetFees());
            txSize    = mi->second.GetTxSize();
            feePerKb  = double(fee - pBaseTx->GetFuel(height, nFuelRate)) / txSize * 1000.0;
            priority  = mi->second.GetPriority();

            txPriorities.emplace(TxPriority(priority, feePerKb, mi->second.GetTransaction()));
        }
    }
}

bool GetCurrentDelegate(const int64_t currentTime, const int32_t currHeight, const vector<CRegID> &delegateList,
                               CRegID &delegate) {
    uint32_t slot  = currentTime / GetBlockInterval(currHeight);
    uint32_t index = slot % IniCfg().GetTotalDelegateNum();
    delegate       = delegateList[index];
    LogPrint("DEBUG", "currentTime=%lld, slot=%d, index=%d, regId=%s\n", currentTime, slot, index, delegate.ToString());

    return true;
}

bool CreateBlockRewardTx(const int64_t currentTime, const CAccount &delegate, CAccountDBCache &accountCache,
                         CBlock *pBlock) {
    CBlock previousBlock;
    CBlockIndex *pBlockIndex = mapBlockIndex[pBlock->GetPrevBlockHash()];
    if (pBlock->GetHeight() != 1 || pBlock->GetPrevBlockHash() != SysCfg().GetGenesisBlockHash()) {
        if (!ReadBlockFromDisk(pBlockIndex, previousBlock))
            return ERRORMSG("read block info fail from disk");

        CAccount prevDelegateAcct;
        if (!accountCache.GetAccount(previousBlock.vptx[0]->txUid, prevDelegateAcct)) {
            return ERRORMSG("get preblock delegate account info error");
        }

        if (currentTime - previousBlock.GetBlockTime() < GetBlockInterval(pBlock->GetHeight())) {
            if (prevDelegateAcct.regid == delegate.regid)
                return ERRORMSG("one delegate can't produce more than one block at the same slot");
        }
    }

    auto pRewardTx          = (CBlockRewardTx *)pBlock->vptx[0].get();
    pRewardTx->txUid        = delegate.regid;
    pRewardTx->valid_height = pBlock->GetHeight();

    pBlock->SetNonce(GetRand(SysCfg().GetBlockMaxNonce()));
    pBlock->SetMerkleRootHash(pBlock->BuildMerkleTree());
    pBlock->SetTime(currentTime);

    vector<uint8_t> signature;
    if (pWalletMain->Sign(delegate.keyid, pBlock->ComputeSignatureHash(), signature, delegate.miner_pubkey.IsValid())) {
        pBlock->SetSignature(signature);
        return true;
    } else {
        return ERRORMSG("Sign failed");
    }
}

void ShuffleDelegates(const int32_t nCurHeight, vector<CRegID> &delegateList) {
    uint32_t totalDelegateNum = IniCfg().GetTotalDelegateNum();
    string seedSource = strprintf("%u", nCurHeight / totalDelegateNum + (nCurHeight % totalDelegateNum > 0 ? 1 : 0));
    CHashWriter ss(SER_GETHASH, 0);
    ss << seedSource;
    uint256 currentSeed  = ss.GetHash();
    uint64_t newIndexSource = 0;
    for (uint32_t i = 0; i < totalDelegateNum; i++) {
        for (uint32_t x = 0; x < 4 && i < totalDelegateNum; i++, x++) {
            memcpy(&newIndexSource, currentSeed.begin() + (x * 8), 8);
            uint32_t newIndex      = newIndexSource % totalDelegateNum;
            CRegID regId           = delegateList[newIndex];
            delegateList[newIndex] = delegateList[i];
            delegateList[i]        = regId;
        }
        ss << currentSeed;
        currentSeed = ss.GetHash();
    }
}

bool VerifyRewardTx(const CBlock *pBlock, CCacheWrapper &cwIn, bool bNeedRunTx) {
    uint32_t maxNonce = SysCfg().GetBlockMaxNonce();

    vector<CRegID> delegateList;
    if (!cwIn.delegateCache.GetTopDelegateList(delegateList))
        return false;

    ShuffleDelegates(pBlock->GetHeight(), delegateList);

    CRegID regId;
    if (!GetCurrentDelegate(pBlock->GetTime(), pBlock->GetHeight(), delegateList, regId))
        return ERRORMSG("VerifyRewardTx() : failed to get current delegate");

    CAccount curDelegate;
    if (!cwIn.accountCache.GetAccount(regId, curDelegate))
        return ERRORMSG("VerifyRewardTx() : failed to get current delegate's account, regId=%s", regId.ToString());

    if (pBlock->GetNonce() > maxNonce)
        return ERRORMSG("VerifyRewardTx() : invalid nonce: %u", pBlock->GetNonce());

    if (pBlock->GetMerkleRootHash() != pBlock->BuildMerkleTree())
        return ERRORMSG("VerifyRewardTx() : wrong merkle root hash");

    auto spCW = std::make_shared<CCacheWrapper>(&cwIn);

    CBlockIndex *pBlockIndex = mapBlockIndex[pBlock->GetPrevBlockHash()];
    if (pBlock->GetHeight() != 1 || pBlock->GetPrevBlockHash() != SysCfg().GetGenesisBlockHash()) {
        CBlock previousBlock;
        if (!ReadBlockFromDisk(pBlockIndex, previousBlock))
            return ERRORMSG("VerifyRewardTx() : read block info failed from disk");

        CAccount prevDelegateAcct;
        if (!spCW->accountCache.GetAccount(previousBlock.vptx[0]->txUid, prevDelegateAcct))
            return ERRORMSG("VerifyRewardTx() : failed to get previous delegate's account, regId=%s",
                previousBlock.vptx[0]->txUid.ToString());

        if (pBlock->GetBlockTime() - previousBlock.GetBlockTime() < GetBlockInterval(pBlock->GetHeight())) {
            if (prevDelegateAcct.regid == curDelegate.regid)
                return ERRORMSG("VerifyRewardTx() : one delegate can't produce more than one block at the same slot");
        }
    }

    CAccount account;
    if (spCW->accountCache.GetAccount(pBlock->vptx[0]->txUid, account)) {
        if (curDelegate.regid != account.regid) {
            return ERRORMSG("VerifyRewardTx() : delegate should be (%s) vs what we got (%s)",
                            curDelegate.regid.ToString(), account.regid.ToString());
        }

        const auto &blockHash      = pBlock->ComputeSignatureHash();
        const auto &blockSignature = pBlock->GetSignature();

        if (blockSignature.size() == 0 || blockSignature.size() > MAX_SIGNATURE_SIZE) {
            return ERRORMSG("VerifyRewardTx() : invalid block signature size, hash=%s", blockHash.ToString());
        }

        if (!VerifySignature(blockHash, blockSignature, account.owner_pubkey))
            if (!VerifySignature(blockHash, blockSignature, account.miner_pubkey))
                return ERRORMSG("VerifyRewardTx() : verify signature error");
    } else {
        return ERRORMSG("VerifyRewardTx() : failed to get account info, regId=%s", pBlock->vptx[0]->txUid.ToString());
    }

    if (pBlock->vptx[0]->nVersion != INIT_TX_VERSION)
        return ERRORMSG("VerifyRewardTx() : transaction version %d vs current %d", pBlock->vptx[0]->nVersion, INIT_TX_VERSION);

    if (bNeedRunTx) {
        uint64_t totalFuel    = 0;
        uint64_t totalRunStep = 0;
        for (uint32_t i = 1; i < pBlock->vptx.size(); i++) {
            shared_ptr<CBaseTx> pBaseTx = pBlock->vptx[i];
            if (spCW->txCache.HaveTx(pBaseTx->GetHash()) != uint256())
                return ERRORMSG("VerifyRewardTx() : duplicate transaction, txid=%s", pBaseTx->GetHash().GetHex());

            CValidationState state;
            uint32_t prevBlockTime = pBlockIndex->pprev != nullptr ? pBlockIndex->pprev->GetBlockTime() : pBlockIndex->GetBlockTime();
            CTxExecuteContext context(pBlock->GetHeight(), i, pBlock->GetFuelRate(), pBlock->GetTime(), prevBlockTime, spCW.get(), &state);
            if (!pBaseTx->ExecuteTx(context)) {
                return ERRORMSG("VerifyRewardTx() : failed to execute transaction, txid=%s",
                                pBaseTx->GetHash().GetHex());
            }

            totalRunStep += pBaseTx->nRunStep;
            if (totalRunStep > MAX_BLOCK_RUN_STEP)
                return ERRORMSG("VerifyRewardTx() : block total run steps(%lu) exceed max run step(%lu)", totalRunStep,
                                MAX_BLOCK_RUN_STEP);

            uint32_t fuelFee = pBaseTx->GetFuel(pBlock->GetHeight(), pBlock->GetFuelRate());
            totalFuel += fuelFee;
            LogPrint("fuel", "VerifyRewardTx() : total fuel fee:%d, tx fuel fee:%d runStep:%d fuelRate:%d txid:%s\n", totalFuel,
                     fuelFee, pBaseTx->nRunStep, pBlock->GetFuelRate(), pBaseTx->GetHash().GetHex());
        }

        if (totalFuel != pBlock->GetFuel())
            return ERRORMSG("VerifyRewardTx() : total fuel fee(%lu) mismatch what(%u) in block header", totalFuel,
                            pBlock->GetFuel());
    }

    return true;
}

std::unique_ptr<CBlock> CreateNewBlock(CCacheWrapper &cwIn) {
    // Create new block
    std::unique_ptr<CBlock> pBlock(new CBlock());
    if (!pBlock.get())
        return nullptr;

    pBlock->vptx.push_back(std::make_shared<CBlockRewardTx>());

    // Largest block you're willing to create:
    uint32_t nBlockMaxSize = SysCfg().GetArg("-blockmaxsize", DEFAULT_BLOCK_MAX_SIZE);
    // Limit to between 1K and MAX_BLOCK_SIZE-1K for sanity:
    nBlockMaxSize = std::max<uint32_t>(1000, std::min<uint32_t>((MAX_BLOCK_SIZE - 1000), nBlockMaxSize));

    // Collect memory pool transactions into the block
    {
        LOCK2(cs_main, mempool.cs);

        CBlockIndex *pIndexPrev = chainActive.Tip();
        UpdateTime(*pBlock, pIndexPrev);
        uint32_t blockTime      = pBlock->GetTime();
        int32_t height          = pIndexPrev->height + 1;
        int32_t index           = 0; // block reward tx
        uint32_t fuelRate       = GetElementForBurn(pIndexPrev);
        uint64_t totalBlockSize = ::GetSerializeSize(*pBlock, SER_NETWORK, PROTOCOL_VERSION);
        uint64_t totalRunStep   = 0;
        uint64_t totalFees      = 0;
        uint64_t totalFuel      = 0;
        uint64_t reward         = 0;

        // Calculate && sort transactions from memory pool.
        set<TxPriority> txPriorities;
        GetPriorityTx(height, txPriorities, fuelRate);

        LogPrint("MINER", "CreateNewBlock() : got %lu transaction(s) sorted by priority rules\n",
                 txPriorities.size());

        // Collect transactions into the block.
        for (auto itor = txPriorities.rbegin(); itor != txPriorities.rend(); ++itor) {
            CBaseTx *pBaseTx = itor->baseTx.get();

            uint32_t txSize = pBaseTx->GetSerializeSize(SER_NETWORK, PROTOCOL_VERSION);
            if (totalBlockSize + txSize >= nBlockMaxSize) {
                LogPrint("MINER", "CreateNewBlock() : exceed max block size, txid: %s\n",
                         pBaseTx->GetHash().GetHex());
                continue;
            }

            auto spCW = std::make_shared<CCacheWrapper>(&cwIn);

            try {
                CValidationState state;
                pBaseTx->nFuelRate = fuelRate;
                uint32_t prevBlockTime = pIndexPrev->GetBlockTime();
                CTxExecuteContext context(height, index + 1, fuelRate, blockTime, prevBlockTime, spCW.get(), &state);
                if (!pBaseTx->CheckTx(context) || !pBaseTx->ExecuteTx(context)) {
                    LogPrint("MINER", "CreateNewBlock() : failed to pack transaction, txid: %s\n",
                            pBaseTx->GetHash().GetHex());
                    continue;
                }

                // Run step limits
                if (totalRunStep + pBaseTx->nRunStep >= MAX_BLOCK_RUN_STEP) {
                    LogPrint("MINER", "CreateNewBlock() : exceed max block run steps, txid: %s\n",
                            pBaseTx->GetHash().GetHex());
                    continue;
                }
            } catch (std::exception &e) {
                LogPrint("ERROR", "CreateNewBlockStableCoinRelease() : unexpected exception: %s\n", e.what());
                continue;
            }

            spCW->Flush();

            auto fuel        = pBaseTx->GetFuel(height, fuelRate);
            auto fees_symbol = std::get<0>(pBaseTx->GetFees());
            auto fees        = std::get<1>(pBaseTx->GetFees());
            assert(fees_symbol == SYMB::WICC);

            totalBlockSize += txSize;
            totalRunStep += pBaseTx->nRunStep;
            totalFuel += fuel;
            totalFees += fees;
            assert(fees >= fuel);
            reward += (fees - fuel);

            ++index;

            pBlock->vptx.push_back(itor->baseTx);

            LogPrint("fuel", "miner total fuel fee:%d, tx fuel fee:%d, fuel:%d, fuelRate:%d, txid:%s\n", totalFuel,
                     pBaseTx->GetFuel(height, fuelRate), pBaseTx->nRunStep, fuelRate, pBaseTx->GetHash().GetHex());
        }

        nLastBlockTx                   = index + 1;
        nLastBlockSize                 = totalBlockSize;
        miningBlockInfo.txCount        = index + 1;
        miningBlockInfo.totalBlockSize = totalBlockSize;
        miningBlockInfo.totalFees      = totalFees;

        ((CBlockRewardTx *)pBlock->vptx[0].get())->reward_fees = reward;

        // Fill in header
        pBlock->SetPrevBlockHash(pIndexPrev->GetBlockHash());
        pBlock->SetNonce(0);
        pBlock->SetHeight(height);
        pBlock->SetFuel(totalFuel);
        pBlock->SetFuelRate(fuelRate);
        UpdateTime(*pBlock, pIndexPrev);

        LogPrint("INFO", "CreateNewBlock() : height=%d, tx=%d, totalBlockSize=%llu\n", height, index + 1,
                 totalBlockSize);
    }

    return pBlock;
}

bool CheckWork(CBlock *pBlock, CWallet &wallet) {
    // Print block information
    pBlock->Print(*pCdMan->pBlockCache);

    // Found a solution
    {
        LOCK(cs_main);
        if (pBlock->GetPrevBlockHash() != chainActive.Tip()->GetBlockHash())
            return ERRORMSG("CheckWork() : generated block is stale");

        // Process this block the same as if we received it from another node
        CValidationState state;
        if (!ProcessBlock(state, nullptr, pBlock))
            return ERRORMSG("CheckWork() : failed to process block");
    }

    return true;
}

bool static MineBlock(CBlock *pBlock, CWallet *pWallet, CBlockIndex *pIndexPrev, uint32_t txUpdated,
                      CCacheWrapper &cw) {
    int64_t nStart = GetTime();

    while (true) {
        boost::this_thread::interruption_point();

        // Should not mine new blocks if the miner does not connect to other nodes except running
        // in regtest network.
        if (vNodes.empty() && SysCfg().NetworkID() != REGTEST_NET)
            return false;

        if (pIndexPrev != chainActive.Tip())
            return false;

        // Take a sleep and check.
        [&]() {
            int64_t whenCanIStart = pIndexPrev->GetBlockTime() + GetBlockInterval(chainActive.Height() + 1);
            while (GetTime() < whenCanIStart) {
                ::MilliSleep(100);
            }
        }();

        bool success = false;
        int64_t lastTime;

        vector<CRegID> delegateList;
        {
            LOCK(cs_main);

            if (!pCdMan->pDelegateCache->GetTopDelegateList(delegateList)) {
                LogPrint("MINER", "MineBlock() : failed to get top delegates\n");
                return false;
            }
        }

        uint16_t index = 0;
        for (auto &delegate : delegateList)
            LogPrint("shuffle", "before shuffle: index=%d, regId=%s\n", index++, delegate.ToString());

        ShuffleDelegates(pBlock->GetHeight(), delegateList);

        index = 0;
        for (auto &delegate : delegateList)
            LogPrint("shuffle", "after shuffle: index=%d, regId=%s\n", index++, delegate.ToString());

        int64_t currentTime = GetTime();

        CRegID regId;
        GetCurrentDelegate(currentTime, pBlock->GetHeight(), delegateList, regId);

        CAccount minerAcct;
        {
            LOCK(cs_main);

            if (!cw.accountCache.GetAccount(regId, minerAcct)) {
                LogPrint("MINER", "MineBlock() : failed to get miner's account: %s\n", regId.ToString());
                return false;
            }
        }

        if (uint32_t(chainActive.Height() + 1) != pBlock->GetHeight())
            return false;

        {
            LOCK2(cs_main, pWalletMain->cs_wallet);
            CKey acctKey;
            if (pWalletMain->GetKey(minerAcct.keyid.ToAddress(), acctKey, true) ||
                pWalletMain->GetKey(minerAcct.keyid.ToAddress(), acctKey)) {
                lastTime   = GetTimeMillis();
                mining     = true;
                minerKeyId = minerAcct.keyid;
                success    = CreateBlockRewardTx(currentTime, minerAcct, cw.accountCache, pBlock);
                LogPrint("MINER", "MineBlock() : %s to create block reward transaction, used %d ms, miner address %s\n",
                        success ? "succeed" : "failed", GetTimeMillis() - lastTime, minerAcct.keyid.ToAddress());
            } else {
                mining = false;
            }
        }

        if (success) {
            SetThreadPriority(THREAD_PRIORITY_NORMAL);

            lastTime = GetTimeMillis();
            success  = CheckWork(pBlock, *pWallet);
            LogPrint("MINER", "MineBlock() : %s to check work, used %s ms\n", success ? "succeed" : "failed",
                     GetTimeMillis() - lastTime);

            SetThreadPriority(THREAD_PRIORITY_LOWEST);

            miningBlockInfo.time          = pBlock->GetBlockTime();
            miningBlockInfo.nonce         = pBlock->GetNonce();
            miningBlockInfo.height        = pBlock->GetHeight();
            miningBlockInfo.totalFuel     = pBlock->GetFuel();
            miningBlockInfo.fuelRate      = pBlock->GetFuelRate();
            miningBlockInfo.hash          = pBlock->GetHash();
            miningBlockInfo.hashPrevBlock = pBlock->GetHash();

            {
                LOCK(csMinedBlocks);
                minedBlocks.push_front(miningBlockInfo);
            }

            return true;
        }

        if (mempool.GetUpdatedTransactionNum() != txUpdated || GetTime() - nStart > 60)
            return false;
    }

    return false;
}

void static CoinMiner(CWallet *pWallet, int32_t targetHeight) {
    LogPrint("INFO", "CoinMiner() : started\n");

    SetThreadPriority(THREAD_PRIORITY_LOWEST);
    RenameThread("Coin-miner");

    auto HaveMinerKey = [&]() {
        LOCK2(cs_main, pWalletMain->cs_wallet);

        set<CKeyID> setMineKey;
        setMineKey.clear();
        pWalletMain->GetKeys(setMineKey, true);
        return !setMineKey.empty();
    };

    if (!HaveMinerKey()) {
        LogPrint("ERROR", "CoinMiner() : terminated for lack of miner key\n");
        return;
    }

    auto GetCurrHeight = [&]() {
        LOCK(cs_main);
        return chainActive.Height();
    };

    targetHeight += GetCurrHeight();

    try {
        SetMinerStatus(true);

        while (true) {
            boost::this_thread::interruption_point();

            if (SysCfg().NetworkID() != REGTEST_NET) {
                // Busy-wait for the network to come online so we don't waste time mining
                // on an obsolete chain. In regtest mode we expect to fly solo.
                while (vNodes.empty() || (chainActive.Tip() && chainActive.Height() > 1 &&
                                          GetAdjustedTime() - chainActive.Tip()->nTime > 60 * 60 &&
                                          !SysCfg().GetBoolArg("-genblockforce", false))) {
                    MilliSleep(1000);
                }
            }

            miningBlockInfo.SetNull();  // TODO: remove

            //
            // Create new block
            //
            int64_t lastTime        = GetTimeMillis();
            uint32_t txUpdated      = mempool.GetUpdatedTransactionNum();
            CBlockIndex *pIndexPrev = chainActive.Tip();

            auto spCW   = std::make_shared<CCacheWrapper>(pCdMan);
            auto pBlock = CreateNewBlock(*spCW);

            if (!pBlock.get())
                throw runtime_error("CoinMiner() : failed to create new block");

            LogPrint("MINER", "CoinMiner() : succeeded in adding a new block, contain %s transactions, used %s ms\n",
                    pBlock->vptx.size(), GetTimeMillis() - lastTime);

            MineBlock(pBlock.get(), pWallet, pIndexPrev, txUpdated, *spCW);

            if (SysCfg().NetworkID() != MAIN_NET && targetHeight <= GetCurrHeight())
                throw boost::thread_interrupted();
        }
    } catch (...) {
        LogPrint("INFO", "CoinMiner() : terminated\n");
        SetMinerStatus(false);
        throw;
    }
}

void GenerateCoinBlock(bool fGenerate, CWallet *pWallet, int32_t targetHeight) {
    static boost::thread_group *minerThreads = nullptr;

    if (minerThreads != nullptr) {
        minerThreads->interrupt_all();
        delete minerThreads;
        minerThreads = nullptr;
    }

    if (!fGenerate)
        return;

    // In mainnet, coin miner should generate blocks continuously regardless of target height.
    if (SysCfg().NetworkID() != MAIN_NET && targetHeight <= 0) {
        LogPrint("ERROR", "GenerateCoinBlock() : target height <=0 (%d)", targetHeight);
        return;
    }

    minerThreads = new boost::thread_group();
    minerThreads->create_thread(boost::bind(&CoinMiner, pWallet, targetHeight));
}

void MinedBlockInfo::SetNull() {
    time           = 0;
    nonce          = 0;
    height         = 0;
    totalFuel      = 0;
    fuelRate       = 0;
    totalFees      = 0;
    txCount        = 0;
    totalBlockSize = 0;
    hash.SetNull();
    hashPrevBlock.SetNull();
}

vector<MinedBlockInfo> GetMinedBlocks(uint32_t count) {
    std::vector<MinedBlockInfo> ret;
    LOCK(csMinedBlocks);
    count = std::min((uint32_t)minedBlocks.size(), count);
    for (uint32_t i = 0; i < count; i++) {
        ret.push_back(minedBlocks[i]);
    }

    return ret;
}
