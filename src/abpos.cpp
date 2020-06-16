// Copyright (c) 2019-2020 barrystyle/xpchain team
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <amount.h>
#include <chainparams.h>
#include <util.h>
#include <validation.h>

//! return if we're in mainnet or not
bool isMainnet()
{
    return Params().NetworkIDString() == "main";
}

//! return age of a given input
int GetLastHeight(uint256 txHash)
{
    uint256 hashBlock;
    CTransactionRef stakeInput;
    const Consensus::Params& consensusParams = Params().GetConsensus();
    if (!GetTransaction(txHash, stakeInput, consensusParams, hashBlock))
        return 0;

    if (hashBlock == uint256())
        return 0;

    return mapBlockIndex[hashBlock]->nHeight;
}

//! return which year 'phase' we are currently in with abpos
void GetPoSPhase(int nHeight, int& nPhase, int& nBlocks)
{
    nPhase = 0, nBlocks = 0;
    const int nYearBlocks = isMainnet() ? 525600 : 1024;
    const int nStartHeight = isMainnet() ? Params().GetConsensus().abposHeight : 128;

    if (nHeight - nStartHeight <= 0)
        return;

    //! subtract recursively to determine year
    int nPhaseStep = nHeight - nStartHeight;
    while (nPhaseStep > nYearBlocks) {
        nPhase++;
        nPhaseStep -= nYearBlocks;
    }
    nBlocks = nPhaseStep;
}

//! return interest rate per input in given PoS phase
int GetPoSInterest(int nHeight, int nCoinAge)
{
    int nPhase = 0, nBlocks = 0;
    std::vector<unsigned int> stakeParameters;

    //! retrieve appropriate params
    GetPoSPhase(nHeight, nPhase, nBlocks);
    switch (nPhase) {
        case 0: return 0;
        case 1: stakeParameters = { 60, 240, 2920 }; //! 0.6% - 2.4%, 2920 = 0.1%
        case 2: stakeParameters = { 50, 200, 3504 }; //! 0.5% - 2.0%, 3504 = 0.1%
        case 3: stakeParameters = { 40, 160, 4380 }; //! 0.4% - 1.6%, 4380 = 0.1%
        case 4: stakeParameters = { 30, 120, 5840 }; //! 0.3% - 1.2%, 5840 = 0.1%
        case 5: stakeParameters = { 15, 60, 11580 }; //! 0.15% - 0.60%, 11580 = 0.1%
        default: return 0;

    }

    //! recursively calculate interest window
    int nBaseInterest = stakeParameters.at(0);
    int nTempCalc = nBlocks - nCoinAge;
    if (nTempCalc < 0)
        nTempCalc = 0;
    while (nTempCalc > 0) {
        nBaseInterest++;
        nTempCalc -= stakeParameters.at(2);
    }
    assert(nBaseInterest < stakeParameters.at(1));

    return nBaseInterest;
}

//! calculate the stake reward, given an input coin amount and its age
CAmount GetStakeReward(CAmount coinAmount, int nCoinAge)
{
    //! we do not calculate on sub-decimal amounts
    CAmount calculatedAmount = round(coinAmount * COIN) / COIN;
    if (calculatedAmount < 1)
        return 0;

    const int nHeight = chainActive.Height();
    const int nInterestRate = GetPoSInterest(nHeight, nCoinAge);
    CAmount nReward = (coinAmount * nInterestRate) / 10000;
    return nReward;
}

