#include <consensus/consensus.h>
#include <interfaces/wallet.h>
#include <key_io.h>
#include <timedata.h>
#include <validation.h>

#include <chain.h>
#include <chainparams.h>
#include <stdint.h>
#include <math.h>

#include <kernelrecord.h>

#include <base58.h>

using namespace std;

bool KernelRecord::showTransaction()
{
    return true;
}

/*
 * Decompose CWallet transaction to model kernel records.
 */
vector<KernelRecord> KernelRecord::decomposeOutput(const interfaces::WalletTx& wtx)
{
    uint256 hash = wtx.tx->GetHash();
    const std::vector<CTxOut> outs = wtx.tx->vout;
    std::vector<isminetype> isMine = wtx.txout_is_mine;
    vector<KernelRecord> parts;
    int64_t nTime = wtx.time;

    for(uint32_t n = 0; n < outs.size(); n++){
        if(isMine[n] == isminetype::ISMINE_SPENDABLE){
            int64_t nValue = outs[n].nValue;
            std::string addrStr = EncodeDestination(wtx.txout_address[n]);
            parts.emplace_back(hash, n, nTime, addrStr, nValue);
        }
    }
    return parts;
}

std::string KernelRecord::getTxID()
{
    return hash.ToString();
}

uint32_t KernelRecord::getTxOutIndex()
{
    return n;
}

int64_t KernelRecord::getAge() const
{
    return (GetAdjustedTime() - nTime) / 86400;
}

uint64_t KernelRecord::getCoinDay() const
{
    return 0;
}

int64_t KernelRecord::getPoSReward(int minutes)
{
    return 0;
}

double KernelRecord::getProbToMintStake(double difficulty, int timeOffset) const
{
    int64_t Weight = (min((GetAdjustedTime() - nTime) + timeOffset, (int64_t)(Params().GetConsensus().nStakeMinAge+Params().GetConsensus().nStakeMaxAge)) - Params().GetConsensus().nStakeMinAge);
    uint64_t coinAge = max(nValue * Weight / (COIN * 86400), (int64_t)0);
    double probability = coinAge / (pow(static_cast<double>(2),32) * difficulty);
    return probability > 1 ? 1 : probability;
}

double KernelRecord::getProbToMintWithinNMinutes(double difficulty, int minutes)
{
    if(difficulty != prevDifficulty || minutes != prevMinutes)
    {
        double prob = 1;
        double p;
        int d = minutes / (60 * 24); // Number of full days
        int m = minutes % (60 * 24); // Number of minutes in the last day
        int i, timeOffset;

        // Probabilities for the first d days
        for(i = 0; i < d; i++)
        {
            timeOffset = i * 86400;
            p = pow(1 - getProbToMintStake(difficulty, timeOffset), 86400);
            prob *= p;
        }

        // Probability for the m minutes of the last day
        timeOffset = d * 86400;
        p = pow(1 - getProbToMintStake(difficulty, timeOffset), 60 * m);
        prob *= p;

        prob = 1 - prob;
        prevProbability = prob;
        prevDifficulty = difficulty;
        prevMinutes = minutes;
    }
    return prevProbability;
}
