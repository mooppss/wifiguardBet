#ifndef UI_H
#define UI_H

#include "Config.h"
#include "Types.h"

class UI {
public:
  void begin();
  void setDirty() { dirty_ = true; }
  void draw();
  void setView(UIView v) { view_ = v; dirty_ = true; }
  UIView getView() const { return view_; }
  void setListIndex(int i) { listIndex_ = i; dirty_ = true; }
  int getListIndex() const { return listIndex_; }
  void setListScroll(int s) { listScroll_ = s; dirty_ = true; }
  int getListScroll() const { return listScroll_; }
  int getFilteredCount() const { return filteredCount_; }
  uint16_t getSortedIndices(uint16_t* out, int maxLen) const;
  const NetworkRecord* getNetworkAtDisplayIndex(int i) const;
  int getSettingsOptionIndex() const { return settingsOptionIndex_; }
  void setSettingsOptionIndex(int i) { settingsOptionIndex_ = i; dirty_ = true; }
  void setHighRiskAlert(bool v) { highRiskAlert_ = v; dirty_ = true; }
  bool hasHighRiskAlert() const { return highRiskAlert_; }
  void setPjShowQR(bool v) { pjShowQR_ = v; dirty_ = true; }
  void setToast(const char* msg);
  void clearToast();
  void cycleEnvPage() { envPage_ = (envPage_ + 1) % 2; dirty_ = true; }
  uint8_t getEnvPage() const { return envPage_; }

private:
  void drawHeader(const char* title);
  void drawFooter(const char* hints);
  void drawIdle();
  void drawScanning();
  void drawListSimple();
  void drawListExpert();
  void drawDetailSimple();
  void drawDetailExpert();
  void drawEnvSummarySimple();
  void drawEnvSummaryExpert();
  void drawSessionSimple();
  void drawTestingSimple();
  void drawTestingExpert();
  void drawProtectedJoin();
  void drawSettings();
  void drawHelp();
  void drawExport();
  void drawSleep();
  void drawStabilityMonitor();
  void drawDebug();
  void drawPortalAssist(int x, int y, const char* portalUrl);
  void drawPortalSuspiciousWarning();
  void drawQRScreen();
  void drawQRCode(int x, int y, int moduleSizePx, const char* url);
  void drawSignalBars(int x, int y, int8_t rssi);
  void applySortFilter();
  const char* authStr(AuthType a) const;
  const char* gradeStr(ConnectivityGrade g) const;
  const char* portalStr(PortalResult p) const;

  bool    dirty_;
  bool    pjShowQR_;
  UIView  view_;
  int     listIndex_;
  int     listScroll_;
  uint16_t sortedIndices_[MAX_NETWORKS];
  int     filteredCount_;
  uint32_t lastRedraw_;
  int     settingsOptionIndex_;
  bool    highRiskAlert_;
  char    toast_[48];
  uint32_t toastUntil_;
  uint8_t  envPage_;
  DeviceState lastDrawnState_;
  UIView      lastDrawnView_;
};

extern UI ui;

#endif
