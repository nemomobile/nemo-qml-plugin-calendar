#include <QObject>
#include <QtTest>

#include "calendarmanager.h"
#include <QSignalSpy>

class tst_NemoCalendarManager : public QObject
{
    Q_OBJECT

private slots:
    void test_isRangeLoaded_data();
    void test_isRangeLoaded();

private:
    NemoCalendarManager mManager;
};

void tst_NemoCalendarManager::test_isRangeLoaded_data()
{
    QTest::addColumn<QList<NemoCalendarData::Range> >("loadedRanges");
    QTest::addColumn<NemoCalendarData::Range>("testRange");
    QTest::addColumn<bool>("loaded");
    QTest::addColumn<QList<NemoCalendarData::Range> >("correctNewRanges");

    QDate march01(2014, 3, 1);
    QDate march02(2014, 3, 2);
    QDate march03(2014, 3, 3);
    QDate march04(2014, 3, 4);
    QDate march05(2014, 3, 5);
    QDate march06(2014, 3, 6);
    QDate march18(2014, 3, 18);
    QDate march19(2014, 3, 19);
    QDate march20(2014, 3, 20);
    QDate march21(2014, 3, 21);
    QDate march29(2014, 3, 29);
    QDate march30(2014, 3, 30);
    QDate march31(2014, 3, 31);
    QDate april17(2014, 4, 17);

    // Legend:
    // * |------|: time scale
    // * x: existing loaded period
    // * n: new period
    // * l: to be loaded
    //

    // Range loaded
    //    nnnnnn
    // |-xxxxxxxxxx------------|
    //    llllll
    QList<NemoCalendarData::Range> rangeList;
    rangeList << NemoCalendarData::Range(march01, march01)
              << NemoCalendarData::Range(march05, march05)
              << NemoCalendarData::Range(march31, march31)
              << NemoCalendarData::Range(march01, march02)
              << NemoCalendarData::Range(march19, march20)
              << NemoCalendarData::Range(march05, march20)
              << NemoCalendarData::Range(march02, march30)
              << NemoCalendarData::Range(march01, march31);

    QList<NemoCalendarData::Range> loadedRanges;
    loadedRanges.append(NemoCalendarData::Range(march01, march31));
    QList<NemoCalendarData::Range> correctNewRanges;
    foreach (const NemoCalendarData::Range &testRange, rangeList)
        QTest::newRow("Range loaded") << loadedRanges << testRange << true << correctNewRanges;

    // Range not loaded
    //    nnnnnn
    // |-----------------------|
    //    llllll
    loadedRanges.clear();
    foreach (const NemoCalendarData::Range &testRange, rangeList) {
        correctNewRanges.clear();
        correctNewRanges.append(testRange);
        QTest::newRow("Range not loaded") << loadedRanges << testRange << false << correctNewRanges;
    }

    // Range not loaded 2
    //     nnnnnn
    // |-xx------xxxx--x--|
    //     llllll
    loadedRanges.clear();
    loadedRanges.append(NemoCalendarData::Range(march01, march02));
    loadedRanges.append(NemoCalendarData::Range(march30, march31));
    loadedRanges.append(NemoCalendarData::Range(april17, april17));
    rangeList.clear();

    rangeList << NemoCalendarData::Range(march03, march05)
              << NemoCalendarData::Range(march03, march20)
              << NemoCalendarData::Range(march03, march29)
              << NemoCalendarData::Range(march05, march05)
              << NemoCalendarData::Range(march05, march21)
              << NemoCalendarData::Range(march05, march29);

    foreach (const NemoCalendarData::Range &testRange, rangeList) {
        correctNewRanges.clear();
        correctNewRanges.append(testRange);
        QTest::newRow("Range not loaded 2") << loadedRanges << testRange << false << correctNewRanges;
    }

    // Beginning missing
    //     nnnnnn
    // |-x-----xxxx--x--|
    //     llll
    loadedRanges.clear();
    loadedRanges.append(NemoCalendarData::Range(march01, march01));
    loadedRanges.append(NemoCalendarData::Range(march20, march31));
    loadedRanges.append(NemoCalendarData::Range(april17, april17));
    rangeList.clear();
    rangeList << NemoCalendarData::Range(march02, march20)
              << NemoCalendarData::Range(march05, march20)
              << NemoCalendarData::Range(march19, march20)
              << NemoCalendarData::Range(march02, march30)
              << NemoCalendarData::Range(march05, march30)
              << NemoCalendarData::Range(march19, march30)
              << NemoCalendarData::Range(march02, march31)
              << NemoCalendarData::Range(march05, march31)
              << NemoCalendarData::Range(march19, march31);

    foreach (const NemoCalendarData::Range &testRange, rangeList) {
        correctNewRanges.clear();
        correctNewRanges.append(NemoCalendarData::Range(testRange.first, march19));
        QTest::newRow("Beginning missing 1") << loadedRanges << testRange << false << correctNewRanges;
    }

    // End missing
    //      nnnnnn
    // |--xxxx--------|
    //        llll
    loadedRanges.clear();
    loadedRanges.append(NemoCalendarData::Range(march01, march19));
    rangeList.clear();
    rangeList << NemoCalendarData::Range(march01, march20)
              << NemoCalendarData::Range(march02, march20)
              << NemoCalendarData::Range(march05, march20)
              << NemoCalendarData::Range(march19, march20)
              << NemoCalendarData::Range(march20, march20)
              << NemoCalendarData::Range(march01, march30)
              << NemoCalendarData::Range(march02, march30)
              << NemoCalendarData::Range(march05, march30)
              << NemoCalendarData::Range(march19, march30)
              << NemoCalendarData::Range(march20, march30);
    foreach (const NemoCalendarData::Range &testRange, rangeList) {
        correctNewRanges.clear();
        correctNewRanges.append(NemoCalendarData::Range(march20, testRange.second));
        QTest::newRow("End missing 1") << loadedRanges << testRange << false << correctNewRanges;
    }

    // Middle missing
    //      nnnnnn
    // |--xxxx---xxx-----|
    //        lll
    loadedRanges.clear();
    loadedRanges.append(NemoCalendarData::Range(march01, march05));
    loadedRanges.append(NemoCalendarData::Range(march20, march31));
    rangeList.clear();
    rangeList << NemoCalendarData::Range(march01, march20)
              << NemoCalendarData::Range(march01, march30)
              << NemoCalendarData::Range(march01, march31)
              << NemoCalendarData::Range(march02, march20)
              << NemoCalendarData::Range(march02, march30)
              << NemoCalendarData::Range(march02, march31)
              << NemoCalendarData::Range(march05, march20)
              << NemoCalendarData::Range(march05, march30)
              << NemoCalendarData::Range(march05, march31);
    correctNewRanges.clear();
    correctNewRanges.append(NemoCalendarData::Range(march06, march19));
    foreach (const NemoCalendarData::Range &testRange, rangeList) {
        QTest::newRow("Middle missing") << loadedRanges << testRange << false << correctNewRanges;
    }

    // Two periods missing
    //      nnnnnnnnnn
    // |--xxxx---xxx-----|
    //        lll   ll
    loadedRanges.clear();
    loadedRanges.append(NemoCalendarData::Range(march01, march02));
    loadedRanges.append(NemoCalendarData::Range(march19, march20));
    rangeList.clear();
    rangeList << NemoCalendarData::Range(march01, march30)
              << NemoCalendarData::Range(march02, march30)
              << NemoCalendarData::Range(march03, march30);
    correctNewRanges.clear();
    correctNewRanges.append(NemoCalendarData::Range(march03, march18));
    correctNewRanges.append(NemoCalendarData::Range(march21, march30));
    foreach (const NemoCalendarData::Range &testRange, rangeList) {
        QTest::newRow("Two periods missing 1") << loadedRanges << testRange << false << correctNewRanges;
    }

    // Two periods missing
    //   nnnnnnnnnnnn
    // |----xxxx---xxxx----|
    //   lll    lll
    loadedRanges.clear();
    loadedRanges.append(NemoCalendarData::Range(march19, march20));
    loadedRanges.append(NemoCalendarData::Range(march30, march31));
    rangeList.clear();
    rangeList << NemoCalendarData::Range(march01, march30)
              << NemoCalendarData::Range(march01, march31)
              << NemoCalendarData::Range(march01, march29);
    correctNewRanges.clear();
    correctNewRanges.append(NemoCalendarData::Range(march01, march18));
    correctNewRanges.append(NemoCalendarData::Range(march21, march29));
    foreach (const NemoCalendarData::Range &testRange, rangeList) {
        QTest::newRow("Two periods missing 2") << loadedRanges << testRange << false << correctNewRanges;
    }

    // Two periods missing
    //   nnnnnnnnnn
    // |----xxxx-------|
    //   lll    lll
    loadedRanges.clear();
    loadedRanges.append(NemoCalendarData::Range(march19, march20));
    rangeList.clear();
    rangeList << NemoCalendarData::Range(march01, march29);
    correctNewRanges.clear();
    correctNewRanges.append(NemoCalendarData::Range(march01, march18));
    correctNewRanges.append(NemoCalendarData::Range(march21, march29));
    foreach (const NemoCalendarData::Range &testRange, rangeList) {
        QTest::newRow("Two periods missing 3") << loadedRanges << testRange << false << correctNewRanges;
    }

    // Two periods missing
    //     nnnnnn
    // |----xxxx-------|
    //     l    l
    loadedRanges.clear();
    loadedRanges.append(NemoCalendarData::Range(march19, march20));
    rangeList.clear();
    rangeList << NemoCalendarData::Range(march18, march21);
    correctNewRanges.clear();
    correctNewRanges.append(NemoCalendarData::Range(march18, march18));
    correctNewRanges.append(NemoCalendarData::Range(march21, march21));
    foreach (const NemoCalendarData::Range &testRange, rangeList) {
        QTest::newRow("Two periods missing 4") << loadedRanges << testRange << false << correctNewRanges;
    }

    // Three periods missing
    //   nnnnnnnnnnnnnnnn
    // |----xxxx---xxxx----|
    //   lll    lll    ll
    loadedRanges.clear();
    loadedRanges.append(NemoCalendarData::Range(march05, march05));
    loadedRanges.append(NemoCalendarData::Range(march19, march20));
    rangeList.clear();
    rangeList << NemoCalendarData::Range(march01, march31);
    correctNewRanges.clear();
    correctNewRanges.append(NemoCalendarData::Range(march01, march04));
    correctNewRanges.append(NemoCalendarData::Range(march06, march18));
    correctNewRanges.append(NemoCalendarData::Range(march21, march31));
    foreach (const NemoCalendarData::Range &testRange, rangeList) {
        QTest::newRow("Three periods missing 1") << loadedRanges << testRange << false << correctNewRanges;
    }

    // Three periods missing
    //   nnnnnnnnnnnnnnnnnnn
    // |-xx---xxxx---xxxx--xx--|
    //     lll    lll    ll
    loadedRanges.clear();
    loadedRanges.append(NemoCalendarData::Range(march01, march02));
    loadedRanges.append(NemoCalendarData::Range(march05, march05));
    loadedRanges.append(NemoCalendarData::Range(march19, march20));
    loadedRanges.append(NemoCalendarData::Range(march30, march31));
    rangeList.clear();
    rangeList << NemoCalendarData::Range(march01, march31);
    rangeList << NemoCalendarData::Range(march02, march31);
    rangeList << NemoCalendarData::Range(march03, march31);
    rangeList << NemoCalendarData::Range(march01, march30);
    rangeList << NemoCalendarData::Range(march02, march30);
    rangeList << NemoCalendarData::Range(march03, march30);
    rangeList << NemoCalendarData::Range(march01, march29);
    rangeList << NemoCalendarData::Range(march02, march29);
    rangeList << NemoCalendarData::Range(march03, march29);
    correctNewRanges.clear();
    correctNewRanges.append(NemoCalendarData::Range(march03, march04));
    correctNewRanges.append(NemoCalendarData::Range(march06, march18));
    correctNewRanges.append(NemoCalendarData::Range(march21, march29));
    foreach (const NemoCalendarData::Range &testRange, rangeList) {
        QTest::newRow("Three periods missing 2") << loadedRanges << testRange << false << correctNewRanges;
    }

    // Five periods missing
    //   nnnnnnnnnnnnnnnnnnnnnnnnnnnn
    // |-xx---xxxx---xxxx--xx--x---x--|
    //     lll    lll    ll  ll lll
    loadedRanges.clear();
    loadedRanges.append(NemoCalendarData::Range(march01, march01));
    loadedRanges.append(NemoCalendarData::Range(march03, march03));
    loadedRanges.append(NemoCalendarData::Range(march05, march05));
    loadedRanges.append(NemoCalendarData::Range(march19, march20));
    loadedRanges.append(NemoCalendarData::Range(march30, march30));
    rangeList.clear();
    rangeList << NemoCalendarData::Range(march01, april17);
    correctNewRanges.clear();
    correctNewRanges.append(NemoCalendarData::Range(march02, march02));
    correctNewRanges.append(NemoCalendarData::Range(march04, march04));
    correctNewRanges.append(NemoCalendarData::Range(march06, march18));
    correctNewRanges.append(NemoCalendarData::Range(march21, march29));
    correctNewRanges.append(NemoCalendarData::Range(march31, april17));
    foreach (const NemoCalendarData::Range &testRange, rangeList) {
        QTest::newRow("Five periods missing") << loadedRanges << testRange << false << correctNewRanges;
    }
}

void tst_NemoCalendarManager::test_isRangeLoaded()
{
    QFETCH(QList<NemoCalendarData::Range>, loadedRanges);
    QFETCH(NemoCalendarData::Range, testRange);
    QFETCH(bool, loaded);
    QFETCH(QList<NemoCalendarData::Range>, correctNewRanges);

    mManager.mLoadedRanges = loadedRanges;
    QList<NemoCalendarData::Range> newRanges;
    bool result = mManager.isRangeLoaded(testRange, &newRanges);

    QCOMPARE(result, loaded);
    QCOMPARE(newRanges.count(), correctNewRanges.count());

    foreach (const NemoCalendarData::Range &range, newRanges)
        QVERIFY(correctNewRanges.contains(range));
}

#include "tst_calendarmanager.moc"
QTEST_MAIN(tst_NemoCalendarManager)
