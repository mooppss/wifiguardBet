#ifndef SESSIONSTATS_H
#define SESSIONSTATS_H

#include "Types.h"

void sessionStatsInit();
void sessionStatsOnScan(const ScanRecord& scan);
void sessionStatsGet(SessionStats& out);

#endif
