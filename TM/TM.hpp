#pragma once

#include "BWAPI.h"

#ifdef BWAPI4
#include <chrono>
#elif BWAPI3
#include <boost/chrono.hpp>
#endif
#include <fstream>

#ifdef BWAPI4
using SteadyClock = std::chrono::steady_clock;
using TimePoint = std::chrono::time_point<SteadyClock>;
using Duration = std::chrono::duration<float, std::milli>;
#elif BWAPI3
typedef boost::chrono::steady_clock SteadyClock;
typedef boost::chrono::time_point<SteadyClock> TimePoint;
typedef boost::chrono::duration<float, boost::milli> Duration;
#endif

#ifdef BWAPI4
enum struct WinReason { Eliminated, Crash };
enum struct Winner { Self, Enemy };
#elif BWAPI3
typedef enum { Eliminated, Crash } WinReason;
typedef enum { Self, Enemy } Winner;
#endif

struct TournamentModuleManager;
extern TournamentModuleManager tm;

#ifdef BWAPI3
BWAPI::Game *BWAPI::Broodwar;
#endif

#ifdef WIN32
#include "Windows.h"
// Assumes no unicode, ASCII only
inline std::string envvar(const char *var) {
  const unsigned long bufSize = (1 << 15) - 1;
  char buf[bufSize];
  const unsigned varSize = GetEnvironmentVariableA(var, buf, bufSize);
  return std::string(buf);
}
#endif

const char *boolname(bool b) { return b ? "true" : "false"; }

template <typename T>
inline void printVar(std::ofstream &os, const char *name, const T &var,
                     const char *postfix = "");

template <>
inline void printVar<std::string>(std::ofstream &os, const char *name,
                                  const std::string &var, const char *postfix) {
  os << "\t\"" << name << "\": \"" << var << "\"" << postfix << "\n";
}

template <typename T>
inline void printVar(std::ofstream &os, const char *name, const T &var,
                     const char *postfix) {
  os << "\t\"" << name << "\": " << var << postfix << "\n";
}

struct TournamentModuleManager {
#ifdef BWAPI4
  TournamentModuleManager() = default;
#elif BWAPI3
	TournamentModuleManager() {
	  wr = Crash;
	  winner = Enemy;
	  building_score = 0;
	  kill_score = 0;
	  razing_score = 0;
	  unit_score = 0;
	  lastFrameTimePoint = SteadyClock::now();
	  maxFrameTime = 0.0f;
	  frameTimeSum = 0.0f;
	}
#endif
#ifdef BWAPI4
  ~TournamentModuleManager() noexcept {
#elif BWAPI3
	~TournamentModuleManager() {
#endif
    try {
      writeResults();
    } catch (...) {
    }
  }
  void onReset() {
    TournamentModuleManager::TournamentModuleManager();
    frametimes.open(envvar("TM_LOG_FRAMETIMES").c_str());
    frametimes << "frame_count, frame_time_max, frame_time_avg\n";
  }

  template <typename Action>
  inline bool onAction(Action action, void *parameter) {
    switch (action) {
    case BWAPI::Tournament::ActionID::EnableFlag:
      switch (*(int *)parameter) {
      case BWAPI::Flag::CompleteMapInformation:
        return false;
      case BWAPI::Flag::UserInput:
        return false;
      default:
        return true;
      }
      break;

    case BWAPI::Tournament::LeaveGame:
    case BWAPI::Tournament::SetLatCom:
    case BWAPI::Tournament::SetTextSize:
    case BWAPI::Tournament::SendText:
    case BWAPI::Tournament::Printf:
    case BWAPI::Tournament::SetCommandOptimizationLevel:
      return true;
    default:
      return false;
    }
  }

  void writeResults() {
    std::ofstream of(envvar("TM_LOG_RESULTS").c_str());
    if (of.is_open()) {
      of << "{\n";
#ifdef BWAPI4
      printVar(of, "is_winner", boolname(winner == Winner::Self), ",");
      printVar(of, "is_crashed", boolname(wr == WinReason::Crash), ",");
#elif BWAPI3
      printVar(of, "is_winner", boolname(winner == Self), ",");
      printVar(of, "is_crashed", boolname(wr == Crash), ",");
#endif
      printVar(of, "building_score", building_score, ",");
      printVar(of, "kill_score", kill_score, ",");
      printVar(of, "razing_score", razing_score, ",");
      printVar(of, "unit_score", unit_score);
      of << "}";
    }
  }

  void onEnd(bool didWin) {
#ifdef BWAPI4
    wr = WinReason::Eliminated;
    winner = didWin ? Winner::Self : Winner::Enemy;
#elif BWAPI3
    wr = Eliminated;
    winner = didWin ? Self : Enemy;
#endif
  }

  void onFrame() {
    BWAPI::Player
#ifdef BWAPI3
        *
#endif
            self = BWAPI::Broodwar->self();

    building_score = self->getBuildingScore();
    kill_score = self->getKillScore();
    razing_score = self->getRazingScore();
    unit_score = self->getUnitScore();

    TimePoint now = SteadyClock::now();
    float lastFrameDuration = Duration(now - lastFrameTimePoint).count();
    lastFrameTimePoint = now;

    if (lastFrameDuration > maxFrameTime)
      maxFrameTime = lastFrameDuration;
    frameTimeSum += lastFrameDuration;

    if ((BWAPI::Broodwar->getFrameCount() % 20) == 19) {
      frametimes << BWAPI::Broodwar->getFrameCount() + 1 << ", " << maxFrameTime
                 << ", " << frameTimeSum / 20.f << "\n" << std::flush;
      frameTimeSum = .0f, maxFrameTime = .0f;
    }
  }

#ifdef BWAPI4
  WinReason wr = WinReason::Crash;
  Winner winner = Winner::Enemy;
  int building_score = 0;
  int kill_score = 0;
  int razing_score = 0;
  int unit_score = 0;
  std::ofstream frametimes;
  std::chrono::time_point<SteadyClock> lastFrameTimePoint = SteadyClock::now();
  float maxFrameTime = 0.0f;
  float frameTimeSum = 0.0f;
#elif BWAPI3
  WinReason wr;
  Winner winner;
  int building_score;
  int kill_score;
  int razing_score;
  int unit_score;
  std::ofstream frametimes;
  TimePoint lastFrameTimePoint;
  float maxFrameTime;
  float frameTimeSum;
#endif

  struct TournamentModule : public BWAPI::TournamentModule {
#ifdef BWAPI4
    virtual bool onAction(BWAPI::Tournament::ActionID action,
#elif BWAPI3
    virtual bool onAction(int action,
#endif
                                 void *parameter) override {
      return tm.onAction(action, parameter);
    }
    virtual void onFirstAdvertisement() override {}
  };

  TournamentModule *mod;

  struct TournamentBot : public BWAPI::AIModule {
    virtual void onStart() override { tm.onReset(); }
    virtual void onFrame() override { tm.onFrame(); }
    virtual void onEnd(bool didWin) override { tm.onEnd(didWin); }
  };

  TournamentBot *bot;
};
