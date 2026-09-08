/// @file TestSerialization.cpp
///
/// Characterisation tests for the binary deserialisation surface: sizes and
/// counts read from a stream, which is what a session or data file supplies.

#include <QTest>

#include "vip_test_main.h"

#include "VipNDArray.h"

class TestSerialization : public QObject
{
	Q_OBJECT

private Q_SLOTS:

	/// A shape round trips through a data stream unchanged.
	void shapeRoundTrip()
	{
		const VipNDArrayShape source = vipVector(3, 5);

		QByteArray buffer;
		{
			QDataStream out(&buffer, QIODevice::WriteOnly);
			out << source;
		}

		VipNDArrayShape read;
		QDataStream in(buffer);
		in >> read;

		QCOMPARE(in.status(), QDataStream::Ok);
		QCOMPARE(read.size(), source.size());
		QCOMPARE(read[0], source[0]);
		QCOMPARE(read[1], source[1]);
	}

	/// A size read from the stream used to be applied as is. The storage is a
	/// fixed array of VIP_MAX_DIMS elements and the resize only asserts in debug
	/// builds, so a crafted stream wrote past the end of the object in release
	/// builds. The read must now refuse the size instead.
	void shapeWithOutOfRangeSizeIsRejected()
	{
		QByteArray buffer;
		{
			QDataStream out(&buffer, QIODevice::WriteOnly);
			out << static_cast<qsizetype>(1000000);
			for (int i = 0; i < 16; ++i)
				out << static_cast<qsizetype>(1);
		}

		VipNDArrayShape read;
		QDataStream in(buffer);
		in >> read;

		QVERIFY2(in.status() != QDataStream::Ok, "an out of range size must mark the stream corrupt");
		QVERIFY2(read.size() <= VIP_MAX_DIMS, "the shape must never hold more than its storage");
	}

	/// Same guard on the negative side: a signed size read from a file can be
	/// negative, and it used to be assigned unchecked.
	void shapeWithNegativeSizeIsRejected()
	{
		QByteArray buffer;
		{
			QDataStream out(&buffer, QIODevice::WriteOnly);
			out << static_cast<qsizetype>(-1);
		}

		VipNDArrayShape read;
		QDataStream in(buffer);
		in >> read;

		QVERIFY(in.status() != QDataStream::Ok);
		QVERIFY(read.size() >= 0);
	}

	/// A truncated stream must not be read as if it were complete: the size is
	/// announced but the elements are missing.
	void truncatedShapeStopsReading()
	{
		QByteArray buffer;
		{
			QDataStream out(&buffer, QIODevice::WriteOnly);
			out << static_cast<qsizetype>(3);
			out << static_cast<qsizetype>(7); // one element only, two are missing
		}

		VipNDArrayShape read;
		QDataStream in(buffer);
		in >> read;

		QVERIFY2(in.status() != QDataStream::Ok, "a truncated stream must be reported");
	}
};

VIP_TEST_MAIN(TestSerialization)
#include "TestSerialization.moc"
