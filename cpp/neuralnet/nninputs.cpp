#include "../neuralnet/nninputs.h"

using namespace std;

int NNPos::xyToPos(int x, int y, int nnXLen) {
  return y * nnXLen + x;
}
int NNPos::locToPos(Loc loc, int boardXSize, int nnXLen, int nnYLen) {
  if(loc == Board::PASS_LOC)
    return nnXLen * nnYLen;
  else if(loc == Board::NULL_LOC)
    return nnXLen * (nnYLen + 1);
  return Location::getY(loc,boardXSize) * nnXLen + Location::getX(loc,boardXSize);
}
int NNPos::locToDoublePos(Loc fromLoc, Loc toLoc, int boardXSize, int nnXLen, int nnYLen) {
  int from = Location::getY(fromLoc,boardXSize) * nnXLen + Location::getX(fromLoc,boardXSize);
  int to = Location::getY(toLoc,boardXSize) * nnXLen + Location::getX(toLoc,boardXSize);
  return from*nnXLen*nnXLen+to;
}
Loc NNPos::posToLoc(int pos, int boardXSize, int boardYSize, int nnXLen, int nnYLen) {
  if(pos == nnXLen * nnYLen)
    return Board::PASS_LOC;
  int x = pos % nnXLen;
  int y = pos / nnXLen;
  if(x < 0 || x >= boardXSize || y < 0 || y >= boardYSize)
    return Board::NULL_LOC;
  return Location::getLoc(x,y,boardXSize);
}
Loc NNPos::posToFromLoc(int pos, int boardXSize, int boardYSize, int nnXLen, int nnYLen) {
  pos = pos/(nnXLen*nnYLen);
  int x = pos % nnXLen;
  int y = pos / nnXLen;
  if(x < 0 || x >= boardXSize || y < 0 || y >= boardYSize)
    return Board::NULL_LOC;
  return Location::getLoc(x,y,boardXSize);
}
Loc NNPos::posToToLoc(int pos, int boardXSize, int boardYSize, int nnXLen, int nnYLen) {
  pos = pos%(nnXLen*nnYLen);
  int x = pos % nnXLen;
  int y = pos / nnXLen;
  if(x < 0 || x >= boardXSize || y < 0 || y >= boardYSize)
    return Board::NULL_LOC;
  return Location::getLoc(x,y,boardXSize);
}

bool NNPos::isPassPos(int pos, int nnXLen, int nnYLen) {
  return false;
}

int NNPos::getPolicySize(int nnXLen, int nnYLen) {
  return nnXLen * nnYLen * nnXLen * nnYLen;
}

//-----------------------------------------------------------------------------------------------------------
//-----------------------------------------------------------------------------------------------------------

const Hash128 MiscNNInputParams::ZOBRIST_CONSERVATIVE_PASS =
  Hash128(0x0c2b96f4b8ae2da9ULL, 0x5a14dee208fec0edULL);
const Hash128 MiscNNInputParams::ZOBRIST_PLAYOUT_DOUBLINGS =
  Hash128(0xa5e6114d380bfc1dULL, 0x4160557f1222f4adULL);
const Hash128 MiscNNInputParams::ZOBRIST_NN_POLICY_TEMP =
  Hash128(0xebcbdfeec6f4334bULL, 0xb85e43ee243b5ad2ULL);
const Hash128 MiscNNInputParams::ZOBRIST_AVOID_MYTDAGGER_HACK =
  Hash128(0x612d22ec402ce054ULL, 0x0db915c49de527aeULL);

//-----------------------------------------------------------------------------------------------------------
//-----------------------------------------------------------------------------------------------------------

double ScoreValue::whiteWinsOfWinner(Player winner, double drawEquivalentWinsForWhite) {
  if(winner == P_WHITE)
    return 1.0;
  else if(winner == P_BLACK)
    return 0.0;

  assert(winner == C_EMPTY);
  return drawEquivalentWinsForWhite;
}

static const double twoOverPi = 0.63661977236758134308;
static const double piOverTwo = 1.57079632679489661923;

double ScoreValue::whiteScoreDrawAdjust(double finalWhiteMinusBlackScore, double drawEquivalentWinsForWhite, const BoardHistory& hist) {
  return finalWhiteMinusBlackScore;
}

double ScoreValue::whiteScoreValueOfScoreSmooth(double finalWhiteMinusBlackScore, double center, double scale, double drawEquivalentWinsForWhite, const Board& b, const BoardHistory& hist) {
  double adjustedScore = finalWhiteMinusBlackScore - center;
  if(b.x_size == b.y_size)
    return atan(adjustedScore / (scale*b.x_size)) * twoOverPi;
  else
    return atan(adjustedScore / (scale*sqrt(b.x_size*b.y_size))) * twoOverPi;
}

double ScoreValue::whiteScoreValueOfScoreSmoothNoDrawAdjust(double finalWhiteMinusBlackScore, double center, double scale, const Board& b) {
  double adjustedScore = finalWhiteMinusBlackScore - center;
  if(b.x_size == b.y_size)
    return atan(adjustedScore / (scale*b.x_size)) * twoOverPi;
  else
    return atan(adjustedScore / (scale*sqrt(b.x_size*b.y_size))) * twoOverPi;
}

static double inverse_atan(double x) {
  if(x >= piOverTwo - 1e-6) return 1e6;
  if(x <= -piOverTwo + 1e-6) return -1e6;
  return tan(x);
}

double ScoreValue::approxWhiteScoreOfScoreValueSmooth(double scoreValue, double center, double scale, const Board& b) {
  assert(scoreValue >= -1 && scoreValue <= 1);
  double scoreUnscaled = inverse_atan(scoreValue*piOverTwo);
  if(b.x_size == b.y_size)
    return scoreUnscaled * (scale*b.x_size) + center;
  else
    return scoreUnscaled * (scale*sqrt(b.x_size*b.y_size)) + center;
}

double ScoreValue::whiteScoreMeanSqOfScoreGridded(double finalWhiteMinusBlackScore, double drawEquivalentWinsForWhite) {
  assert((int)(finalWhiteMinusBlackScore * 2) == finalWhiteMinusBlackScore * 2);
  bool finalScoreIsInteger = ((int)finalWhiteMinusBlackScore == finalWhiteMinusBlackScore);
  if(!finalScoreIsInteger)
    return finalWhiteMinusBlackScore * finalWhiteMinusBlackScore;

  double lower = finalWhiteMinusBlackScore - 0.5;
  double upper = finalWhiteMinusBlackScore + 0.5;
  double lowerSq = lower * lower;
  double upperSq = upper * upper;

  return lowerSq + (upperSq - lowerSq) * drawEquivalentWinsForWhite;
}


static bool scoreValueTablesInitialized = false;
static double* expectedSVTable = NULL;
static const int svTableAssumedBSize = NNPos::MAX_BOARD_LEN;
static const int svTableMeanRadius = svTableAssumedBSize*svTableAssumedBSize + NNPos::EXTRA_SCORE_DISTR_RADIUS;
static const int svTableMeanLen = svTableMeanRadius*2;
static const int svTableStdevLen = svTableAssumedBSize*svTableAssumedBSize + NNPos::EXTRA_SCORE_DISTR_RADIUS;

void ScoreValue::freeTables() {
  if(scoreValueTablesInitialized) {
    delete[] expectedSVTable;
    expectedSVTable = NULL;
    scoreValueTablesInitialized = false;
  }
}

void ScoreValue::initTables() {
  assert(!scoreValueTablesInitialized);
  expectedSVTable = new double[svTableMeanLen*svTableStdevLen];

  //Precompute normal PDF
  const int stepsPerUnit = 10; //Must be divisible by 2. This is both the number of segments that we divide points into, and that we divide stdevs into
  const int boundStdevs = 5;
  int minStdevSteps = -boundStdevs*stepsPerUnit;
  int maxStdevSteps = boundStdevs*stepsPerUnit;
  double* normalPDF = new double[(maxStdevSteps-minStdevSteps)+1];
  for(int i = minStdevSteps; i <= maxStdevSteps; i++) {
    double xInStdevs = (double)i / stepsPerUnit;
    double w = exp(-0.5 * xInStdevs * xInStdevs);
    normalPDF[i-minStdevSteps] = w;
  }
  //Precompute scorevalue at increments of 1/stepsPerUnit points
  Board board(svTableAssumedBSize,svTableAssumedBSize);
  int minSVSteps = - (svTableMeanRadius*stepsPerUnit + stepsPerUnit/2 + boundStdevs * svTableStdevLen * stepsPerUnit);
  int maxSVSteps = -minSVSteps;
  double* svPrecomp = new double[(maxSVSteps-minSVSteps)+1];
  for(int i = minSVSteps; i <= maxSVSteps; i++) {
    double mean = (double)i / stepsPerUnit;
    double sv = whiteScoreValueOfScoreSmoothNoDrawAdjust(mean, 0.0, 1.0, board);
    svPrecomp[i-minSVSteps] = sv;
  }

  //Perform numeric integration
  for(int meanIdx = 0; meanIdx < svTableMeanLen; meanIdx++) {
    int meanSteps = (meanIdx - svTableMeanRadius) * stepsPerUnit - stepsPerUnit/2;
    for(int stdevIdx = 0; stdevIdx < svTableStdevLen; stdevIdx++) {
      double wSum = 0.0;
      double wsvSum = 0.0;
      for(int i = minStdevSteps; i <= maxStdevSteps; i++) {
        int xSteps = meanSteps + stdevIdx * i;
        double w = normalPDF[i-minStdevSteps];
        assert(xSteps >= minSVSteps && xSteps <= maxSVSteps);
        double sv = svPrecomp[xSteps-minSVSteps];
        wSum += w;
        wsvSum += w*sv;
      }
      expectedSVTable[meanIdx*svTableStdevLen + stdevIdx] = wsvSum / wSum;
    }
  }

  delete[] normalPDF;
  delete[] svPrecomp;
  scoreValueTablesInitialized = true;
}

double ScoreValue::expectedWhiteScoreValue(double whiteScoreMean, double whiteScoreStdev, double center, double scale, const Board& b) {
  assert(scoreValueTablesInitialized);

  double scaleFactor;
  if(b.x_size == b.y_size)
    scaleFactor = (double)svTableAssumedBSize / (scale * b.x_size);
  else
    scaleFactor = (double)svTableAssumedBSize / (scale * sqrt(b.x_size*b.y_size));

  double meanScaled = (whiteScoreMean - center) * scaleFactor;
  double stdevScaled = whiteScoreStdev * scaleFactor;

  double meanRounded = round(meanScaled);
  double stdevFloored = floor(stdevScaled);
  int meanIdx0 = (int)meanRounded + svTableMeanRadius;
  int stdevIdx0 = (int)stdevFloored;
  int meanIdx1 = meanIdx0+1;
  int stdevIdx1 = stdevIdx0+1;

  if(meanIdx0 < 0) { meanIdx0 = 0; meanIdx1 = 0; }
  if(meanIdx1 >= svTableMeanLen) { meanIdx0 = svTableMeanLen-1; meanIdx1 = svTableMeanLen-1; }
  assert(stdevIdx0 >= 0);
  if(stdevIdx1 >= svTableStdevLen) { stdevIdx0 = svTableStdevLen-1; stdevIdx1 = svTableStdevLen-1; }

  double lambdaMean = meanScaled - meanRounded + 0.5;
  double lambdaStdev = stdevScaled - stdevFloored;

  double a00 = expectedSVTable[meanIdx0*svTableStdevLen + stdevIdx0];
  double a01 = expectedSVTable[meanIdx0*svTableStdevLen + stdevIdx1];
  double a10 = expectedSVTable[meanIdx1*svTableStdevLen + stdevIdx0];
  double a11 = expectedSVTable[meanIdx1*svTableStdevLen + stdevIdx1];

  double b0 = a00 + lambdaStdev*(a01-a00);
  double b1 = a10 + lambdaStdev*(a11-a10);
  return b0 + lambdaMean*(b1-b0);
}

//-----------------------------------------------------------------------------------------------------------
//-----------------------------------------------------------------------------------------------------------

void NNInputs::fillScoring(
  const Board& board,
  const Color* area,
  bool groupTax,
  float* scoring
) {
  if(!groupTax) {
    std::fill(scoring, scoring + Board::MAX_ARR_SIZE, 0.0f);
    for(int y = 0; y<board.y_size; y++) {
      for(int x = 0; x<board.x_size; x++) {
        Loc loc = Location::getLoc(x,y,board.x_size);
        Color areaColor = area[loc];
        if(areaColor == P_BLACK)
          scoring[loc] = -1.0f;
        else if(areaColor == P_WHITE)
          scoring[loc] = 1.0f;
        else {
          assert(areaColor == C_EMPTY);
          scoring[loc] = 0;
        }
      }
    }
  }
  else {
    bool visited[Board::MAX_ARR_SIZE];
    Loc queue[Board::MAX_ARR_SIZE];

    std::fill(visited, visited + Board::MAX_ARR_SIZE, false);
    std::fill(scoring, scoring + Board::MAX_ARR_SIZE, 0.0f);
    for(int y = 0; y<board.y_size; y++) {
      for(int x = 0; x<board.x_size; x++) {
        Loc loc = Location::getLoc(x,y,board.x_size);
        if(visited[loc])
          continue;
        Color areaColor = area[loc];
        if(areaColor == P_BLACK || areaColor == P_WHITE) {
          float fullValue = areaColor == P_WHITE ? 1.0f : -1.0f;
          int queueHead = 0;
          int queueTail = 1;
          queue[0] = loc;
          visited[loc] = true;

          //First, count how many empty or opp locations there are
          int territoryCount = 0;
          while(queueHead < queueTail) {
            Loc next = queue[queueHead];
            queueHead++;
            if(board.colors[next] != areaColor)
              territoryCount++;
            //Push adjacent locations on to queue
            for(int i = 0; i<4; i++) {
              Loc adj = next + board.adj_offsets[i];
              if(area[adj] == areaColor && !visited[adj]) {
                queue[queueTail] = adj;
                queueTail++;
                visited[adj] = true;
              }
            }
          }

          //Then, actually fill values
          float territoryValue = territoryCount <= 2 ? 0.0f : fullValue * (territoryCount - 2.0f) / territoryCount;
          for(int j = 0; j<queueTail; j++) {
            Loc next = queue[j];
            queueHead++;
            if(board.colors[next] != areaColor)
              scoring[next] = territoryValue;
            else
              scoring[next] = fullValue;
          }
        }
        else {
          assert(areaColor == C_EMPTY);
          scoring[loc] = 0;
        }
      }
    }
  }
}


//-----------------------------------------------------------------------------------------------------------
//-----------------------------------------------------------------------------------------------------------


NNOutput::NNOutput()
  :whiteOwnerMap(NULL),noisedPolicyProbs(NULL)
{}
NNOutput::NNOutput(const NNOutput& other) {
  nnHash = other.nnHash;
  whiteWinProb = other.whiteWinProb;
  whiteLossProb = other.whiteLossProb;
  whiteNoResultProb = other.whiteNoResultProb;
  whiteScoreMean = other.whiteScoreMean;
  whiteScoreMeanSq = other.whiteScoreMeanSq;
  whiteLead = other.whiteLead;
  varTimeLeft = other.varTimeLeft;
  shorttermWinlossError = other.shorttermWinlossError;
  shorttermScoreError = other.shorttermScoreError;

  nnXLen = other.nnXLen;
  nnYLen = other.nnYLen;
  if(other.whiteOwnerMap != NULL) {
    whiteOwnerMap = new float[nnXLen * nnYLen];
    std::copy(other.whiteOwnerMap, other.whiteOwnerMap + nnXLen * nnYLen, whiteOwnerMap);
  }
  else
    whiteOwnerMap = NULL;

  if(other.noisedPolicyProbs != NULL) {
    noisedPolicyProbs = new float[NNPos::MAX_NN_POLICY_SIZE];
    std::copy(other.noisedPolicyProbs, other.noisedPolicyProbs + NNPos::MAX_NN_POLICY_SIZE, noisedPolicyProbs);
  }
  else
    noisedPolicyProbs = NULL;

  std::copy(other.policyProbs, other.policyProbs+NNPos::MAX_NN_POLICY_SIZE, policyProbs);
}

NNOutput::NNOutput(const vector<shared_ptr<NNOutput>>& others) {
  assert(others.size() < 1000000);
  int len = (int)others.size();
  float floatLen = (float)len;
  assert(len > 0);
  for(int i = 1; i<len; i++) {
    assert(others[i]->nnHash == others[0]->nnHash);
  }
  nnHash = others[0]->nnHash;

  whiteWinProb = 0.0f;
  whiteLossProb = 0.0f;
  whiteNoResultProb = 0.0f;
  whiteScoreMean = 0.0f;
  whiteScoreMeanSq = 0.0f;
  whiteLead = 0.0f;
  varTimeLeft = 0.0f;
  shorttermWinlossError = 0.0f;
  shorttermScoreError = 0.0f;
  for(int i = 0; i<len; i++) {
    const NNOutput& other = *(others[i]);
    whiteWinProb += other.whiteWinProb;
    whiteLossProb += other.whiteLossProb;
    whiteNoResultProb += other.whiteNoResultProb;
    whiteScoreMean += other.whiteScoreMean;
    whiteScoreMeanSq += other.whiteScoreMeanSq;
    whiteLead += other.whiteLead;
    varTimeLeft += other.varTimeLeft;
    shorttermWinlossError += other.shorttermWinlossError;
    shorttermScoreError += other.shorttermScoreError;
  }
  whiteWinProb /= floatLen;
  whiteLossProb /= floatLen;
  whiteNoResultProb /= floatLen;
  whiteScoreMean /= floatLen;
  whiteScoreMeanSq /= floatLen;
  whiteLead /= floatLen;
  varTimeLeft /= floatLen;
  shorttermWinlossError /= floatLen;
  shorttermScoreError /= floatLen;

  nnXLen = others[0]->nnXLen;
  nnYLen = others[0]->nnYLen;

  {
    float whiteOwnerMapCount = 0.0f;
    whiteOwnerMap = NULL;
    for(int i = 0; i<len; i++) {
      const NNOutput& other = *(others[i]);
      if(other.whiteOwnerMap != NULL) {
        if(whiteOwnerMap == NULL) {
          whiteOwnerMap = new float[nnXLen * nnYLen];
          std::fill(whiteOwnerMap, whiteOwnerMap + nnXLen * nnYLen, 0.0f);
        }
        whiteOwnerMapCount += 1.0f;
        for(int pos = 0; pos<nnXLen*nnYLen; pos++)
          whiteOwnerMap[pos] += other.whiteOwnerMap[pos];
      }
    }
    if(whiteOwnerMap != NULL) {
      assert(whiteOwnerMapCount > 0);
      for(int pos = 0; pos<nnXLen*nnYLen; pos++)
        whiteOwnerMap[pos] /= whiteOwnerMapCount;
    }
  }

  noisedPolicyProbs = NULL;

  //For technical correctness in case of impossibly rare hash collisions:
  //Just give up if they don't all match in move legality
  {
    bool mismatch = false;
    std::fill(policyProbs, policyProbs + NNPos::MAX_NN_POLICY_SIZE, 0.0f);
    for(int i = 0; i<len; i++) {
      const NNOutput& other = *(others[i]);
      for(int pos = 0; pos<NNPos::MAX_NN_POLICY_SIZE; pos++) {
        if(i > 0 && (policyProbs[pos] < 0) != (other.policyProbs[pos] < 0))
          mismatch = true;
        policyProbs[pos] += other.policyProbs[pos];
      }
    }
    //In case of mismatch, just take the first one
    //This should basically never happen, only on true hash collisions
    if(mismatch) {
      const NNOutput& other = *(others[0]);
      std::copy(other.policyProbs, other.policyProbs + NNPos::MAX_NN_POLICY_SIZE, policyProbs);
    }
    else {
      for(int pos = 0; pos<NNPos::MAX_NN_POLICY_SIZE; pos++)
        policyProbs[pos] /= floatLen;
    }
  }

}

NNOutput& NNOutput::operator=(const NNOutput& other) {
  if(&other == this)
    return *this;
  nnHash = other.nnHash;
  whiteWinProb = other.whiteWinProb;
  whiteLossProb = other.whiteLossProb;
  whiteNoResultProb = other.whiteNoResultProb;
  whiteScoreMean = other.whiteScoreMean;
  whiteScoreMeanSq = other.whiteScoreMeanSq;
  whiteLead = other.whiteLead;
  varTimeLeft = other.varTimeLeft;
  shorttermWinlossError = other.shorttermWinlossError;
  shorttermScoreError = other.shorttermScoreError;

  nnXLen = other.nnXLen;
  nnYLen = other.nnYLen;
  if(whiteOwnerMap != NULL)
    delete[] whiteOwnerMap;
  if(other.whiteOwnerMap != NULL) {
    whiteOwnerMap = new float[nnXLen * nnYLen];
    std::copy(other.whiteOwnerMap, other.whiteOwnerMap + nnXLen * nnYLen, whiteOwnerMap);
  }
  else
    whiteOwnerMap = NULL;
  if(noisedPolicyProbs != NULL)
    delete[] noisedPolicyProbs;
  if(other.noisedPolicyProbs != NULL) {
    noisedPolicyProbs = new float[NNPos::MAX_NN_POLICY_SIZE];
    std::copy(other.noisedPolicyProbs, other.noisedPolicyProbs + NNPos::MAX_NN_POLICY_SIZE, noisedPolicyProbs);
  }
  else
    noisedPolicyProbs = NULL;

  std::copy(other.policyProbs, other.policyProbs+NNPos::MAX_NN_POLICY_SIZE, policyProbs);

  return *this;
}


NNOutput::~NNOutput() {
  if(whiteOwnerMap != NULL) {
    delete[] whiteOwnerMap;
    whiteOwnerMap = NULL;
  }
  if(noisedPolicyProbs != NULL) {
    delete[] noisedPolicyProbs;
    noisedPolicyProbs = NULL;
  }
}


void NNOutput::debugPrint(ostream& out, const Board& board) {
  out << "Win " << Global::strprintf("%.2fc",whiteWinProb*100) << endl;
  out << "Loss " << Global::strprintf("%.2fc",whiteLossProb*100) << endl;
  out << "NoResult " << Global::strprintf("%.2fc",whiteNoResultProb*100) << endl;
  out << "ScoreMean " << Global::strprintf("%.1f",whiteScoreMean) << endl;
  out << "ScoreMeanSq " << Global::strprintf("%.1f",whiteScoreMeanSq) << endl;
  out << "Lead " << Global::strprintf("%.1f",whiteLead) << endl;
  out << "VarTimeLeft " << Global::strprintf("%.1f",varTimeLeft) << endl;
  out << "STWinlossError " << Global::strprintf("%.1f",shorttermWinlossError) << endl;
  out << "STScoreError " << Global::strprintf("%.1f",shorttermScoreError) << endl;

  out << "Policy" << endl;
  for(int y = 0; y<board.y_size; y++) {
    for(int x = 0; x<board.x_size; x++) {
      int pos = NNPos::xyToPos(x,y,nnXLen);
      float prob = policyProbs[pos];
      if(prob < 0)
        out << "   - ";
      else
        out << Global::strprintf("%4d ", (int)round(prob * 1000));
    }
    out << endl;
  }

  if(whiteOwnerMap != NULL) {
    for(int y = 0; y<board.y_size; y++) {
      for(int x = 0; x<board.x_size; x++) {
        int pos = NNPos::xyToPos(x,y,nnXLen);
        float whiteOwn = whiteOwnerMap[pos];
        out << Global::strprintf("%5d ", (int)round(whiteOwn * 1000));
      }
      out << endl;
    }
    out << endl;
  }
}

//-------------------------------------------------------------------------------------------------------------

static void copyWithSymmetry(const float* src, float* dst, int nSize, int hSize, int wSize, int cSize, bool useNHWC, int symmetry, bool reverse) {
  bool transpose = (symmetry & 0x4) != 0 && hSize == wSize;
  bool flipX = (symmetry & 0x2) != 0;
  bool flipY = (symmetry & 0x1) != 0;
  if(transpose && !reverse)
    std::swap(flipX,flipY);
  if(useNHWC) {
    int nStride = hSize * wSize * cSize;
    int hStride = wSize * cSize;
    int wStride = cSize;
    int hBaseNew = 0; int hStrideNew = hStride;
    int wBaseNew = 0; int wStrideNew = wStride;

    if(flipY) { hBaseNew = (hSize-1) * hStrideNew; hStrideNew = -hStrideNew; }
    if(flipX) { wBaseNew = (wSize-1) * wStrideNew; wStrideNew = -wStrideNew; }

    if(transpose)
      std::swap(hStrideNew,wStrideNew);

    for(int n = 0; n<nSize; n++) {
      for(int h = 0; h<hSize; h++) {
        int nhOld = n * nStride + h*hStride;
        int nhNew = n * nStride + hBaseNew + h*hStrideNew;
        for(int w = 0; w<wSize; w++) {
          int nhwOld = nhOld + w*wStride;
          int nhwNew = nhNew + wBaseNew + w*wStrideNew;
          for(int c = 0; c<cSize; c++) {
            dst[nhwNew + c] = src[nhwOld + c];
          }
        }
      }
    }
  }
  else {
    int ncSize = nSize * cSize;
    int ncStride = hSize * wSize;
    int hStride = wSize;
    int wStride = 1;
    int hBaseNew = 0; int hStrideNew = hStride;
    int wBaseNew = 0; int wStrideNew = wStride;

    if(flipY) { hBaseNew = (hSize-1) * hStrideNew; hStrideNew = -hStrideNew; }
    if(flipX) { wBaseNew = (wSize-1) * wStrideNew; wStrideNew = -wStrideNew; }

    if(transpose)
      std::swap(hStrideNew,wStrideNew);

    for(int nc = 0; nc<ncSize; nc++) {
      for(int h = 0; h<hSize; h++) {
        int nchOld = nc * ncStride + h*hStride;
        int nchNew = nc * ncStride + hBaseNew + h*hStrideNew;
        for(int w = 0; w<wSize; w++) {
          int nchwOld = nchOld + w*wStride;
          int nchwNew = nchNew + wBaseNew + w*wStrideNew;
          dst[nchwNew] = src[nchwOld];
        }
      }
    }
  }
}


void SymmetryHelpers::copyInputsWithSymmetry(const float* src, float* dst, int nSize, int hSize, int wSize, int cSize, bool useNHWC, int symmetry) {
  copyWithSymmetry(src, dst, nSize, hSize, wSize, cSize, useNHWC, symmetry, false);
}

void SymmetryHelpers::copyOutputsWithSymmetry(const float* src, float* dst, int nSize, int hSize, int wSize, int symmetry) {
  copyWithSymmetry(src, dst, nSize, hSize, wSize, 1, false, symmetry, true);
}

Loc SymmetryHelpers::getSymLoc(int x, int y, const Board& board, int symmetry) {
  bool transpose = (symmetry & 0x4) != 0;
  bool flipX = (symmetry & 0x2) != 0;
  bool flipY = (symmetry & 0x1) != 0;
  if(flipX) { x = board.x_size - x - 1; }
  if(flipY) { y = board.y_size - y - 1; }

  if(transpose)
    std::swap(x,y);
  return Location::getLoc(x,y,transpose ? board.y_size : board.x_size);
}

//-------------------------------------------------------------------------------------------------------------

static void setRowBin(float* rowBin, int pos, int feature, float value, int posStride, int featureStride) {
  rowBin[pos * posStride + feature * featureStride] = value;
}

//===========================================================================================
//INPUTSVERSION 3
//===========================================================================================

void NNInputs::fillRowV3(
  const Board& board, const BoardHistory& hist, Player nextPlayer,
  const MiscNNInputParams& nnInputParams,
  int nnXLen, int nnYLen, bool useNHWC, float* rowBin, float* rowGlobal
) {
  assert(nnXLen <= NNPos::MAX_BOARD_LEN);
  assert(nnYLen <= NNPos::MAX_BOARD_LEN);
  assert(board.x_size <= nnXLen);
  assert(board.y_size <= nnYLen);
  std::fill(rowBin,rowBin+NUM_FEATURES_SPATIAL_V3*nnXLen*nnYLen,false);
  std::fill(rowGlobal,rowGlobal+NUM_FEATURES_GLOBAL_V3,0.0f);

  Player pla = nextPlayer;
  Player opp = getOpp(pla);
  int xSize = board.x_size;
  int ySize = board.y_size;

  int featureStride;
  int posStride;
  if(useNHWC) {
    featureStride = 1;
    posStride = NNInputs::NUM_FEATURES_SPATIAL_V3;
  }
  else {
    featureStride = nnXLen * nnYLen;
    posStride = 1;
  }

  for(int y = 0; y<ySize; y++) {
    for(int x = 0; x<xSize; x++) {
      int pos = NNPos::xyToPos(x,y,nnXLen);
      Loc loc = Location::getLoc(x,y,xSize);

      //Feature 0 - on board
      setRowBin(rowBin,pos,0, 1.0f, posStride, featureStride);

      Color stone = board.colors[loc];

      //Features 1,2 - pla,opp stone
      if(stone == pla)
        setRowBin(rowBin,pos,1, 1.0f, posStride, featureStride);
      else if(stone == opp)
        setRowBin(rowBin,pos,2, 1.0f, posStride, featureStride);
    }
  }
  /*
  //Hide history from the net if a pass would end things and we're behaving as if a pass won't.
  //Or if the game is in fact over right now!
  bool hideHistory =
    hist.isGameFinished ||
    (nnInputParams.conservativePass && hist.passWouldEndGame(board,nextPlayer));
  */
  bool hideHistory = false;
  //Features 3,4,5,6,7,8
  if(!hideHistory) {
    const vector<Move>& moveHistory = hist.moveHistory;
    size_t moveHistoryLen = moveHistory.size();
    if(moveHistoryLen >= 1 && moveHistory[moveHistoryLen-1].pla == opp) {
      Loc prev1Loc = moveHistory[moveHistoryLen-1].fromLoc;
      if(prev1Loc != Board::NULL_LOC) {
        int pos = NNPos::locToPos(prev1Loc,xSize,nnXLen,nnYLen);
        setRowBin(rowBin,pos,3, 1.0f, posStride, featureStride);
      }
      prev1Loc = moveHistory[moveHistoryLen-1].toLoc;
      if(prev1Loc != Board::NULL_LOC) {
        int pos = NNPos::locToPos(prev1Loc,xSize,nnXLen,nnYLen);
        setRowBin(rowBin,pos,4, 1.0f, posStride, featureStride);
      }
      if(moveHistoryLen >= 2 && moveHistory[moveHistoryLen-2].pla == pla) {
        Loc prev2Loc = moveHistory[moveHistoryLen-2].fromLoc;
        if(prev2Loc != Board::NULL_LOC) {
          int pos = NNPos::locToPos(prev2Loc,xSize,nnXLen,nnYLen);
          setRowBin(rowBin,pos,5, 1.0f, posStride, featureStride);
        }
        prev2Loc = moveHistory[moveHistoryLen-2].toLoc;
        if(prev2Loc != Board::NULL_LOC) {
          int pos = NNPos::locToPos(prev2Loc,xSize,nnXLen,nnYLen);
          setRowBin(rowBin,pos,6, 1.0f, posStride, featureStride);
        }
        if(moveHistoryLen >= 3 && moveHistory[moveHistoryLen-3].pla == opp) {
          Loc prev3Loc = moveHistory[moveHistoryLen-3].fromLoc;
          if(prev3Loc != Board::NULL_LOC) {
            int pos = NNPos::locToPos(prev3Loc,xSize,nnXLen,nnYLen);
            setRowBin(rowBin,pos,7, 1.0f, posStride, featureStride);
          }
          prev3Loc = moveHistory[moveHistoryLen-3].toLoc;
          if(prev3Loc != Board::NULL_LOC) {
            int pos = NNPos::locToPos(prev3Loc,xSize,nnXLen,nnYLen);
            setRowBin(rowBin,pos,8, 1.0f, posStride, featureStride);
          }
        }
      }
    }
  }

  //Global features.

}


//===========================================================================================
//INPUTSVERSION 4
//===========================================================================================

void NNInputs::fillRowV4(
  const Board& board, const BoardHistory& hist, Player nextPlayer,
  const MiscNNInputParams& nnInputParams,
  int nnXLen, int nnYLen, bool useNHWC, float* rowBin, float* rowGlobal
) {
  assert(nnXLen <= NNPos::MAX_BOARD_LEN);
  assert(nnYLen <= NNPos::MAX_BOARD_LEN);
  assert(board.x_size <= nnXLen);
  assert(board.y_size <= nnYLen);
  std::fill(rowBin,rowBin+NUM_FEATURES_SPATIAL_V4*nnXLen*nnYLen,false);
  std::fill(rowGlobal,rowGlobal+NUM_FEATURES_GLOBAL_V4,0.0f);

  Player pla = nextPlayer;
  Player opp = getOpp(pla);
  int xSize = board.x_size;
  int ySize = board.y_size;

  int featureStride;
  int posStride;
  if(useNHWC) {
    featureStride = 1;
    posStride = NNInputs::NUM_FEATURES_SPATIAL_V4;
  }
  else {
    featureStride = nnXLen * nnYLen;
    posStride = 1;
  }

  for(int y = 0; y<ySize; y++) {
    for(int x = 0; x<xSize; x++) {
      int pos = NNPos::xyToPos(x,y,nnXLen);
      Loc loc = Location::getLoc(x,y,xSize);

      //Feature 0 - on board
      setRowBin(rowBin,pos,0, 1.0f, posStride, featureStride);

      Color stone = board.colors[loc];

      //Features 1,2 - pla,opp stone
      //Features 3,4,5 - 1,2,3 libs
      if(stone == pla)
        setRowBin(rowBin,pos,1, 1.0f, posStride, featureStride);
      else if(stone == opp)
        setRowBin(rowBin,pos,2, 1.0f, posStride, featureStride);
    }
  }

  /*
  //Hide history from the net if a pass would end things and we're behaving as if a pass won't.
  //Or if the game is in fact over right now!
  bool hideHistory =
    hist.isGameFinished ||
    (nnInputParams.conservativePass && hist.passWouldEndGame(board,nextPlayer));
  */
  bool hideHistory = false;
  //Features 3,4,5,6,7,8
  if(!hideHistory) {
    const vector<Move>& moveHistory = hist.moveHistory;
    size_t moveHistoryLen = moveHistory.size();
    if(moveHistoryLen >= 1 && moveHistory[moveHistoryLen-1].pla == opp) {
      Loc prev1Loc = moveHistory[moveHistoryLen-1].fromLoc;
      if(prev1Loc != Board::NULL_LOC) {
        int pos = NNPos::locToPos(prev1Loc,xSize,nnXLen,nnYLen);
        setRowBin(rowBin,pos,3, 1.0f, posStride, featureStride);
      }
      prev1Loc = moveHistory[moveHistoryLen-1].toLoc;
      if(prev1Loc != Board::NULL_LOC) {
        int pos = NNPos::locToPos(prev1Loc,xSize,nnXLen,nnYLen);
        setRowBin(rowBin,pos,4, 1.0f, posStride, featureStride);
      }
      if(moveHistoryLen >= 2 && moveHistory[moveHistoryLen-2].pla == pla) {
        Loc prev2Loc = moveHistory[moveHistoryLen-2].fromLoc;
        if(prev2Loc != Board::NULL_LOC) {
          int pos = NNPos::locToPos(prev2Loc,xSize,nnXLen,nnYLen);
          setRowBin(rowBin,pos,5, 1.0f, posStride, featureStride);
        }
        prev2Loc = moveHistory[moveHistoryLen-2].toLoc;
        if(prev2Loc != Board::NULL_LOC) {
          int pos = NNPos::locToPos(prev2Loc,xSize,nnXLen,nnYLen);
          setRowBin(rowBin,pos,6, 1.0f, posStride, featureStride);
        }
        if(moveHistoryLen >= 3 && moveHistory[moveHistoryLen-3].pla == opp) {
          Loc prev3Loc = moveHistory[moveHistoryLen-3].fromLoc;
          if(prev3Loc != Board::NULL_LOC) {
            int pos = NNPos::locToPos(prev3Loc,xSize,nnXLen,nnYLen);
            setRowBin(rowBin,pos,7, 1.0f, posStride, featureStride);
          }
          prev3Loc = moveHistory[moveHistoryLen-3].toLoc;
          if(prev3Loc != Board::NULL_LOC) {
            int pos = NNPos::locToPos(prev3Loc,xSize,nnXLen,nnYLen);
            setRowBin(rowBin,pos,8, 1.0f, posStride, featureStride);
          }
        }
      }
    }
  }

  //Global features.
}



//===========================================================================================
//INPUTSVERSION 5
//===========================================================================================

void NNInputs::fillRowV5(
  const Board& board, const BoardHistory& hist, Player nextPlayer,
  const MiscNNInputParams& nnInputParams,
  int nnXLen, int nnYLen, bool useNHWC, float* rowBin, float* rowGlobal
) {
  assert(nnXLen <= NNPos::MAX_BOARD_LEN);
  assert(nnYLen <= NNPos::MAX_BOARD_LEN);
  assert(board.x_size <= nnXLen);
  assert(board.y_size <= nnYLen);
  std::fill(rowBin,rowBin+NUM_FEATURES_SPATIAL_V5*nnXLen*nnYLen,false);
  std::fill(rowGlobal,rowGlobal+NUM_FEATURES_GLOBAL_V5,0.0f);

  Player pla = nextPlayer;
  Player opp = getOpp(pla);
  int xSize = board.x_size;
  int ySize = board.y_size;

  int featureStride;
  int posStride;
  if(useNHWC) {
    featureStride = 1;
    posStride = NNInputs::NUM_FEATURES_SPATIAL_V5;
  }
  else {
    featureStride = nnXLen * nnYLen;
    posStride = 1;
  }

  for(int y = 0; y<ySize; y++) {
    for(int x = 0; x<xSize; x++) {
      int pos = NNPos::xyToPos(x,y,nnXLen);
      Loc loc = Location::getLoc(x,y,xSize);

      //Feature 0 - on board
      setRowBin(rowBin,pos,0, 1.0f, posStride, featureStride);

      Color stone = board.colors[loc];

      //Features 1,2 - pla,opp stone
      if(stone == pla)
        setRowBin(rowBin,pos,1, 1.0f, posStride, featureStride);
      else if(stone == opp)
        setRowBin(rowBin,pos,2, 1.0f, posStride, featureStride);
    }
  }

  /*
  //Hide history from the net if a pass would end things and we're behaving as if a pass won't.
  //Or if the game is in fact over right now!
  bool hideHistory =
    hist.isGameFinished ||
    (nnInputParams.conservativePass && hist.passWouldEndGame(board,nextPlayer));
  */
  bool hideHistory = false;
  //Features 3,4,5,6,7,8
  if(!hideHistory) {
    const vector<Move>& moveHistory = hist.moveHistory;
    size_t moveHistoryLen = moveHistory.size();
    if(moveHistoryLen >= 1 && moveHistory[moveHistoryLen-1].pla == opp) {
      Loc prev1Loc = moveHistory[moveHistoryLen-1].fromLoc;
      if(prev1Loc != Board::NULL_LOC) {
        int pos = NNPos::locToPos(prev1Loc,xSize,nnXLen,nnYLen);
        setRowBin(rowBin,pos,3, 1.0f, posStride, featureStride);
      }
      prev1Loc = moveHistory[moveHistoryLen-1].toLoc;
      if(prev1Loc != Board::NULL_LOC) {
        int pos = NNPos::locToPos(prev1Loc,xSize,nnXLen,nnYLen);
        setRowBin(rowBin,pos,4, 1.0f, posStride, featureStride);
      }
      if(moveHistoryLen >= 2 && moveHistory[moveHistoryLen-2].pla == pla) {
        Loc prev2Loc = moveHistory[moveHistoryLen-2].fromLoc;
        if(prev2Loc != Board::NULL_LOC) {
          int pos = NNPos::locToPos(prev2Loc,xSize,nnXLen,nnYLen);
          setRowBin(rowBin,pos,5, 1.0f, posStride, featureStride);
        }
        prev2Loc = moveHistory[moveHistoryLen-2].toLoc;
        if(prev2Loc != Board::NULL_LOC) {
          int pos = NNPos::locToPos(prev2Loc,xSize,nnXLen,nnYLen);
          setRowBin(rowBin,pos,6, 1.0f, posStride, featureStride);
        }
        if(moveHistoryLen >= 3 && moveHistory[moveHistoryLen-3].pla == opp) {
          Loc prev3Loc = moveHistory[moveHistoryLen-3].fromLoc;
          if(prev3Loc != Board::NULL_LOC) {
            int pos = NNPos::locToPos(prev3Loc,xSize,nnXLen,nnYLen);
            setRowBin(rowBin,pos,7, 1.0f, posStride, featureStride);
          }
          prev3Loc = moveHistory[moveHistoryLen-3].toLoc;
          if(prev3Loc != Board::NULL_LOC) {
            int pos = NNPos::locToPos(prev3Loc,xSize,nnXLen,nnYLen);
            setRowBin(rowBin,pos,8, 1.0f, posStride, featureStride);
          }
        }
      }
    }
  }

}

//===========================================================================================
//INPUTSVERSION 6
//===========================================================================================


void NNInputs::fillRowV6(
  const Board& board, const BoardHistory& hist, Player nextPlayer,
  const MiscNNInputParams& nnInputParams,
  int nnXLen, int nnYLen, bool useNHWC, float* rowBin, float* rowGlobal
) {
  assert(nnXLen <= NNPos::MAX_BOARD_LEN);
  assert(nnYLen <= NNPos::MAX_BOARD_LEN);
  assert(board.x_size <= nnXLen);
  assert(board.y_size <= nnYLen);
  std::fill(rowBin,rowBin+NUM_FEATURES_SPATIAL_V6*nnXLen*nnYLen,false);
  std::fill(rowGlobal,rowGlobal+NUM_FEATURES_GLOBAL_V6,0.0f);

  Player pla = nextPlayer;
  Player opp = getOpp(pla);
  int xSize = board.x_size;
  int ySize = board.y_size;

  int featureStride;
  int posStride;
  if(useNHWC) {
    featureStride = 1;
    posStride = NNInputs::NUM_FEATURES_SPATIAL_V6;
  }
  else {
    featureStride = nnXLen * nnYLen;
    posStride = 1;
  }

  for(int y = 0; y<ySize; y++) {
    for(int x = 0; x<xSize; x++) {
      int pos = NNPos::xyToPos(x,y,nnXLen);
      Loc loc = Location::getLoc(x,y,xSize);

      //Feature 0 - on board
      setRowBin(rowBin,pos,0, 1.0f, posStride, featureStride);

      Color stone = board.colors[loc];

      //Features 1,2 - pla,opp stone
      //Features 3,4,5 - 1,2,3 libs
      if(stone == pla)
        setRowBin(rowBin,pos,1, 1.0f, posStride, featureStride);
      else if(stone == opp)
        setRowBin(rowBin,pos,2, 1.0f, posStride, featureStride);
    }
  }

  /*
  //Hide history from the net if a pass would end things and we're behaving as if a pass won't.
  //Or if the game is in fact over right now!
  bool hideHistory =
    hist.isGameFinished ||
    (nnInputParams.conservativePass && hist.passWouldEndGame(board,nextPlayer));
  */
  bool hideHistory = false;
  //Features 3,4,5,6,7,8
  if(!hideHistory) {
    const vector<Move>& moveHistory = hist.moveHistory;
    size_t moveHistoryLen = moveHistory.size();
    if(moveHistoryLen >= 1 && moveHistory[moveHistoryLen-1].pla == opp) {
      Loc prev1Loc = moveHistory[moveHistoryLen-1].fromLoc;
      if(prev1Loc != Board::NULL_LOC) {
        int pos = NNPos::locToPos(prev1Loc,xSize,nnXLen,nnYLen);
        setRowBin(rowBin,pos,3, 1.0f, posStride, featureStride);
      }
      prev1Loc = moveHistory[moveHistoryLen-1].toLoc;
      if(prev1Loc != Board::NULL_LOC) {
        int pos = NNPos::locToPos(prev1Loc,xSize,nnXLen,nnYLen);
        setRowBin(rowBin,pos,4, 1.0f, posStride, featureStride);
      }
      if(moveHistoryLen >= 2 && moveHistory[moveHistoryLen-2].pla == pla) {
        Loc prev2Loc = moveHistory[moveHistoryLen-2].fromLoc;
        if(prev2Loc != Board::NULL_LOC) {
          int pos = NNPos::locToPos(prev2Loc,xSize,nnXLen,nnYLen);
          setRowBin(rowBin,pos,5, 1.0f, posStride, featureStride);
        }
        prev2Loc = moveHistory[moveHistoryLen-2].toLoc;
        if(prev2Loc != Board::NULL_LOC) {
          int pos = NNPos::locToPos(prev2Loc,xSize,nnXLen,nnYLen);
          setRowBin(rowBin,pos,6, 1.0f, posStride, featureStride);
        }
        if(moveHistoryLen >= 3 && moveHistory[moveHistoryLen-3].pla == opp) {
          Loc prev3Loc = moveHistory[moveHistoryLen-3].fromLoc;
          if(prev3Loc != Board::NULL_LOC) {
            int pos = NNPos::locToPos(prev3Loc,xSize,nnXLen,nnYLen);
            setRowBin(rowBin,pos,7, 1.0f, posStride, featureStride);
          }
          prev3Loc = moveHistory[moveHistoryLen-3].toLoc;
          if(prev3Loc != Board::NULL_LOC) {
            int pos = NNPos::locToPos(prev3Loc,xSize,nnXLen,nnYLen);
            setRowBin(rowBin,pos,8, 1.0f, posStride, featureStride);
          }
        }
      }
    }
  }
}

//===========================================================================================
//INPUTSVERSION 7
//===========================================================================================


void NNInputs::fillRowV7(
  const Board& board, const BoardHistory& hist, Player nextPlayer,
  const MiscNNInputParams& nnInputParams,
  int nnXLen, int nnYLen, bool useNHWC, float* rowBin, float* rowGlobal
) {
  assert(nnXLen <= NNPos::MAX_BOARD_LEN);
  assert(nnYLen <= NNPos::MAX_BOARD_LEN);
  assert(board.x_size <= nnXLen);
  assert(board.y_size <= nnYLen);
  std::fill(rowBin,rowBin+NUM_FEATURES_SPATIAL_V7*nnXLen*nnYLen,false);
  std::fill(rowGlobal,rowGlobal+NUM_FEATURES_GLOBAL_V7,0.0f);

  Player pla = nextPlayer;
  Player opp = getOpp(pla);
  int xSize = board.x_size;
  int ySize = board.y_size;

  int featureStride;
  int posStride;
  if(useNHWC) {
    featureStride = 1;
    posStride = NNInputs::NUM_FEATURES_SPATIAL_V7;
  }
  else {
    featureStride = nnXLen * nnYLen;
    posStride = 1;
  }

  for(int y = 0; y<ySize; y++) {
    for(int x = 0; x<xSize; x++) {
      int pos = NNPos::xyToPos(x,y,nnXLen);
      Loc loc = Location::getLoc(x,y,xSize);

      //Feature 0 - on board
      setRowBin(rowBin,pos,0, 1.0f, posStride, featureStride);

      Color stone = board.colors[loc];

      //Features 1,2 - pla,opp stone
      //Features 3,4,5 - 1,2,3 libs
      if(stone == pla)
        setRowBin(rowBin,pos,1, 1.0f, posStride, featureStride);
      else if(stone == opp)
        setRowBin(rowBin,pos,2, 1.0f, posStride, featureStride);
    }
  }

  /*
  //Hide history from the net if a pass would end things and we're behaving as if a pass won't.
  //Or if the game is in fact over right now!
  bool hideHistory =
    hist.isGameFinished ||
    (nnInputParams.conservativePass && hist.passWouldEndGame(board,nextPlayer));
  */
  bool hideHistory = false;
  //Features 3,4,5,6,7,8
  if(!hideHistory) {
    const vector<Move>& moveHistory = hist.moveHistory;
    size_t moveHistoryLen = moveHistory.size();
    if(moveHistoryLen >= 1 && moveHistory[moveHistoryLen-1].pla == opp) {
      Loc prev1Loc = moveHistory[moveHistoryLen-1].fromLoc;
      if(prev1Loc != Board::NULL_LOC) {
        int pos = NNPos::locToPos(prev1Loc,xSize,nnXLen,nnYLen);
        setRowBin(rowBin,pos,3, 1.0f, posStride, featureStride);
      }
      prev1Loc = moveHistory[moveHistoryLen-1].toLoc;
      if(prev1Loc != Board::NULL_LOC) {
        int pos = NNPos::locToPos(prev1Loc,xSize,nnXLen,nnYLen);
        setRowBin(rowBin,pos,4, 1.0f, posStride, featureStride);
      }
      if(moveHistoryLen >= 2 && moveHistory[moveHistoryLen-2].pla == pla) {
        Loc prev2Loc = moveHistory[moveHistoryLen-2].fromLoc;
        if(prev2Loc != Board::NULL_LOC) {
          int pos = NNPos::locToPos(prev2Loc,xSize,nnXLen,nnYLen);
          setRowBin(rowBin,pos,5, 1.0f, posStride, featureStride);
        }
        prev2Loc = moveHistory[moveHistoryLen-2].toLoc;
        if(prev2Loc != Board::NULL_LOC) {
          int pos = NNPos::locToPos(prev2Loc,xSize,nnXLen,nnYLen);
          setRowBin(rowBin,pos,6, 1.0f, posStride, featureStride);
        }
        if(moveHistoryLen >= 3 && moveHistory[moveHistoryLen-3].pla == opp) {
          Loc prev3Loc = moveHistory[moveHistoryLen-3].fromLoc;
          if(prev3Loc != Board::NULL_LOC) {
            int pos = NNPos::locToPos(prev3Loc,xSize,nnXLen,nnYLen);
            setRowBin(rowBin,pos,7, 1.0f, posStride, featureStride);
          }
          prev3Loc = moveHistory[moveHistoryLen-3].toLoc;
          if(prev3Loc != Board::NULL_LOC) {
            int pos = NNPos::locToPos(prev3Loc,xSize,nnXLen,nnYLen);
            setRowBin(rowBin,pos,8, 1.0f, posStride, featureStride);
          }
        }
      }
    }
  }
}
