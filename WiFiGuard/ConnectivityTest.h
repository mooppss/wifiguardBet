#ifndef CONNECTIVITYTEST_H
#define CONNECTIVITYTEST_H

#include "Config.h"
#include "Types.h"

// Non-blocking connectivity test: associate, DNS, HTTP; portal detection and grading.
class ConnectivityTest {
public:
  void begin();
  void start(const char* ssid);  // open network only
  bool update();  // returns true when test complete
  void getResult(ConnectivityResult& out) const;
  bool isComplete() const { return phase_ == 4; }
  uint8_t getPhase() const { return phase_; }
  const char* getTestedSsid() const { return ssid_; }

private:
  void runPhase();
  void setResultFromState();

  uint8_t  phase_;  // 0=idle, 1=connect, 2=dns, 3=http, 33=fallback, 5=benchmark, 4=done
  bool     triedFallback_;
  char     ssid_[33];
  uint32_t phaseStart_;
  ConnectivityResult result_;
  int      benchmarkTimes_[BENCHMARK_PINGS];
  uint8_t  benchmarkIndex_;
};

extern ConnectivityTest connectivityTest;

#endif
