/// @file TestLogSharedMemory.cpp
///
/// Characterisation tests for the shared memory output of the log. The segment
/// is named, so its four byte header is written by whoever can open it: it is
/// untrusted input that the module uses as a length and as an offset.

#include <QTest>

#include "vip_test_main.h"

#include "VipLogging.h"

#include <QSharedMemory>
#include <cstring>
#include <limits>

class TestLogSharedMemory : public QObject
{
	Q_OBJECT

	// The key of the segment when no file logger is given, which is this case.
	static const char* key() { return "Log"; }

	/// Writes a header straight into the segment, the way another process would.
	static bool forgeHeader(qint32 size)
	{
		QSharedMemory memory(QString::fromLatin1(key()));
		if (!memory.attach())
			return false;
		bool ok = false;
		if (memory.lock()) {
			memcpy(memory.data(), &size, sizeof(qint32));
			memory.unlock();
			ok = true;
		}
		memory.detach();
		return ok;
	}

private Q_SLOTS:

	void initTestCase() { QVERIFY(VipLogging::instance().open(VipLogging::SharedMemory)); }

	void cleanupTestCase() { VipLogging::instance().close(); }

	/// An ordinary round trip still works.
	void anEntryComesBackFromTheSegment()
	{
		VipLogging::instance().directLog("first entry", VipLogging::Info);
		const QStringList entries = VipLogging::instance().lastLogEntries();
		QVERIFY2(!entries.isEmpty(), "an entry written to the segment must come back");
		QVERIFY(entries.join("\n").contains("first entry"));
	}

	/// A header longer than the segment used to be the length of the read.
	void aHeaderLongerThanTheSegmentIsRefused()
	{
		QVERIFY(forgeHeader(std::numeric_limits<qint32>::max()));
		QCOMPARE(VipLogging::instance().lastLogEntries(), QStringList());
	}

	/// And a negative one reached the byte array as a length too.
	void aNegativeHeaderIsRefused()
	{
		QVERIFY(forgeHeader(-1));
		QCOMPARE(VipLogging::instance().lastLogEntries(), QStringList());
	}

	/// On the writing side the header is the destination offset, and only the sum
	/// was tested: a value near the maximum wraps negative and passes that test,
	/// which puts the copy before the start of the segment.
	void aHeaderNearTheMaximumDoesNotMoveTheWriteBackwards()
	{
		QVERIFY(forgeHeader(std::numeric_limits<qint32>::max() - 8));
		VipLogging::instance().directLog("after a forged header", VipLogging::Info);

		// The forged header is dropped, so what comes back is the entry alone.
		const QStringList entries = VipLogging::instance().lastLogEntries();
		QVERIFY(entries.join("\n").contains("after a forged header"));
	}
};

VIP_TEST_MAIN(TestLogSharedMemory)
#include "TestLogSharedMemory.moc"
