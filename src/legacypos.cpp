// Copyright (c) 2019-2020 barrystyle/xpchain team
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <legacypos.h>

#include <amount.h>
#include <chainparams.h>
#include <outputtype.h>
#include <util.h>
#include <serialize.h>
#include <streams.h>
#include <validation.h>

//! legacy nsubsidy routines from xpg-wg, so we can
//! actually calculate sane stake rewards just before fork

double_t GetAnnualRateLegacy(int nHeight)
{
    int nSubsidyReducingInterval = 60 * 24 * 365;
    if(nHeight <= nSubsidyReducingInterval)
    {
        return 0.10;
    }
    else if(nSubsidyReducingInterval < nHeight && nHeight <= nSubsidyReducingInterval * 2)
    {
        return 0.09;
    }
    else if(nSubsidyReducingInterval * 2 < nHeight && nHeight <= nSubsidyReducingInterval * 3)
    {
        return 0.08;
    }
    else if(nSubsidyReducingInterval * 3 < nHeight && nHeight <= nSubsidyReducingInterval * 4)
    {
        return 0.07;
    }
    else if(nSubsidyReducingInterval * 4 < nHeight && nHeight <= nSubsidyReducingInterval * 5)
    {
        return 0.06;
    }
    else if(nSubsidyReducingInterval * 5 < nHeight)
    {
        return 0.05;
    }
}

CAmount GetProofOfStakeRewardLegacy(int nHeight, CAmount nAmount, uint32_t nTime)
{
    const int64_t nStakeMinAgeLegacy = 60 * 60 * 24 * 3;
    const int64_t nStakeMaxAgeLegacy = 60 * 60 * 24 * 60;

    double_t dRewardCurveMaximum = 1.02500000;
    double_t dRewardCurveLimit = 1.00000000;
    double_t dRewardCurveBase = 0.01800000;
    double_t dRewardCurveSteepness = 0.00000285;

    if(nTime < nStakeMinAgeLegacy)
    {
        return 0;
    }

    nTime = std::min(nTime, (uint32_t)nStakeMaxAgeLegacy);

    CAmount annual = nAmount * GetAnnualRateLegacy(nHeight);

    double_t coefficient = dRewardCurveMaximum / (1.0 + (dRewardCurveMaximum / dRewardCurveBase - 1.0) * exp(-dRewardCurveSteepness * nTime));
    coefficient = std::min(coefficient, dRewardCurveLimit);

    return (CAmount) (annual * coefficient * nTime / (365 * 24 * 60 * 60));
}

//! legacy stake validation routines from xpg-wg, so we can
//! actually calculate sane stake rewards just before fork

bool IsDestinationSame(const CScript& a, const CScript& b)
{
    txnouttype aType, bType;
    std::vector <std::vector<unsigned char>> aSol, bSol;

    if (!Solver(a, aType, aSol) || !Solver(b, bType, bSol)) {
        return false;
    }

    if (aSol.size() != 1 || bSol.size() != 1) {
        return false;
    }

    CTxDestination aDest,bDest;
    if(!ExtractDestination(a, aDest) || !ExtractDestination(b, bDest)) {
        return false;
    }

    if (!(aDest == bDest)) {
        return false;
    }

    return true;
}

bool IsCoinStakeTx(CTransactionRef tx, const Consensus::Params &consensusParams, uint256 &hashBlock, CTransactionRef& prevTx)
{
    if (tx->vin.size() != 1) {
        return error("%s: coinstake has too many inputs", __func__);
    }
    if (tx->vout.size() != 1) {
        return error("%s: coinstake has too many outputs", __func__);
    }

    if (!GetTransaction(tx->vin[0].prevout.hash, prevTx, consensusParams, hashBlock, true)) {
        return error("%s: unknown coinstake input", __func__);
    }

    if (prevTx->GetHash() != tx->vin[0].prevout.hash) {
        return error("%s: invalid coinstake input hash", __func__);
    }

    if (!IsDestinationSame(prevTx->vout[tx->vin[0].prevout.n].scriptPubKey, tx->vout[0].scriptPubKey)) {
        return error("%s: invalid coinstake output", __func__);
    }

    return true;
}

uint256 GetRewardHash(const std::vector<std::pair<CScript, CAmount>>& vReward, CTransactionRef txCoinStake, uint32_t nTime)
{
    CDataStream ss(SER_GETHASH, 0);
    for(std::pair<CScript, CAmount> p:vReward)
    {
        ss << p.first << p.second;
    }
    ss << nTime << txCoinStake->vin[0];
    return Hash(ss.begin(), ss.end());
}

bool EqualDestination(CTransactionRef txCoinStake, CPubKey pubkey)
{
    //address of txcoinstake output == address from pubkey
    std::vector<std::vector<unsigned char>> vSolutions;
    txnouttype whichType;
    if (!Solver(txCoinStake->vout[0].scriptPubKey, whichType, vSolutions))
        return false;

    if(whichType == TX_SCRIPTHASH)
    {
        //p2sh(p2wpkh)
        CTxDestination dest = GetDestinationForKey(pubkey, OutputType::P2SH_SEGWIT);
        if(auto scriptID = boost::get<CScriptID>(&dest))
        {
            return *scriptID == CScriptID(uint160(vSolutions[0]));
        }
    }
    else if(whichType == TX_PUBKEYHASH || whichType == TX_WITNESS_V0_KEYHASH)
    {
        //p2wpkh p2pkh
        return CKeyID(uint160(vSolutions[0])) == pubkey.GetID();
    }
    return false;
}

bool VerifyCoinBaseTx(const CBlock& block, CValidationState& state)
{
    if (block.vtx.size() < 1) {
        return false;
    }

    CScript sig = block.vtx[0]->vout[0].scriptPubKey;
    if (sig.size() < 1) {
        return false;
    }

    if (sig[0] != OP_RETURN) {
        return false;
    }

    CScriptBase::const_iterator ptr = sig.begin() + 1;
    opcodetype sizeOP;
    std::vector<unsigned char> vchSize;
    if (!GetScriptOp(ptr, sig.end(), sizeOP, &vchSize)) {
        return false;
    }

    if (vchSize.size() > 4) {
        return false;
    }

    CScriptNum nSize(vchSize, false);
    int size = nSize.getint();
    opcodetype sigOP;
    std::vector<unsigned char> vchSig;
    if (!GetScriptOp(ptr, sig.end(), sigOP, &vchSig)) {
        return false;
    }

    opcodetype pubkeyOP;
    std::vector<unsigned char> vchPubKey;
    if (!GetScriptOp(ptr, sig.end(), pubkeyOP, &vchPubKey)) {
        return false;
    }

    CPubKey pubkey(vchPubKey.begin(), vchPubKey.end());
    if (!pubkey.IsFullyValid()) {
        return false;
    }
    if (size != block.vtx[0]->vout.size() - 2) {
        return false;
    }
    if (block.vtx[0]->vout[0].nValue != 0) {
        return false;
    }
    if (block.vtx[0]->vout.back().nValue != 0) {
        return false;
    }

    //make rewardvalues and hash
    std::vector <std::pair<CScript, CAmount>> rewardValues;
    rewardValues.clear();
    rewardValues.resize(size);
    //printf("vout size =  %d\n",block.vtx[0]->vout.size());
    for (size_t i = 1; i <= size; i++) {
        rewardValues[i - 1].first = block.vtx[0]->vout[i].scriptPubKey;
        rewardValues[i - 1].second = block.vtx[0]->vout[i].nValue;
    }

    //address from coinstakeTX output == address from pubkey
    if (!EqualDestination(block.vtx[1], pubkey)) {
        return false;
    }

    uint256 hash = GetRewardHash(rewardValues, block.vtx[1], block.nTime);
    if (pubkey.Verify(hash, vchSig)) {
        return true;
    }
    return false;
}

//! and finally an exception table for the twin-input bug
//! seen in bitcoin 0.17; patched against this, but what remains, remains..

bool knownAbuseOfInputBug(const uint256 checkHash)
{
    if (checkHash == uint256S("f7ed64886878598d8ddbb587017a6324a670331eecaa0f716b71442ef63dc427") ||
        checkHash == uint256S("bfe31e8a75dda9accbb3307af8a293be1f029e9b91e3c2d339dcbde53cda96e5") ||
        checkHash == uint256S("cee964493303b36c755e72b5c04070943b0708c3af50aa00e22255658355a9e4") ||
        checkHash == uint256S("0ad33e81b865fea076f31ea138ceb7480f16e447cc7e45b1b1d9d301fde80706") ||
        checkHash == uint256S("86f5b30e2e17478fa93fed5994f9ec99cf00874a662b5ad8f0242e178efae112") ||
        checkHash == uint256S("e7b528a44ce78cf31d22b6177ee31d1482303462a4264a7f7df49e64b311b8d0"))
        return true;
    return false;
} 

