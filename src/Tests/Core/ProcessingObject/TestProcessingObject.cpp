/// @file TestProcessingObject.cpp
///
/// Characterisation tests for the scheduling pivot of the SDK. They capture
/// CURRENT behaviour, flaws included: the point is to lock down boundaries so
/// that any change to these surfaces shows up, not to validate a specification.

#include <QTest>

#include "vip_test_main.h"

#include "VipProcessingObject.h"
#include "VipImageProcessing.h"
#include "VipStandardProcessing.h"
#include "VipStreamingFromDevice.h"
#include "VipXmlArchive.h"

#include <atomic>
#include <functional>
#include <memory>
#include <QElapsedTimer>
#include <QThread>

#ifdef _WIN32
#include <windows.h>
#else
#include <ctime>
#endif

/// Processor time charged to the calling thread, in milliseconds. A wait that
/// sleeps leaves it flat; a wait that spins makes it follow the clock.
static qint64 processorMilliseconds()
{
#ifdef _WIN32
	FILETIME creation, exited, kernel, user;
	if (!GetThreadTimes(GetCurrentThread(), &creation, &exited, &kernel, &user))
		return 0;
	const auto toMs = [](const FILETIME& f) { return ((static_cast<qint64>(f.dwHighDateTime) << 32) | f.dwLowDateTime) / 10000; };
	return toMs(kernel) + toMs(user);
#else
	timespec ts;
	if (clock_gettime(CLOCK_THREAD_CPUTIME_ID, &ts) != 0)
		return 0;
	return static_cast<qint64>(ts.tv_sec) * 1000 + ts.tv_nsec / 1000000;
#endif
}

// ---------------------------------------------------------------------------
// Test processings, kept to the strict minimum.
// ---------------------------------------------------------------------------

/// Multiplies its input by a property. Counts its invocations, which makes the
/// scheduling observable.
class MultiplyByProperty : public VipProcessingObject
{
	Q_OBJECT
	VIP_IO(VipInput input)
	VIP_IO(VipOutput output)
	VIP_IO(VipProperty factor)

public:
	std::atomic<int> applyCount{ 0 };

	MultiplyByProperty(QObject* parent = nullptr)
	  : VipProcessingObject(parent)
	{
		propertyAt(0)->setData(2.0);
	}

protected:
	void apply() override
	{
		const double in = inputAt(0)->data().value<double>();
		const double f = propertyAt(0)->value<double>();
		outputAt(0)->setData(create(QVariant(in * f)));
		++applyCount;
	}
};

/// Adds a constant. Used to compose a VipProcessingList.
class AddOne : public VipProcessingObject
{
	Q_OBJECT
	VIP_IO(VipInput input)
	VIP_IO(VipOutput output)

public:
	AddOne(QObject* parent = nullptr)
	  : VipProcessingObject(parent)
	{
	}

protected:
	void apply() override { outputAt(0)->setData(create(QVariant(inputAt(0)->data().value<double>() + 1.0))); }
};

/// Takes a long time and no processor while doing so.
class SlowProcessing : public VipProcessingObject
{
	Q_OBJECT
	VIP_IO(VipInput input)
	VIP_IO(VipOutput output)

public:
	SlowProcessing(QObject* parent = nullptr)
	  : VipProcessingObject(parent)
	{
	}

protected:
	void apply() override
	{
		QThread::msleep(400);
		outputAt(0)->setData(create(inputAt(0)->data().data()));
	}
};

/// Reports when the list propagates its source properties to it. The hook is
/// the shortest reimplemented virtual the list calls while it inserts.
class WatchingProcessing : public VipProcessingObject
{
	Q_OBJECT
	VIP_IO(VipInput input)
	VIP_IO(VipOutput output)

public:
	std::function<void()> onSourceProperty;

	WatchingProcessing(QObject* parent = nullptr)
	  : VipProcessingObject(parent)
	{
	}

	void setSourceProperty(const char* name, const QVariant& value) override
	{
		if (onSourceProperty)
			onSourceProperty();
		VipProcessingObject::setSourceProperty(name, value);
	}

protected:
	void apply() override { outputAt(0)->setData(create(inputAt(0)->data().data())); }
};

/// Carries a multi input, so the container operations can be exercised.
class MultiInputProcessing : public VipProcessingObject
{
	Q_OBJECT
	VIP_IO(VipMultiInput inputs)
	VIP_IO(VipOutput output)

public:
	MultiInputProcessing(QObject* parent = nullptr)
	  : VipProcessingObject(parent)
	{
	}

protected:
	void apply() override {}
};

// ---------------------------------------------------------------------------

class TestProcessingObject : public QObject
{
	Q_OBJECT

	static VipAnyData makeData(double v)
	{
		return VipAnyData(QVariant(v), 0);
	}

private Q_SLOTS:

	// -- Input buffer boundaries --------------------------------------------

	/// A fresh buffer is empty and status() is -1 until something is pushed.
	void dataListEmptyBeforeFirstPush()
	{
		const VipDataList::Type types[] = { VipDataList::FIFO, VipDataList::LIFO, VipDataList::LastAvailable };
		for (VipDataList::Type type : types) {
			MultiplyByProperty proc;
			proc.inputAt(0)->setListType(type, VipDataList::None);
			QVERIFY2(proc.inputAt(0)->empty(), "a fresh buffer must be empty");
			QCOMPARE(proc.inputAt(0)->status(), -1);
			QVERIFY(!proc.inputAt(0)->hasNewData());
		}
	}

	/// After a single push the buffer is no longer empty and data is available,
	/// whatever the list type.
	void dataListSingleElement()
	{
		const VipDataList::Type types[] = { VipDataList::FIFO, VipDataList::LIFO, VipDataList::LastAvailable };
		for (VipDataList::Type type : types) {
			MultiplyByProperty proc;
			VipInput* in = proc.inputAt(0);
			in->setListType(type, VipDataList::None);
			in->setData(makeData(7.0));

			QVERIFY(!in->empty());
			QVERIFY(in->hasNewData());
			QCOMPARE(in->data().value<double>(), 7.0);
		}
	}

	/// Ordering, measured on the three VipDataList implementations DIRECTLY
	/// rather than through VipInput, which replaces instead of queueing while
	/// the processing is not asynchronous (see the dedicated test below).
	void dataListOrderingPerType()
	{
		{
			VipFIFOList list;
			list.setListLimitType(VipDataList::None);
			list.push(makeData(1.0));
			list.push(makeData(2.0));
			list.push(makeData(3.0));
			QCOMPARE(list.next().value<double>(), 1.0);
		}
		{
			VipLIFOList list;
			list.setListLimitType(VipDataList::None);
			list.push(makeData(1.0));
			list.push(makeData(2.0));
			list.push(makeData(3.0));
			QCOMPARE(list.next().value<double>(), 3.0);
		}
		{
			VipLastAvailableList list;
			list.push(makeData(1.0));
			list.push(makeData(2.0));
			list.push(makeData(3.0));
			QCOMPARE(list.next().value<double>(), 3.0);
		}
	}

	/// A FIFO drains in insertion order until exhausted.
	void fifoDrainsInInsertionOrder()
	{
		VipFIFOList list;
		list.setListLimitType(VipDataList::None);
		for (int i = 0; i < 5; ++i)
			list.push(makeData(i));
		QCOMPARE(list.remaining(), 5);

		for (int i = 0; i < 5; ++i)
			QCOMPARE(list.next().value<double>(), static_cast<double>(i));
		QCOMPARE(list.remaining(), 0);
	}

	/// LastAvailable always returns the last data, even when read repeatedly:
	/// this is what tells it apart from a FIFO of size one.
	void lastAvailableKeepsReturningLastData()
	{
		VipLastAvailableList list;
		list.push(makeData(5.0));

		QCOMPARE(list.next().value<double>(), 5.0);
		QCOMPARE(list.next().value<double>(), 5.0);
		QCOMPARE(list.next().value<double>(), 5.0);
	}

	/// A fresh list is empty and status() is -1 until something is pushed.
	void dataListEmptyBeforeFirstPushDirect()
	{
		{
			VipFIFOList list;
			QVERIFY(list.empty());
			QCOMPARE(list.status(), -1);
			QVERIFY(!list.hasNewData());
			QCOMPARE(list.remaining(), 0);
		}
		{
			VipLIFOList list;
			QVERIFY(list.empty());
			QCOMPARE(list.status(), -1);
			QVERIFY(!list.hasNewData());
		}
		{
			VipLastAvailableList list;
			QVERIFY(list.empty());
			QCOMPARE(list.status(), -1);
			QVERIFY(!list.hasNewData());
		}
	}

	/// Count limit on a FIFO: push past the maximum and check the invariant,
	/// namely that the oldest data is what gets dropped.
	void fifoNumberLimitCaps()
	{
		VipFIFOList list;
		list.setListLimitType(VipDataList::Number);
		list.setMaxListSize(4);
		QCOMPARE(list.maxListSize(), 4);

		for (int i = 0; i < 7; ++i)
			list.push(makeData(i));

		const int remaining = list.remaining();
		QVERIFY2(remaining <= 4, qPrintable(QStringLiteral("count limit not honoured: %1 left for a maximum of 4").arg(remaining)));
		QVERIFY(remaining > 0);
		QVERIFY2(list.next().value<double>() > 0.0, "the limit must drop the oldest data");
	}

	/// Same limit on a LIFO, where it is implemented the other way round. Only
	/// the shared invariant is pinned here.
	void lifoNumberLimitCaps()
	{
		VipLIFOList list;
		list.setListLimitType(VipDataList::Number);
		list.setMaxListSize(4);

		for (int i = 0; i < 7; ++i)
			list.push(makeData(i));

		const int remaining = list.remaining();
		QVERIFY2(remaining <= 4, qPrintable(QStringLiteral("count limit not honoured on LIFO: %1 left for a maximum of 4").arg(remaining)));
		QVERIFY(remaining > 0);
	}

	/// clear() drops pending data.
	void clearRemovesPendingData()
	{
		VipFIFOList list;
		list.setListLimitType(VipDataList::None);
		list.push(makeData(1.0));
		list.push(makeData(2.0));
		QCOMPARE(list.remaining(), 2);

		list.clear();
		QCOMPARE(list.remaining(), 0);
		QVERIFY(!list.hasNewData());
	}

	/// In synchronous mode, the default, VipInput::setData REPLACES the buffer
	/// content instead of queueing, whatever the list type. This is intended and
	/// commented in the code, but it was written nowhere else, and it makes any
	/// list type setting inert until the processing becomes asynchronous.
	void inputInSynchronousModeReplacesInsteadOfQueueing()
	{
		const VipDataList::Type types[] = { VipDataList::FIFO, VipDataList::LIFO, VipDataList::LastAvailable };
		for (VipDataList::Type type : types) {
			MultiplyByProperty proc;
			QVERIFY2(!(proc.scheduleStrategies() & VipProcessingObject::Asynchronous), "the default mode must be synchronous");

			VipInput* in = proc.inputAt(0);
			in->setListType(type, VipDataList::None);
			in->setData(makeData(1.0));
			in->setData(makeData(2.0));
			in->setData(makeData(3.0));

			QCOMPARE(in->buffer()->remaining(), 1);
			QCOMPARE(in->data().value<double>(), 3.0);
		}
	}

	// -- Synchronous pipeline -----------------------------------------------

	/// One data in, apply() runs once, the output carries the expected result.
	void synchronousUpdateAppliesOnce()
	{
		MultiplyByProperty proc;
		proc.setScheduleStrategy(VipProcessingObject::Asynchronous, false);
		proc.propertyAt(0)->setData(3.0);
		proc.inputAt(0)->setData(makeData(4.0));

		proc.update();

		QCOMPARE(proc.applyCount.load(), 1);
		QCOMPARE(proc.outputAt(0)->data().value<double>(), 12.0);
	}

	/// The property is read on every application, not captured once.
	void propertyChangeIsHonoured()
	{
		MultiplyByProperty proc;
		proc.setScheduleStrategy(VipProcessingObject::Asynchronous, false);

		proc.propertyAt(0)->setData(2.0);
		proc.inputAt(0)->setData(makeData(10.0));
		proc.update();
		QCOMPARE(proc.outputAt(0)->data().value<double>(), 20.0);

		proc.propertyAt(0)->setData(5.0);
		proc.inputAt(0)->setData(makeData(10.0));
		proc.update();
		QCOMPARE(proc.outputAt(0)->data().value<double>(), 50.0);
	}

	/// A disabled processing must not apply.
	void disabledProcessingDoesNotApply()
	{
		MultiplyByProperty proc;
		proc.setScheduleStrategy(VipProcessingObject::Asynchronous, false);
		proc.setEnabled(false);
		proc.inputAt(0)->setData(makeData(4.0));
		proc.update();

		QCOMPARE(proc.applyCount.load(), 0);
	}

	// -- Asynchronous scheduling and thread pool ----------------------------

	/// Data is queued, wait() returns once the queue is drained, and the apply
	/// count is exact. This is the only test exercising run() and the pool.
	void asynchronousSchedulingConsumesEveryInput()
	{
		MultiplyByProperty proc;
		proc.setComputeTimeStatistics(false);
		proc.inputAt(0)->setListType(VipDataList::FIFO, VipDataList::None);
		proc.setScheduleStrategy(VipProcessingObject::Asynchronous, true);

		const int count = 200;
		VipInput* in = proc.inputAt(0);
		for (int i = 0; i < count; ++i)
			in->setData(makeData(1.0));

		// wait(bool wait_for_sources, int max_milli_time): the first parameter is
		// a boolean, not a timeout. Passing 30000 directly truncates to true and
		// leaves the timeout at -1, that is an unbounded wait.
		QVERIFY2(proc.wait(true, 30000), "wait() must return once the queue is drained");
		QCOMPARE(proc.applyCount.load(), count);
	}

	// -- Session format round trip ------------------------------------------

	/// copy() serialises then reads the object back through the session format.
	/// VipClamp is used because it is actually registered with the type system,
	/// so this exercises the real path.
	void copyPreservesTypeAndProperties()
	{
		VipClamp proc;
		proc.propertyAt(0)->setData(-1.5);
		proc.propertyAt(1)->setData(7.25);

		VipProcessingObject* clone = proc.copy();
		QVERIFY2(clone != nullptr, "copy() must not return nullptr for a registered type");
		const std::unique_ptr<VipProcessingObject> guard(clone);

		QVERIFY2(qobject_cast<VipClamp*>(clone) != nullptr, "the copy must carry the concrete type");
		QCOMPARE(clone->propertyCount(), proc.propertyCount());
		QCOMPARE(clone->inputCount(), proc.inputCount());
		QCOMPARE(clone->outputCount(), proc.outputCount());
		QCOMPARE(clone->propertyAt(0)->value<double>(), -1.5);
		QCOMPARE(clone->propertyAt(1)->value<double>(), 7.25);
	}

	/// The copy is independent from the source.
	void copyIsIndependentFromSource()
	{
		VipClamp proc;
		proc.propertyAt(0)->setData(1.0);

		VipProcessingObject* clone = proc.copy();
		QVERIFY(clone != nullptr);
		const std::unique_ptr<VipProcessingObject> guard(clone);

		clone->propertyAt(0)->setData(99.0);
		QCOMPARE(proc.propertyAt(0)->value<double>(), 1.0);
	}

	/// Counterpart: a processing NOT registered with the type system cannot be
	/// read back and copy() returns nullptr, silently, with no error and no log.
	/// Pinned as is, because a plugin forgetting the registration macro gets
	/// exactly that today.
	void copyOfUnregisteredTypeReturnsNull()
	{
		MultiplyByProperty proc;
		proc.propertyAt(0)->setData(6.5);

		VipProcessingObject* clone = proc.copy();
		const std::unique_ptr<VipProcessingObject> guard(clone);
		QVERIFY2(clone == nullptr, "an unregistered type cannot be rebuilt by the factory");
	}

	/// The three offset processings are rebuildable by name. Regression guard:
	/// two of them were missing from the type registry, so copy() returned
	/// nullptr on them.
	void offsetProcessingsAreRegistered()
	{
		{
			VipStartAtZero p;
			const std::unique_ptr<VipProcessingObject> c(p.copy());
			QVERIFY2(c != nullptr, "VipStartAtZero must be rebuildable by name");
		}
		{
			VipStartYAtZero p;
			const std::unique_ptr<VipProcessingObject> c(p.copy());
			QVERIFY2(c != nullptr, "VipStartYAtZero must be rebuildable by name");
		}
		{
			VipXOffset p;
			const std::unique_ptr<VipProcessingObject> c(p.copy());
			QVERIFY2(c != nullptr, "VipXOffset must be rebuildable by name");
		}
	}

	/// The list writer records an element count. Kept separate from the read
	/// below so that a failure points at one side or the other.
	void sessionListWritesItsCount()
	{
		VipProcessingList source;
		// A registered processing: the writer only serialises those the factory
		// can rebuild.
		QVERIFY(source.append(new VipClamp()));

		VipXOStringArchive out;
		QVERIFY(out.content("list", &source));
		QVERIFY2(out.toString().contains(">1</count>"), qPrintable(out.toString()));
	}

	/// The reader used that count directly as a loop bound, so a session file
	/// declaring a huge one made it allocate until memory ran out, silently.
	/// The archive here is a fixed string rather than one just written, which is
	/// what a crafted session file actually is.
	void hugeCountInSessionIsRejected()
	{
		const QString xml = QStringLiteral(
			"<list type_name=\"VipProcessingList*\">"
			"<processing_name type_name=\"QString\"></processing_name>"
			"<count type_name=\"qlonglong\">100000000</count>"
			"</list>");

		VipProcessingList target;
		VipXIStringArchive in(xml);
		QVERIFY(in.isOpen());
		in.content("list", &target);

		QCOMPARE(target.size(), 0);
		QVERIFY2(in.hasError(), "an out of range count must be reported, not consumed");
	}

	/// Extracting an attribute as a number must accept a number and nothing
	/// else. The conversion used to accept any string starting with digits and
	/// silently drop the rest, so "12abc" became 12 and "1,5" became 1, both
	/// reported as valid measurements. Non finite values must be refused for the
	/// same reason: they travel downstream as if they had been measured.
	void attributeToDoubleRejectsPartialNumbers()
	{
		struct Case
		{
			const char* text;
			bool accepted;
			double value;
		};
		const Case cases[] = { { "12", true, 12.0 },	 { " 3.5 ", true, 3.5 },  { "-2.25", true, -2.25 }, { "1e3", true, 1000.0 },
				       { "12abc", false, 0.0 },	 { "1,5", false, 0.0 },	  { "abc", false, 0.0 },    { "", false, 0.0 },
				       { "nan", false, 0.0 },	 { "inf", false, 0.0 },	  { "1e999", false, 0.0 },  { "0x10", false, 0.0 } };

		for (const Case& c : cases) {
			VipExtractAttribute proc;
			proc.setScheduleStrategy(VipProcessingObject::Asynchronous, false);
			proc.propertyAt(0)->setData(QString("measure"));
			proc.propertyAt(1)->setData(true);

			VipAnyData in(QVariant(0), 0);
			in.setAttribute("measure", QString::fromLatin1(c.text));
			proc.inputAt(0)->setData(in);
			proc.update();

			const bool accepted = !proc.hasError();
			QVERIFY2(accepted == c.accepted, qPrintable(QStringLiteral("'%1': expected %2, got %3").arg(c.text).arg(c.accepted).arg(accepted)));
			if (c.accepted)
				QCOMPARE(proc.outputAt(0)->data().value<double>(), c.value);
		}
	}

	// -- Image transform list ------------------------------------------------

	/// A transform list must survive a session round trip. It does not.
	///
	/// EXPECTED FAILURE. Saving two transforms writes 48 bytes — the size as a
	/// qsizetype plus twenty bytes per transform — and loading them back returns
	/// success with an empty list. The stream operators the file declares are
	/// static, so the metatype system does not use them: what runs is the generic
	/// container streaming, and the two halves do not agree. An image transform
	/// list stored in a session is therefore lost on reload, without a message.
	/// This test states the EXPECTED behaviour and must stay red until the format
	/// is made symmetric.
	void transformListRoundTrip()
	{
		TransformList source;
		source.push_back(Transform(Transform::Rotate, 90, 0));
		source.push_back(Transform(Transform::Scale, 2, 3));

		QByteArray buffer;
		{
			QDataStream out(&buffer, QIODevice::WriteOnly);
			QVERIFY(QMetaType(qMetaTypeId<TransformList>()).save(out, &source));
		}
		QCOMPARE(buffer.size(), 48);

		TransformList read;
		QDataStream in(buffer);
		QVERIFY(QMetaType(qMetaTypeId<TransformList>()).load(in, &read));

		QEXPECT_FAIL("", "the declared stream operators are static, the metatype system uses the generic ones", Continue);
		QCOMPARE(read.size(), source.size());
	}

	/// The memory a queue holds is counted in bytes, and the cap it feeds is too.
	/// The whole chain was a signed 32 bit int, which saturates at two gigabytes: a
	/// queue of large images passes that without difficulty, the count wraps, and a
	/// negative count compares favourably against any cap, so the limit stopped
	/// working exactly where it was needed.
	void queueMemoryAccountingIsSixtyFourBit()
	{
		VipFIFOList list;
		list.setMaxListMemory(Q_INT64_C(8) * 1024 * 1024 * 1024);
		QCOMPARE(list.maxListMemory(), Q_INT64_C(8) * 1024 * 1024 * 1024);

		VipProcessingManager::setMaxListMemory(Q_INT64_C(6) * 1024 * 1024 * 1024);
		QCOMPARE(VipProcessingManager::maxListMemory(), Q_INT64_C(6) * 1024 * 1024 * 1024);
		VipProcessingManager::setMaxListMemory(50000000);
	}

	/// The sum itself was an int, so a footprint above two gigabytes came back
	/// negative. The copies below share one buffer, so this measures the sum
	/// without allocating three gigabytes.
	void aFootprintAboveTwoGigabytesStaysPositive()
	{
		VipNDArrayType<double> big(vipVector(2048, 8192)); // 128 MB
		QVariantList many;
		for (int i = 0; i < 24; ++i)
			many.append(QVariant::fromValue(VipNDArray(big)));

		const qint64 footprint = vipGetMemoryFootprint(QVariant(many));
		QVERIFY2(footprint > Q_INT64_C(2) * 1024 * 1024 * 1024, "the sum must not wrap at two gigabytes");
	}

	/// And the eviction loop accumulated in an int too, so a negative sum never
	/// reached the cap and the buffer was never trimmed.
	void theMemoryCapTrimsTheBufferAboveTwoGigabytes()
	{
		VipNDArrayType<double> big(vipVector(2048, 8192)); // 128 MB, one shared buffer

		VipFIFOList list;
		list.setListLimitType(VipDataList::MemorySize);
		list.setMaxListMemory(Q_INT64_C(2560) * 1024 * 1024);

		int count = 0;
		for (int i = 0; i < 24; ++i)
			count = list.push(VipAnyData(QVariant::fromValue(VipNDArray(big)), i));

		QVERIFY2(count < 24, "the buffer must be trimmed once its footprint passes the cap");
	}

	/// A footprint larger than an int can hold is reported as it is.
	void largeDataFootprintIsNotTruncated()
	{
		VipNDArrayType<double> big(vipVector(4096, 8192)); // 256 MB
		VipAnyData any(QVariant::fromValue(VipNDArray(big)), 0);

		const qint64 footprint = any.memoryFootprint();
		QVERIFY2(footprint > 0, "a large array must not report a negative footprint");
		QVERIFY(footprint >= Q_INT64_C(256) * 1024 * 1024);
	}

	// -- Multiple inputs -----------------------------------------------------

	/// Inserting anywhere but at the end configured the element that happened to be
	/// last instead of the one just inserted, so the new entry was left unset. The
	/// setter next to it writes the right one.
	void insertingAnInputConfiguresTheInsertedOne()
	{
		MultiInputProcessing proc;
		VipMultiInput* inputs = proc.topLevelInputAt(0)->toMultiInput();
		QVERIFY(inputs);

		QVERIFY(inputs->resize(3));
		QCOMPARE(inputs->count(), 3);

		QVERIFY(inputs->insert(0));
		QCOMPARE(inputs->count(), 4);
		for (int i = 0; i < inputs->count(); ++i)
			QVERIFY2(inputs->at(i)->parentProcessing() == &proc, qPrintable(QStringLiteral("input %1 was left unconfigured").arg(i)));
	}

	/// An index outside the vector is refused rather than applied.
	void insertingOutsideTheRangeIsRefused()
	{
		MultiInputProcessing proc;
		VipMultiInput* inputs = proc.topLevelInputAt(0)->toMultiInput();
		QVERIFY(inputs->resize(2));

		QVERIFY(!inputs->insert(-1));
		QVERIFY(!inputs->insert(99));
		QCOMPARE(inputs->count(), 2);
	}

	/// The container is documented as empty by default, and back() on it is out of
	/// bounds: both accessors used it without a test.
	void anEmptyMultiInputHasNoData()
	{
		MultiInputProcessing proc;
		VipMultiInput* inputs = proc.topLevelInputAt(0)->toMultiInput();
		QVERIFY(inputs->resize(0));
		QCOMPARE(inputs->count(), 0);

		QVERIFY(!inputs->data().isValid());
		inputs->setData(makeData(1.0)); // must not crash
		QVERIFY(true);
	}

	// -- VipProcessingList ---------------------------------------------------

	/// An empty list short circuits and forwards its input to its output.
	void emptyProcessingListForwardsInput()
	{
		VipProcessingList list;
		list.setScheduleStrategy(VipProcessingObject::Asynchronous, false);
		QCOMPARE(list.size(), 0);

		list.inputAt(0)->setData(makeData(42.0));
		list.update();

		QCOMPARE(list.outputAt(0)->data().value<double>(), 42.0);
	}

	/// Two stacked processings apply in insertion order: (3 * 2) + 1 = 7, not
	/// (3 + 1) * 2 = 8.
	void processingListAppliesInInsertionOrder()
	{
		VipProcessingList list;
		list.setScheduleStrategy(VipProcessingObject::Asynchronous, false);

		MultiplyByProperty* mult = new MultiplyByProperty();
		mult->propertyAt(0)->setData(2.0);
		QVERIFY(list.append(mult));
		QVERIFY(list.append(new AddOne()));
		QCOMPARE(list.size(), 2);

		list.inputAt(0)->setData(makeData(3.0));
		list.update();

		QCOMPARE(list.outputAt(0)->data().value<double>(), 7.0);
	}

	/// VipProcessingList owns its processings: destroying the list destroys
	/// them, checked through QPointer.
	void processingListOwnsItsProcessings()
	{
		QPointer<VipProcessingObject> observed;
		{
			VipProcessingList list;
			AddOne* child = new AddOne();
			observed = child;
			QVERIFY(list.append(child));
			QVERIFY(!observed.isNull());
		}
		QVERIFY2(observed.isNull(), "destroying the list must destroy the processings it owns");
	}

	/// The indexed accessors cast their argument to size_t and indexed a vector
	/// with it, so a negative index addressed far past the end. They now hold
	/// the contract of the accessors by name: out of range gives nullptr.
	void indexedAccessorsRejectAnIndexOutOfRange()
	{
		MultiplyByProperty processing;

		QVERIFY(processing.inputAt(0));
		QVERIFY2(!processing.inputAt(-1), "a negative index must not be cast into a huge one");
		QVERIFY(!processing.inputAt(processing.inputCount()));
		QVERIFY(!processing.outputAt(-1));
		QVERIFY(!processing.outputAt(processing.outputCount()));
		QVERIFY(!processing.propertyAt(-1));
		QVERIFY(!processing.propertyAt(processing.propertyCount()));
		QVERIFY(!processing.topLevelInputAt(-1));
		QVERIFY(!processing.topLevelInputAt(processing.topLevelInputCount()));
		QVERIFY(!processing.topLevelOutputAt(processing.topLevelOutputCount()));
		QVERIFY(!processing.topLevelPropertyAt(processing.topLevelPropertyCount()));
	}

	/// Same for the list, whose position argument is public input: inserting
	/// past the end of a QList is undefined, and at()/take() indexed on trust.
	void processingListBoundsItsPositions()
	{
		VipProcessingList list;
		AddOne* first = new AddOne();
		QVERIFY(list.append(first));

		AddOne* late = new AddOne();
		QVERIFY2(list.insert(50, late), "a position past the end is clamped, not rejected");
		QCOMPARE(list.size(), 2);
		QCOMPARE(list.at(1), late);

		QVERIFY(!list.at(-1));
		QVERIFY(!list.at(list.size()));
		QVERIFY(!list.take(-1));
		QVERIFY(!list.take(list.size()));
		QCOMPARE(list.size(), 2);
	}

	/// Assigning an output left its data behind, so the replaced output kept
	/// serving the value of the one it was supposed to become. The sibling
	/// property class always assigned it.
	void assigningAnOutputCarriesItsData()
	{
		VipOutput first("first");
		first.setData(VipAnyData(QVariant(1.0), 10));
		VipOutput second("second");
		second.setData(VipAnyData(QVariant(2.0), 20));

		first = second;

		QCOMPARE(first.data().value<double>(), 2.0);
		QCOMPARE(first.data().time(), (qint64)20);
	}

	/// The set of error codes actually logged was copied from an empty member in
	/// the initialiser list, before the constructor body filled it, so nothing
	/// was ever logged.
	void theDefaultLoggedErrorCodesAreNotEmpty()
	{
		QVERIFY(VipProcessingManager::isLogErrorEnabled(VipProcessingObject::RuntimeError));
		QVERIFY(VipProcessingManager::isLogErrorEnabled(VipProcessingObject::WrongInput));
		QVERIFY(VipProcessingManager::isLogErrorEnabled(VipProcessingObject::IOError));
	}

	/// A connection carries a parent processing object only once it has been
	/// attached to one. Opening a standalone connection walked straight through
	/// that null parent, and so did the branch meant to report the bad address.
	void anUnattachedConnectionReportsInsteadOfFaulting()
	{
		VipConnectionPtr connection(new VipConnection());
		connection->setupConnection("VipConnection:no_such_processing;output");

		QVERIFY2(!connection->openConnection(VipConnection::InputConnection), "an address that resolves to nothing must fail to open");
		QVERIFY(connection->hasError());

		connection->receiveData(VipAnyData(QVariant(1.0), 0));
	}

	/// Opening the connections of a processing dropped the result for every one
	/// of them, so a session whose address does not resolve reloaded silently
	/// incomplete.
	void openingConnectionsReportsAnAddressThatDoesNotResolve()
	{
		MultiplyByProperty processing;
		processing.setObjectName("consumer");
		processing.inputAt(0)->setConnection("VipConnection:no_such_processing;output");

		QVERIFY(!processing.openInputConnections());
	}

	/// The priority map used to be streamed through a reinterpret_cast onto a map
	/// of int, which is undefined and let any value from the stream reach
	/// QThread::setPriority. The wire format is unchanged.
	void aThreadPriorityOutsideTheEnumerationIsRefused()
	{
		QByteArray buffer;
		{
			QDataStream str(&buffer, QIODevice::WriteOnly);
			str << (quint32)2 << QString("known") << (qint32)QThread::HighPriority << QString("forged") << (qint32)987654;
		}

		PriorityMap map;
		{
			QDataStream str(&buffer, QIODevice::ReadOnly);
			str >> map;
		}

		QCOMPARE(map.size(), 2);
		QCOMPARE(map.value("known"), QThread::HighPriority);
		QCOMPARE(map.value("forged"), QThread::InheritPriority);
	}

	/// And a map written by the current code reads back identically.
	void thePriorityMapSurvivesARoundTrip()
	{
		PriorityMap written;
		written.insert("first", QThread::LowestPriority);
		written.insert("second", QThread::TimeCriticalPriority);

		QByteArray buffer;
		{
			QDataStream str(&buffer, QIODevice::WriteOnly);
			str << written;
		}
		PriorityMap read;
		{
			QDataStream str(&buffer, QIODevice::ReadOnly);
			str >> read;
		}
		QCOMPARE(read, written);
	}

	/// The descent along the sources had no visited set, so two processings that
	/// are sources of each other recursed until the stack ran out.
	void aCycleInTheSourcesDoesNotRecurseForever()
	{
		MultiplyByProperty first;
		MultiplyByProperty second;
		QVERIFY(first.outputAt(0)->setConnection(second.inputAt(0)));
		QVERIFY(second.outputAt(0)->setConnection(first.inputAt(0)));
		QVERIFY(first.directSources().contains(&second));
		QVERIFY(second.directSources().contains(&first));

		first.setSourceProperty("campaign", QVariant(7));

		QCOMPARE(first.property("campaign").toInt(), 7);
		QCOMPARE(second.property("campaign").toInt(), 7);
	}

	/// The reader dropped the connections and wrote the object as it went, so an
	/// archive that stops after the first field left a disconnected object
	/// carrying default values. It now applies nothing at all.
	void aTruncatedProcessingArchiveLeavesTheObjectAlone()
	{
		const QString xml = QStringLiteral("<processing type_name=\"VipClamp*\">"
						   "<processing_name type_name=\"QString\">from_file</processing_name>"
						   "</processing>");

		VipClamp target;
		target.setObjectName("original");
		target.setProcessingVisible(true);

		VipXIStringArchive in(xml);
		QVERIFY(in.isOpen());
		in.content("processing", &target);

		QCOMPARE(target.objectName(), QString("original"));
		QVERIFY2(target.isProcessingVisible(), "a field that was never read must not be applied");
	}

	/// The task pool outlives the derived parts of the object: it is destroyed by
	/// the base destructor, after every derived destructor has run. Nothing may
	/// be submitted or run from the moment destruction starts.
	void anObjectBeingDestroyedTakesNoMoreWork()
	{
		MultiplyByProperty* processing = new MultiplyByProperty();
		processing->inputAt(0)->setData(VipAnyData(QVariant(3.0), 0));
		QVERIFY(processing->update(true));
		const int applied = processing->applyCount.load();

		bool seen = false;
		bool accepted = true;
		bool flagged = false;
		QObject::connect(processing, &VipProcessingObject::destroyed, processing, [&](VipProcessingObject* obj) {
			seen = true;
			flagged = obj->isBeingDestroyed();
			obj->inputAt(0)->setData(VipAnyData(QVariant(5.0), 1));
			accepted = obj->update(true);
		});

		delete processing;

		QVERIFY2(seen, "the destruction signal must reach the slot");
		QVERIFY2(flagged, "the object must know it is being destroyed");
		QVERIFY2(!accepted, "no work may be submitted once destruction has started");
		Q_UNUSED(applied);
	}

	/// The setter says it takes ownership of the device. Refusing it once the
	/// object is open used to leave it neither stored nor destroyed, and the
	/// caller had already let go of it.
	void aRefusedDeviceIsStillDestroyed()
	{
		VipStreamingFromDevice streaming;
		VipAnyResource* inner = new VipAnyResource();
		inner->setData(QVariant(1.0));
		streaming.setIODevice(inner);
		QVERIFY(streaming.open(VipIODevice::ReadOnly));

		QPointer<VipIODevice> observed = new VipAnyResource();
		streaming.setIODevice(observed);

		QVERIFY2(observed.isNull(), "a device that is taken but not kept must be destroyed");
		QCOMPARE(streaming.IODevice(), (VipIODevice*)inner);
	}

	/// The signal that says a processing is done reaches a processing list in a
	/// direct connection, and that list then takes its own mutex; the list, while
	/// holding that mutex, runs its children, which take the lock serialising a
	/// run. Two threads closed the cycle, so the signal now goes out once that
	/// lock is released, through a flag the run leaves behind. This pins the
	/// contract of that flag: one signal per run, no more and no less.
	void processingDoneIsEmittedOncePerRun()
	{
		MultiplyByProperty processing;
		processing.inputAt(0)->setData(VipAnyData(QVariant(2.0), 0));

		processing.setScheduleStrategies(VipProcessingObject::OneInput | VipProcessingObject::NoThread);

		// Running again from the slot needs the lock that serialises a run, and that
		// lock does not nest: emitted from under it, this call spins for ever.
		int depth = 0;
		QObject::connect(&processing,
				 &VipProcessingObject::processingDone,
				 &processing,
				 [&](VipProcessingObject* obj, qint64) {
					 ++depth;
				 },
				 Qt::DirectConnection);

		QVERIFY(processing.update(true));
		QCOMPARE(depth, 1);

		QVERIFY(processing.update(true));
		QCOMPARE(depth, 2);
	}

	/// The manager is read from the thread that processes and written from the
	/// thread that configures. The set of error codes and the map of priorities
	/// were read without the mutex that guards the writes, and a Qt container
	/// being rehashed is not readable. This exercises both sides at once.
	void theManagerSurvivesConcurrentConfiguration()
	{
		const QSet<int> initial = VipProcessingManager::logErrors();
		const qint64 initialMemory = VipProcessingManager::maxListMemory();

		std::atomic<bool> stop{ false };
		std::atomic<int> reads{ 0 };

		QThread* reader = QThread::create([&]() {
			while (!stop.load()) {
				VipProcessingManager::logErrors();
				VipProcessingManager::defaultPriorities();
				VipProcessingManager::maxListMemory();
				VipProcessingManager::listLimitType();
				++reads;
			}
		});
		reader->start();

		// The writer below is short: without this the reader could still be
		// starting when it ends, and the count asserted at the end would be zero.
		QElapsedTimer started;
		started.start();
		while (reads.load() == 0 && started.elapsed() < 30000)
			QThread::msleep(1);

		for (int i = 0; i < 200; ++i) {
			VipProcessingManager::setLogErrorEnabled(VipProcessingObject::RuntimeError, i % 2 == 0);
			VipProcessingManager::setMaxListMemory(40000000 + i);
		}

		stop.store(true);
		QVERIFY(reader->wait(30000));
		delete reader;

		QVERIFY2(reads.load() > 0, "the reader must have run");

		VipProcessingManager::setLogErrors(initial);
		VipProcessingManager::setMaxListMemory(initialMemory);
		QCOMPARE(VipProcessingManager::maxListMemory(), initialMemory);
	}

	/// The list ran its whole pipeline holding the mutex that guards its
	/// container, so anything else that only wanted to look at the list waited
	/// behind every processing. Looking at it from inside a processing of that
	/// same list is the shortest way to show the mutex is no longer held: the
	/// mutex nests, but only for the thread that owns it, and this is the thread
	/// that runs the pipeline.
	void theListDoesNotHoldItsMutexWhileRunning()
	{
		VipProcessingList list;
		AddOne* first = new AddOne();
		QVERIFY(list.append(first));

		bool sourcesRead = false;
		QObject::connect(first,
				 &VipProcessingObject::processingDone,
				 &list,
				 [&](VipProcessingObject*, qint64) {
					 // Reads the container under the mutex.
					 list.directSources();
					 sourcesRead = true;
				 },
				 Qt::DirectConnection);

		list.inputAt(0)->setData(VipAnyData(QVariant(1.0), 0));
		QVERIFY(list.update(true));
		list.wait();

		QVERIFY2(sourcesRead, "the container must be readable while the pipeline runs");
		QCOMPARE(list.outputAt(0)->data().value<double>(), 2.0);
	}

	/// The pool thread held the mutex of the pool for the whole processing, so
	/// every bounded wait spun in try_lock_for until the processing was over:
	/// the calling thread, most often the one of the interface, burnt a core for
	/// as long as the work lasted. The wait now sleeps on the condition, which
	/// shows as processor time far below the elapsed time.
	void waitingForAProcessingDoesNotBurnTheProcessor()
	{
		SlowProcessing proc;
		proc.setComputeTimeStatistics(false);
		proc.setScheduleStrategy(VipProcessingObject::Asynchronous, true);
		proc.inputAt(0)->setData(makeData(1.0));
		QVERIFY(proc.update());

		const qint64 processorBefore = processorMilliseconds();
		QElapsedTimer timer;
		timer.start();
		proc.wait(false, 200);
		const qint64 elapsed = timer.elapsed();
		const qint64 processor = processorMilliseconds() - processorBefore;

		QVERIFY2(elapsed >= 150, qPrintable(QString("the wait returned after %1 ms, it did not wait").arg(elapsed)));
		QVERIFY2(processor * 2 < elapsed, qPrintable(QString("%1 ms of processor time for %2 ms of wait").arg(processor).arg(elapsed)));

		QVERIFY(proc.wait(false, 30000));
	}

	/// update() took a spinlock and kept it across the wait for the result, so a
	/// second call from another thread span on it for the whole processing. The
	/// lock is now released before that wait, and what an update in flight is
	/// answered by a counter rather than by the state of the lock.
	void aSecondUpdateDoesNotSpinOnTheFirst()
	{
		SlowProcessing proc;
		proc.setComputeTimeStatistics(false);
		proc.inputAt(0)->setData(makeData(1.0));

		std::atomic<bool> updatingSeen{ false };
		QThread* first = QThread::create([&]() {
			proc.update(true);
		});
		first->start();

		// Let the first call reach its wait.
		QThread::msleep(100);
		updatingSeen = proc.isUpdating();

		const qint64 processorBefore = processorMilliseconds();
		QElapsedTimer timer;
		timer.start();
		proc.update(true);
		const qint64 elapsed = timer.elapsed();
		const qint64 processor = processorMilliseconds() - processorBefore;

		QVERIFY(first->wait(30000));
		delete first;

		QVERIFY2(updatingSeen.load(), "an update waiting for its result is still an update in flight");
		QVERIFY2(elapsed >= 200, qPrintable(QString("the second update returned after %1 ms").arg(elapsed)));
		QVERIFY2(processor * 4 < elapsed, qPrintable(QString("%1 ms of processor time for %2 ms of update").arg(processor).arg(elapsed)));
	}

	/// A run that finds no new input asks the pool to drop what is scheduled.
	/// Asked for from a thread that is not the one of the pool, the request
	/// simply stayed armed, and the pool then threw away the first batch it woke
	/// up for: real work, pushed after the skip, silently lost.
	void skippingForLackOfInputDoesNotLoseTheNextTask()
	{
		MultiplyByProperty proc;
		proc.setComputeTimeStatistics(false);

		// Asynchronous first, so the pool exists and runs one real input.
		proc.setScheduleStrategies(VipProcessingObject::OneInput | VipProcessingObject::SkipIfNoInput | VipProcessingObject::Asynchronous);
		proc.inputAt(0)->setData(makeData(1.0));
		QVERIFY(proc.wait(true, 30000));
		QCOMPARE(proc.applyCount.load(), 1);

		// A forced run in the calling thread, with nothing new on the input: this
		// is the skip, and the pool is idle at that moment.
		proc.setScheduleStrategies(VipProcessingObject::OneInput | VipProcessingObject::SkipIfNoInput | VipProcessingObject::NoThread);
		QVERIFY(proc.update(true));
		QCOMPARE(proc.applyCount.load(), 1);

		// Back to the pool, with real input.
		proc.setScheduleStrategies(VipProcessingObject::OneInput | VipProcessingObject::SkipIfNoInput | VipProcessingObject::Asynchronous);
		proc.inputAt(0)->setData(makeData(3.0));
		QVERIFY(proc.wait(true, 30000));

		QCOMPARE(proc.applyCount.load(), 2);
		QCOMPARE(proc.outputAt(0)->data().value<double>(), 6.0);
	}

	/// Inserting a processing propagated the source properties of the list while
	/// holding the mutex of the list. That propagation is a virtual reimplemented
	/// outside the library, plugins included, and every other thread that only
	/// wanted the size of the list waited behind it.
	void insertingDoesNotHoldTheMutexWhileItCallsTheProcessing()
	{
		VipProcessingList list;
		list.setSourceProperty("test_property", QVariant(1));

		WatchingProcessing* proc = new WatchingProcessing();
		bool readInTime = false;
		QThread* reader = nullptr;
		proc->onSourceProperty = [&]() {
			if (reader)
				return;
			reader = QThread::create([&]() { list.size(); });
			reader->start();
			// Answered here, while the insert is still on the stack: joined after
			// it returns, the read always gets through in the end.
			readInTime = reader->wait(1000);
		};

		QVERIFY(list.insert(0, proc));

		QVERIFY(reader);
		QVERIFY(reader->wait(30000));
		delete reader;

		QVERIFY2(readInTime, "the list must be readable while it configures a processing");
	}
};

VIP_TEST_MAIN(TestProcessingObject)
#include "TestProcessingObject.moc"
