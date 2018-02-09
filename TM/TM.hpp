#pragma once

#include "BWAPI.h"

#include <chrono>
#include <fstream>

enum struct WinReason { Eliminated, Crash };
enum struct Winner { Self, Enemy };

struct TournamentModuleManager;
extern TournamentModuleManager tm;

#ifdef BWAPI3
BWAPI::Game *BWAPI::Broodwar;
#endif

#ifdef WIN32
#include "Windows.h"
// Assumes no unicode, ASCII only
inline std::string envvar(const char *var) {
  constexpr unsigned long bufSize = (1 << 15) - 1;
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
  TournamentModuleManager() = default;
  ~TournamentModuleManager() noexcept {
    try {
      writeResults();
    } catch (...) {
    }
  }
  void onReset() {
    TournamentModuleManager::TournamentModuleManager();
    frametimes.open(envvar("TM_LOG_FRAMETIMES"));
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
    std::ofstream of(envvar("TM_LOG_RESULTS"));
    if (of.is_open()) {
      of << "{\n";
      printVar(of, "is_winner", boolname(winner == Winner::Self), ",");
      printVar(of, "is_crashed", boolname(wr == WinReason::Crash), ",");
      printVar(of, "building_score", building_score, ",");
      printVar(of, "kill_score", kill_score, ",");
      printVar(of, "razing_score", razing_score, ",");
      printVar(of, "unit_score", unit_score);
      of << "}";
    }
  }

  void onEnd(bool didWin) {
    wr = WinReason::Eliminated;
    winner = didWin ? Winner::Self : Winner::Enemy;
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

    std::chrono::time_point<std::chrono::steady_clock> now =
        std::chrono::steady_clock::now();
    float lastFrameDuration =
        std::chrono::duration<float, std::milli>(now - lastFrameTimePoint)
            .count();
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

  WinReason wr = WinReason::Crash;
  Winner winner = Winner::Enemy;
  int building_score = 0;
  int kill_score = 0;
  int razing_score = 0;
  int unit_score = 0;
  std::ofstream frametimes;
  std::chrono::time_point<std::chrono::steady_clock> lastFrameTimePoint =
      std::chrono::steady_clock::now();
  float maxFrameTime = 0.0f;
  float frameTimeSum = 0.0f;

  struct TournamentModule : public BWAPI::TournamentModule {
#ifdef BWAPI4
    inline virtual bool onAction(BWAPI::Tournament::ActionID action,
#elif BWAPI3
    inline virtual bool onAction(int action,
#endif
                                 void *parameter) override {
      return tm.onAction(action, parameter);
    }
    inline virtual void onFirstAdvertisement() override {}
  };

  TournamentModule *mod;

  struct TournamentBot : public BWAPI::AIModule {
    inline virtual void onStart() override { tm.onReset(); }
    inline virtual void onFrame() override { tm.onFrame(); }
    inline virtual void onEnd(bool didWin) override { tm.onEnd(didWin); }
  };

  TournamentBot *bot;
};
