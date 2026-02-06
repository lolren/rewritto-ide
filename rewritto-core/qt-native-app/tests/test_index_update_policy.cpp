#include <QtTest/QtTest>

#include <QDateTime>

#include "index_update_policy.h"

class TestIndexUpdatePolicy final : public QObject {
  Q_OBJECT

 private slots:
  void updatesWhenNeverSucceeded();
  void doesNotUpdateWhenRecentSuccess();
  void updatesWhenSuccessIsOld();
  void throttlesAfterRecentAttempt();
};

void TestIndexUpdatePolicy::updatesWhenNeverSucceeded() {
  const QDateTime nowUtc = QDateTime::fromString("2026-01-30T12:00:00Z", Qt::ISODate);
  QVERIFY(nowUtc.isValid());
  QVERIFY(shouldAutoUpdateIndex(QDateTime{}, QDateTime{}, nowUtc, 24, 10));
}

void TestIndexUpdatePolicy::doesNotUpdateWhenRecentSuccess() {
  const QDateTime nowUtc = QDateTime::fromString("2026-01-30T12:00:00Z", Qt::ISODate);
  const QDateTime lastSuccess = QDateTime::fromString("2026-01-30T06:00:00Z", Qt::ISODate);
  QVERIFY(nowUtc.isValid());
  QVERIFY(lastSuccess.isValid());
  QVERIFY(!shouldAutoUpdateIndex(lastSuccess, QDateTime{}, nowUtc, 24, 10));
}

void TestIndexUpdatePolicy::updatesWhenSuccessIsOld() {
  const QDateTime nowUtc = QDateTime::fromString("2026-01-30T12:00:00Z", Qt::ISODate);
  const QDateTime lastSuccess = QDateTime::fromString("2026-01-28T11:00:00Z", Qt::ISODate);
  QVERIFY(nowUtc.isValid());
  QVERIFY(lastSuccess.isValid());
  QVERIFY(shouldAutoUpdateIndex(lastSuccess, QDateTime{}, nowUtc, 24, 10));
}

void TestIndexUpdatePolicy::throttlesAfterRecentAttempt() {
  const QDateTime nowUtc = QDateTime::fromString("2026-01-30T12:00:00Z", Qt::ISODate);
  const QDateTime lastSuccess = QDateTime::fromString("2026-01-28T11:00:00Z", Qt::ISODate);
  const QDateTime lastAttempt = QDateTime::fromString("2026-01-30T11:55:00Z", Qt::ISODate);
  QVERIFY(nowUtc.isValid());
  QVERIFY(lastSuccess.isValid());
  QVERIFY(lastAttempt.isValid());
  QVERIFY(!shouldAutoUpdateIndex(lastSuccess, lastAttempt, nowUtc, 24, 10));
}

QTEST_MAIN(TestIndexUpdatePolicy)

#include "test_index_update_policy.moc"

