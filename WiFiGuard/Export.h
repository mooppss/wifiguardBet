#ifndef EXPORT_H
#define EXPORT_H

#include "Config.h"
#include "Types.h"

class Export {
public:
  void begin();
  void exportCurrentScanCSV(bool maskBssid = true);
  void exportSummaryOnly();
  void quickReportSummary();
  void exportHumanReadable();
#if FEATURE_EXPORT_JSON
  void exportCurrentScanJSON(bool maskBssid = true);
#endif
};

extern Export exportModule;

#endif
