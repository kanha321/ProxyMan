#include "cli/stats.h"
#include "logging/data_tracker.h"

int HandleStats() {
    DataTracker::Instance().PrintSummaryReport();
    return 0;
}
