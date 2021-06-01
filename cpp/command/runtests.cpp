
#include <sstream>
#include "../core/global.h"
#include "../core/bsearch.h"
#include "../core/rand.h"
#include "../core/elo.h"
#include "../core/fancymath.h"
#include "../core/config_parser.h"
#include "../core/base64.h"
#include "../game/board.h"
#include "../game/rules.h"
#include "../game/boardhistory.h"
#include "../neuralnet/nninputs.h"
#include "../program/gtpconfig.h"
#include "../tests/tests.h"
#include "../tests/tinymodel.h"
#include "../main.h"

using namespace std;

int MainCmds::runtests(int argc, const char* const* argv) {
  (void)argc;
  (void)argv;
  testAssert(sizeof(size_t) == 8);
  Board::initHash();
  ScoreValue::initTables();

  BSearch::runTests();
  Rand::runTests();
  FancyMath::runTests();
  ComputeElos::runTests();
  Base64::runTests();

  Tests::runBoardIOTests();
  Tests::runBoardBasicTests();
  Tests::runBoardAreaTests();

  Tests::runRulesTests();

  Tests::runBoardUndoTest();
  Tests::runBoardHandicapTest();
  Tests::runBoardStressTest();

  Tests::runSgfTests();

  ScoreValue::freeTables();

  cout << "All tests passed" << endl;
  return 0;
}

int MainCmds::runoutputtests(int argc, const char* const* argv) {
  (void)argc;
  (void)argv;
  Board::initHash();
  ScoreValue::initTables();

  Tests::runNNInputsV3V4Tests();
  Tests::runNNLessSearchTests();
  Tests::runTrainingWriteTests();
  Tests::runTimeControlsTests();
  Tests::runScoreTests();
  Tests::runBasicSymmetryTests();
  Tests::runNNSymmetryTests();
  Tests::runSgfFileTests();

  ScoreValue::freeTables();

  return 0;
}

int MainCmds::runsearchtests(int argc, const char* const* argv) {
  Board::initHash();
  ScoreValue::initTables();

  if(argc != 6) {
    cerr << "Must supply exactly five arguments: MODEL_FILE INPUTSNHWC CUDANHWC SYMMETRY FP16" << endl;
    return 1;
  }
  Tests::runSearchTests(
    string(argv[1]),
    Global::stringToBool(argv[2]),
    Global::stringToBool(argv[3]),
    Global::stringToInt(argv[4]),
    Global::stringToBool(argv[5])
  );

  ScoreValue::freeTables();

  return 0;
}

int MainCmds::runsearchtestsv3(int argc, const char* const* argv) {
  Board::initHash();
  ScoreValue::initTables();

  if(argc != 6) {
    cerr << "Must supply exactly five arguments: MODEL_FILE INPUTSNHWC CUDANHWC SYMMETRY FP16" << endl;
    return 1;
  }
  Tests::runSearchTestsV3(
    string(argv[1]),
    Global::stringToBool(argv[2]),
    Global::stringToBool(argv[3]),
    Global::stringToInt(argv[4]),
    Global::stringToBool(argv[5])
  );

  ScoreValue::freeTables();

  return 0;
}

int MainCmds::runsearchtestsv8(int argc, const char* const* argv) {
  Board::initHash();
  ScoreValue::initTables();

  if(argc != 5) {
    cerr << "Must supply exactly four arguments: MODEL_FILE INPUTSNHWC CUDANHWC FP16" << endl;
    return 1;
  }
  Tests::runSearchTestsV8(
    string(argv[1]),
    Global::stringToBool(argv[2]),
    Global::stringToBool(argv[3]),
    Global::stringToBool(argv[4])
  );

  ScoreValue::freeTables();

  return 0;
}

int MainCmds::runselfplayinittests(int argc, const char* const* argv) {
  if(argc != 2) {
    cerr << "Must supply exactly one argument: MODEL_FILE" << endl;
    return 1;
  }

  Board::initHash();
  ScoreValue::initTables();

  Tests::runSelfplayInitTestsWithNN(
    string(argv[1])
  );
  Tests::runMoreSelfplayTestsWithNN(
    string(argv[1])
  );

  ScoreValue::freeTables();

  return 0;
}

int MainCmds::runselfplayinitstattests(int argc, const char* const* argv) {
  if(argc != 2) {
    cerr << "Must supply exactly one argument: MODEL_FILE" << endl;
    return 1;
  }

  Board::initHash();
  ScoreValue::initTables();

  Tests::runSelfplayStatTestsWithNN(
    string(argv[1])
  );

  ScoreValue::freeTables();

  return 0;
}

int MainCmds::runsekitrainwritetests(int argc, const char* const* argv) {
  if(argc != 2) {
    cerr << "Must supply exactly one argument: MODEL_FILE" << endl;
    return 1;
  }

  Board::initHash();
  ScoreValue::initTables();

  Tests::runSekiTrainWriteTests(
    string(argv[1])
  );

  ScoreValue::freeTables();

  return 0;
}

int MainCmds::runnnlayertests(int argc, const char* const* argv) {
  (void)argc;
  (void)argv;
  Tests::runNNLayerTests();
  return 0;
}

int MainCmds::runnnontinyboardtest(int argc, const char* const* argv) {
  if(argc != 6) {
    cerr << "Must supply exactly five arguments: MODEL_FILE INPUTSNHWC CUDANHWC SYMMETRY FP16" << endl;
    return 1;
  }
  Board::initHash();
  ScoreValue::initTables();

  Tests::runNNOnTinyBoard(
    string(argv[1]),
    Global::stringToBool(argv[2]),
    Global::stringToBool(argv[3]),
    Global::stringToInt(argv[4]),
    Global::stringToBool(argv[5])
  );

  ScoreValue::freeTables();

  return 0;
}

int MainCmds::runnnsymmetriestest(int argc, const char* const* argv) {
  if(argc != 5) {
    cerr << "Must supply exactly four arguments: MODEL_FILE INPUTSNHWC CUDANHWC FP16" << endl;
    return 1;
  }
  Board::initHash();
  ScoreValue::initTables();

  Tests::runNNSymmetries(
    string(argv[1]),
    Global::stringToBool(argv[2]),
    Global::stringToBool(argv[3]),
    Global::stringToBool(argv[4])
  );

  ScoreValue::freeTables();

  return 0;
}

int MainCmds::runnnonmanyposestest(int argc, const char* const* argv) {
  if(argc != 6 && argc != 7) {
    cerr << "Must supply five or six arguments: MODEL_FILE INPUTSNHWC CUDANHWC SYMMETRY FP16 [COMPARISONFILE]" << endl;
    return 1;
  }
  Board::initHash();
  ScoreValue::initTables();

  if(argc == 6) {
    Tests::runNNOnManyPoses(
      string(argv[1]),
      Global::stringToBool(argv[2]),
      Global::stringToBool(argv[3]),
      Global::stringToInt(argv[4]),
      Global::stringToBool(argv[5]),
      ""
    );
  }
  else if(argc == 7) {
    Tests::runNNOnManyPoses(
      string(argv[1]),
      Global::stringToBool(argv[2]),
      Global::stringToBool(argv[3]),
      Global::stringToInt(argv[4]),
      Global::stringToBool(argv[5]),
      string(argv[6])
    );
  }

  ScoreValue::freeTables();

  return 0;
}

int MainCmds::runnnbatchingtest(int argc, const char* const* argv) {
  if(argc != 5) {
    cerr << "Must supply exactly four arguments: MODEL_FILE INPUTSNHWC CUDANHWC FP16" << endl;
    return 1;
  }
  Board::initHash();
  ScoreValue::initTables();

  Tests::runNNBatchingTest(
    string(argv[1]),
    Global::stringToBool(argv[2]),
    Global::stringToBool(argv[3]),
    Global::stringToBool(argv[4])
  );

  ScoreValue::freeTables();

  return 0;
}


int MainCmds::runownershiptests(int argc, const char* const* argv) {
  if(argc != 3) {
    cerr << "Must supply exactly two arguments: GTP_CONFIG MODEL_FILE" << endl;
    return 1;
  }
  Board::initHash();
  ScoreValue::initTables();

  Tests::runOwnershipTests(
    string(argv[1]),
    string(argv[2])
  );

  ScoreValue::freeTables();
  return 0;
}


int MainCmds::runtinynntests(int argc, const char* const* argv) {
  if(argc != 2) {
    cerr << "Must supply exactly one arguments: TMPDIR" << endl;
    return 1;
  }
  Board::initHash();
  ScoreValue::initTables();

  Logger logger;
  logger.setLogToStdout(true);
  logger.setLogTime(false);

  ConfigParser cfg;
  {
    //Dummy parameters
    int maxVisits = 500;
    int maxPlayouts = 500;
    double maxTime = 1.0;
    double maxPonderTime = 1.0;
    int nnCacheSizePowerOfTwo = 12;
    int nnMutexPoolSizePowerOfTwo = 10;
    int numSearchThreads = 3;
    string cfgStr = GTPConfig::makeConfig(
      Rules(),
      maxVisits,
      maxPlayouts,
      maxTime,
      maxPonderTime,
      vector<int>(),
      nnCacheSizePowerOfTwo,
      nnMutexPoolSizePowerOfTwo,
      numSearchThreads
    );
    istringstream in(cfgStr);
    cfg.initialize(in);
  }

  TinyModelTest::runTinyModelTest(
    string(argv[1]),
    logger,
    cfg
  );

  ScoreValue::freeTables();
  return 0;
}
