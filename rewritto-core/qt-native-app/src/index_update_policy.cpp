#include "index_update_policy.h"

bool shouldAutoUpdateIndex(const QDateTime& lastSuccessUtc,
                           const QDateTime& lastAttemptUtc,
                           const QDateTime& nowUtc,
                           int maxAgeHours,
                           int retryCooldownMinutes) {
  if (!nowUtc.isValid()) {
    return false;
  }

  if (lastAttemptUtc.isValid() && lastAttemptUtc <= nowUtc) {
    const qint64 secsSinceAttempt = lastAttemptUtc.secsTo(nowUtc);
    if (secsSinceAttempt < static_cast<qint64>(retryCooldownMinutes) * 60) {
      return false;
    }
  }

  if (!lastSuccessUtc.isValid()) {
    return true;
  }
  if (lastSuccessUtc > nowUtc) {
    return false;
  }

  const qint64 ageSecs = lastSuccessUtc.secsTo(nowUtc);
  return ageSecs >= static_cast<qint64>(maxAgeHours) * 3600;
}

