/// @file TestNDArray.cpp
///
/// Characterisation tests for the array handles: who owns the buffer, what a
/// failed allocation leaves behind, and what a shape read from a file may ask
/// for.

#include <QTest>

#include "vip_test_main.h"

#include "VipNDArray.h"
#include "VipStack.h"

#include <vector>

class TestNDArray : public QObject
{
	Q_OBJECT

private Q_SLOTS:

	/// A shape comes from the file, and it sizes the allocation on its own. The
	/// elements have not been read yet, so a shape asking for more bytes than the
	/// stream holds cannot be honoured.
	void aShapeLargerThanTheStreamIsRefused()
	{
		QByteArray buffer;
		{
			QDataStream out(&buffer, QIODevice::WriteOnly);
			out << (int)VipNDArrayHandle::Standard;
			out << (int)QMetaType::Double;
			out << vipVector(100000, 100000); // 80 GB, and eight bytes of stream
		}

		VipNDArray ar;
		QDataStream in(&buffer, QIODevice::ReadOnly);
		in >> ar;

		QVERIFY2(ar.isEmpty(), "an unreadable shape must not size an allocation");
		QCOMPARE(in.status(), QDataStream::ReadCorruptData);
	}

	/// A shape whose product wraps would allocate less than the caller writes.
	void aShapeWhoseProductWrapsIsRefused()
	{
		QByteArray buffer;
		{
			QDataStream out(&buffer, QIODevice::WriteOnly);
			out << (int)VipNDArrayHandle::Standard;
			out << (int)QMetaType::Double;
			out << vipVector((qsizetype)1 << 40, (qsizetype)1 << 40);
		}

		VipNDArray ar;
		QDataStream in(&buffer, QIODevice::ReadOnly);
		in >> ar;

		QVERIFY(ar.isEmpty());
		QCOMPARE(in.status(), QDataStream::ReadCorruptData);
	}

	/// A round trip of an ordinary array still works.
	void anOrdinaryArrayStillRoundTrips()
	{
		VipNDArrayType<double> source(vipVector(2, 3));
		for (int y = 0; y < 2; ++y)
			for (int x = 0; x < 3; ++x)
				source(vipVector(y, x)) = y * 3 + x;

		QByteArray buffer;
		{
			QDataStream out(&buffer, QIODevice::WriteOnly);
			out << VipNDArray(source);
		}
		VipNDArray read;
		{
			QDataStream in(&buffer, QIODevice::ReadOnly);
			in >> read;
		}

		QCOMPARE(read.shape(), source.shape());
		VipNDArrayType<double> typed = read.toDouble();
		for (int y = 0; y < 2; ++y)
			for (int x = 0; x < 3; ++x)
				QCOMPARE(typed(vipVector(y, x)), (double)(y * 3 + x));
	}

	/// An allocation that cannot be served leaves the handle as it was. It used
	/// to publish the new shape and size first, so the handle then described an
	/// array that does not exist.
	void aFailedReallocLeavesTheHandleIntact()
	{
		VipNDArrayType<double> ar(vipVector(4));
		for (int i = 0; i < 4; ++i)
			ar(vipVector(i)) = i;

		VipNDArrayHandle* handle = const_cast<VipNDArrayHandle*>(ar.handle());
		QVERIFY(!handle->realloc(vipVector((qsizetype)1 << 45))); // 256 TB

		QCOMPARE(handle->size, (qsizetype)4);
		QVERIFY(handle->opaque != nullptr);
		for (int i = 0; i < 4; ++i)
			QCOMPARE(ar(vipVector(i)), (double)i);
	}

	/// Two views over the same external buffer: destroying one used to clear the
	/// buffer of the handle they share, pulling it from under the other.
	void oneViewDoesNotEmptyAnother()
	{
		std::vector<double> owned(8, 3.5);

		VipNDArray first = VipNDArray::makeView(owned.data(), vipVector(8));
		{
			VipNDArray second = first;
			QCOMPARE(second.shape(), vipVector(8));
		}

		QVERIFY2(!first.isEmpty(), "the surviving view must still see the buffer");
		QCOMPARE(first.value(vipVector(0)).toDouble(), 3.5);

		// And the buffer belongs to the caller: it is still readable here.
		QCOMPARE(owned[0], 3.5);
	}

	/// The axis of a stack comes from the caller and is used as an index into a
	/// shape held on the stack, twice to write. The only bound was a debug
	/// assertion, which is nothing in a release build.
	void stackingRefusesAnAxisOutOfRange()
	{
		VipNDArrayType<double> a(vipVector(2, 3));
		VipNDArrayType<double> b(vipVector(2, 3));

		QVERIFY(vipStack(VipNDArray(a), VipNDArray(b), 7).isEmpty());
		QVERIFY(vipStack(VipNDArray(a), VipNDArray(b), -1).isEmpty());

		VipNDArray dst(qMetaTypeId<double>(), vipVector(4, 3));
		QVERIFY(!vipStack(dst, VipNDArray(a), VipNDArray(b), 7));
		QVERIFY(!vipStack(dst, VipNDArray(a), VipNDArray(b), -1));

		// And the ordinary case still works.
		QVERIFY(vipStack(dst, VipNDArray(a), VipNDArray(b), 0));
		QCOMPARE(vipStack(VipNDArray(a), VipNDArray(b), 0).shape(), vipVector(4, 3));
	}

	/// The guard on the shapes compared the same two things twice, so the shape of
	/// the destination was never checked at all.
	void stackingRefusesADestinationOfTheWrongShape()
	{
		VipNDArrayType<double> a(vipVector(2, 3));
		VipNDArrayType<double> b(vipVector(2, 3));

		VipNDArray narrow(qMetaTypeId<double>(), vipVector(4, 2));
		QVERIFY2(!vipStack(narrow, VipNDArray(a), VipNDArray(b), 0), "a destination that is too narrow must be refused");
	}
};

VIP_TEST_MAIN(TestNDArray)
#include "TestNDArray.moc"
