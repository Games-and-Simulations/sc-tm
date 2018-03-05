#pragma once

#include "BWAPI.h"

#ifdef BWAPI4
#include <chrono>
#elif BWAPI3
#include <boost/chrono.hpp>
#endif
#include <fstream>

#ifdef BWAPI4
enum struct WinReason { Eliminated, Crash };
enum struct Winner { Self, Enemy };
#define WR WinReason::
#define Wn Winner::
#define _NOEXCEPT noexcept
using SteadyClock = std::chrono::steady_clock;
using TimePoint = std::chrono::time_point<SteadyClock>;
using Duration = std::chrono::duration<float, std::milli>;
using BWAPIPlayer = BWAPI::Player;
using TournamentAction = BWAPI::Tournament::ActionID;
#elif BWAPI3
typedef enum { Eliminated, Crash } WinReason;
typedef enum { Self, Enemy } Winner;
#define WR 
#define Wn 
#define _NOEXCEPT
typedef boost::chrono::steady_clock SteadyClock;
typedef boost::chrono::time_point<SteadyClock> TimePoint;
typedef boost::chrono::duration<float, boost::milli> Duration;
typedef BWAPI::Player *BWAPIPlayer;
typedef int TournamentAction;
#endif

struct TournamentModuleManager;
extern TournamentModuleManager tm;

#ifdef WIN32
#include "Windows.h"
// Assumes no unicode, ASCII only
inline std::string Envvar(const char *var) {
  const unsigned long bufSize = (1 << 15) - 1;
  char buf[bufSize];
  const unsigned ignore = GetEnvironmentVariableA(var, buf, bufSize);
  return std::string(buf);
}
#endif

inline const char *BoolName(const bool b) { return b ? "true" : "false"; }

template <typename T>
void PrintVar(std::ofstream &os, const char *name, const T &var, const char *postfix = "");

template <>
inline void PrintVar<std::string>(std::ofstream &os, const char *name, const std::string &var, const char *postfix) {
  os << "\t\"" << name << "\": \"" << var << "\"" << postfix << "\n";
}

template <typename T>
void PrintVar(std::ofstream &os, const char *name, const T &var, const char *postfix) {
  os << "\t\"" << name << "\": " << var << postfix << "\n";
}

const std::string log_results_file = Envvar("TM_LOG_RESULTS");
const std::string log_frametimes_file = Envvar("TM_LOG_FRAMETIMES");
const bool allow_user_input = (Envvar("TM_ALLOW_USER_INPUT").compare("1") == 0);
const int speed_override = std::atoi(Envvar("TM_SPEED_OVERRIDE").c_str());

struct TournamentModuleManager {
  TournamentModuleManager() : win_reason(WR Crash), winner(Wn Enemy), num_actions(0), minerals_gathered(0), minerals_spent(0), gas_gathered(0), gas_spent(0) {

  }

  ~TournamentModuleManager() _NOEXCEPT {
    try {
      writeResults();
    }
    catch (...) {
    }
  }
  void onReset() {
    TournamentModuleManager();
    frametimes.open(log_frametimes_file.c_str());
    frametimes << "frame_count, frame_time_max, frame_time_avg, num_actions, minerals_gathered, minerals_spent, gas_gathered, gas_spent, supply_used, supply_total\n";
    BWAPI::Broodwar->setLocalSpeed(speed_override);
    lastFrameTimePoint = SteadyClock::now();
  }

  template <typename Action>
  static bool onAction(Action action, void *parameter) {
    switch (action) {
    case BWAPI::Tournament::EnableFlag:
      switch (*static_cast<int *>(parameter)) {
      case BWAPI::Flag::CompleteMapInformation:
        return false;
      case BWAPI::Flag::UserInput:
        return allow_user_input;
      default:
        return true;
      }

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

  void writeResults() const {
    std::ofstream of(log_results_file.c_str());
    if (of.is_open()) {
      of << "{\n";
      PrintVar(of, "is_winner", BoolName(winner == Wn Self), ",");
      PrintVar(of, "is_crashed", BoolName(win_reason == WR Crash), ",");
      PrintVar(of, "building_score", building_score, ",");
      PrintVar(of, "kill_score", kill_score, ",");
      PrintVar(of, "razing_score", razing_score, ",");
      PrintVar(of, "unit_score", unit_score);
      of << "}";
    }
  }

  void onEnd(bool didWin) {
    win_reason = WR Eliminated;
    winner = didWin ? Wn Self : Wn Enemy;
  }

  void onFrame() {
    BWAPIPlayer const self = BWAPI::Broodwar->self();

    building_score = self->getBuildingScore();
    kill_score = self->getKillScore();
    razing_score = self->getRazingScore();
    unit_score = self->getUnitScore();

    const TimePoint now = SteadyClock::now();
    const float lastFrameDuration = Duration(now - lastFrameTimePoint).count();
    lastFrameTimePoint = now;

    if (lastFrameDuration > maxFrameTime) {
      maxFrameTime = lastFrameDuration;
    }
    frameTimeSum += lastFrameDuration;

    // Check executed actions
    for(BWAPI::Unitset::iterator it = BWAPI::Broodwar->self()->getUnits().begin(); it != BWAPI::Broodwar->self()->getUnits().end(); ++ it) {
      int id = (*it)->getID();
      std::map<int, BWAPI::Position>::const_iterator it2 = lastCommandPosition.find(id);
      if (it2 != lastCommandPosition.end()) {
        if (lastCommandType[id] != (*it)->getOrder().getID() || (*it)->getTargetPosition() != (*it2).second || ((*it)->getTarget() && (*it)->getTarget()->getID() != lastCommandTarget[id])) {
          ++num_actions;
        }
      }

      lastCommandPosition[id] = (*it)->getTargetPosition();
      if((*it)->getTarget())
        lastCommandTarget[id] = (*it)->getTarget()->getID();
      lastCommandType[id] = (*it)->getOrder().getID();
    }

    if (BWAPI::Broodwar->getFrameCount() % 20 == 19) {
      // Write game data to file
      frametimes
        << BWAPI::Broodwar->getFrameCount() + 1 << ","
        << maxFrameTime << ","
        << frameTimeSum / 20.f << ", "
        << num_actions << ", "
        << BWAPI::Broodwar->self()->gatheredMinerals() - minerals_gathered << ", "
        << BWAPI::Broodwar->self()->spentMinerals() - minerals_spent << ", "
        << BWAPI::Broodwar->self()->gatheredGas() - gas_gathered << ", "
        << BWAPI::Broodwar->self()->spentGas() - gas_spent  << ", "
        << BWAPI::Broodwar->self()->supplyUsed() << ", "
        << BWAPI::Broodwar->self()->supplyTotal()
        << "\n" << std::flush;

      // Reset variables
      frameTimeSum = .0f, maxFrameTime = .0f;
      num_actions = 0;
      minerals_gathered = BWAPI::Broodwar->self()->gatheredMinerals();
      minerals_spent = BWAPI::Broodwar->self()->spentMinerals();
      gas_gathered = BWAPI::Broodwar->self()->gatheredGas();
      gas_spent = BWAPI::Broodwar->self()->spentGas();
    }
  }

  WinReason win_reason;
  Winner winner;
  int building_score;
  int kill_score;
  int razing_score;
  int unit_score;
  std::ofstream frametimes;
  TimePoint lastFrameTimePoint;
  float maxFrameTime;
  float frameTimeSum;
  int num_actions;
  int minerals_gathered;
  int minerals_spent;
  int gas_gathered;
  int gas_spent;
  std::map<int, BWAPI::Position> lastCommandPosition;
  std::map<int, int> lastCommandTarget;
  std::map<int, int> lastCommandType;

  struct TournamentModule : BWAPI::TournamentModule {
    bool onAction(TournamentAction action, void *parameter) override {
      return tm.onAction(action, parameter);
    }
    void onFirstAdvertisement() override {}
  };

  TournamentModule *mod;

  struct TournamentBot : BWAPI::AIModule {
    void onStart() override { tm.onReset(); }
    void onFrame() override { tm.onFrame(); }
    void onEnd(bool didWin) override { tm.onEnd(didWin); }
  };

  TournamentBot *bot;
};
