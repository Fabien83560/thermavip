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
#include "VipXmlArchive.h"

#include <atomic>
#include <memory>

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

	/// A footprint larger than an int can hold is reported as it is.
	void largeDataFootprintIsNotTruncated()
	{
		VipNDArrayType<double> big(vipVector(4096, 8192)); // 256 MB
		VipAnyData any(QVariant::fromValue(VipNDArray(big)), 0);

		const qint64 footprint = any.memoryFootprint();
		QVERIFY2(footprint > 0, "a large array must not report a negative footprint");
		QVERIFY(footprint >= Q_INT64_C(256) * 1024 * 1024);
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
};

VIP_TEST_MAIN(TestProcessingObject)
#include "TestProcessingObject.moc"
