#pragma once

#include <QDateTime>

bool shouldAutoUpdateIndex(const QDateTime& lastSuccessUtc,
                           const QDateTime& lastAttemptUtc,
                           const QDateTime& nowUtc,
                           int maxAgeHours = 24,
                           int retryCooldownMinutes = 10);

