/// @file TestSerialization.cpp
///
/// Characterisation tests for the binary deserialisation surface: sizes and
/// counts read from a stream, which is what a session or data file supplies.

#include <QTest>

#include "vip_test_main.h"

#include <QDir>
#include <type_traits>

#include "VipIterator.h"
#include "VipNDArray.h"
#include "VipLongDouble.h"
#include "VipVectors.h"

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

	/// A sample vector round trips through a data stream unchanged.
	void sampleVectorRoundTrip()
	{
		VipPointVector source;
		source.push_back(VipPoint(1.0, 2.0));
		source.push_back(VipPoint(3.0, 4.0));

		QByteArray buffer;
		{
			QDataStream out(&buffer, QIODevice::WriteOnly);
			out << source;
		}

		VipPointVector read;
		QDataStream in(buffer);
		in >> read;

		QCOMPARE(in.status(), QDataStream::Ok);
		QCOMPARE(read.size(), source.size());
		QCOMPARE(read[1].x(), 3.0);
	}

	/// The element count was reserved before a single element had been read, so a
	/// crafted stream asked for an arbitrary allocation up front. A count larger
	/// than the bytes left in the stream cannot be honoured and must be refused.
	void sampleVectorWithImplausibleCountIsRejected()
	{
		QByteArray buffer;
		{
			QDataStream out(&buffer, QIODevice::WriteOnly);
			out << static_cast<qint64>(200000000); // ~3 GB of points announced
			out << 1.0 << 2.0;
		}

		VipPointVector read;
		QDataStream in(buffer);
		in >> read;

		QVERIFY2(in.status() != QDataStream::Ok, "a count the stream cannot back must be refused");
		QVERIFY(read.isEmpty());
	}

	/// Same guard on the negative side: the count is signed and comes from the file.
	void sampleVectorWithNegativeCountIsRejected()
	{
		QByteArray buffer;
		{
			QDataStream out(&buffer, QIODevice::WriteOnly);
			out << static_cast<qint64>(-1);
		}

		VipPointVector read;
		QDataStream in(buffer);
		in >> read;

		QVERIFY(in.status() != QDataStream::Ok);
		QVERIFY(read.isEmpty());
	}

	/// A truncated stream must stop the read rather than fill the container.
	void truncatedSampleVectorStopsReading()
	{
		QByteArray buffer;
		{
			QDataStream out(&buffer, QIODevice::WriteOnly);
			out << static_cast<qint64>(3);
			out << 1.0 << 2.0; // one point only, two are missing
		}

		VipPointVector read;
		QDataStream in(buffer);
		in >> read;

		QVERIFY(in.status() != QDataStream::Ok);
		QVERIFY(read.isEmpty());
	}

	/// The width of an extended precision value is carried by a dynamic property
	/// on the device, so any code holding that device sets it, and it used to be
	/// passed as is to a raw read over a sixteen byte stack buffer. Anything wider
	/// than the buffer must be refused.
	void extendedPrecisionWidthIsBounded()
	{
		QByteArray payload(4096, '\xcc');
		QDataStream in(payload);

		const unsigned savedAsLongDouble = 1u << 31;
		const vip_double value = vipReadLEDouble(savedAsLongDouble | 1000u, in);

		QCOMPARE(in.status(), QDataStream::ReadCorruptData);
		QCOMPARE(static_cast<double>(value), 0.0);
	}

	/// Same guard on the second reader, and on a width of zero.
	void extendedPrecisionZeroWidthIsRejected()
	{
		QByteArray payload(4096, '\xcc');
		QDataStream in(payload);

		const unsigned savedAsLongDouble = 1u << 31;
		vipReadLELongDouble(savedAsLongDouble | 0u, in);

		QCOMPARE(in.status(), QDataStream::ReadCorruptData);
	}

	/// A width the buffer can hold, but the stream cannot supply, must be reported
	/// rather than read as if complete.
	void extendedPrecisionShortReadIsReported()
	{
		QByteArray payload(4, '\x01');
		QDataStream in(payload);

		const unsigned savedAsLongDouble = 1u << 31;
		vipReadLEDouble(savedAsLongDouble | 10u, in);

		QVERIFY(in.status() != QDataStream::Ok);
	}

	/// The size of an array is the product of its dimensions, computed in a signed
	/// integer. Four dimensions of 65536 reach 2^64: the product used to wrap, which
	/// is undefined behaviour and yielded a small or negative size that was then
	/// used to walk the array and to size allocations. Each dimension here is
	/// plausible on its own, which is what tells this apart from a bad dimension.
	void shapeSizeOverflowIsReported()
	{
		VipNDArrayShape shape = vipVector(65536, 65536, 65536, 65536);

		QCOMPARE(vipShapeToSize(shape), (qsizetype)-1);
	}

	/// A negative dimension is refused rather than multiplied.
	void negativeDimensionIsReported()
	{
		QCOMPARE(vipShapeToSize(vipVector(4, -1)), (qsizetype)-1);
	}

	/// A shape that fits still gives its size.
	void shapeSizeIsComputed()
	{
		QCOMPARE(vipShapeToSize(vipVector(3, 5, 7)), (qsizetype)105);
	}

	/// The stride computation multiplies the same dimensions and must report the
	/// same overflow.
	void defaultStridesReportOverflow()
	{
		VipNDArrayShape shape = vipVector(65536, 65536, 65536, 65536);
		VipNDArrayShape strides;

		QCOMPARE((vipComputeDefaultStrides<Vip::FirstMajor>(shape, strides)), (qsizetype)-1);
	}

	/// Reading an array from a file is a conversion no longer: with a default second
	/// argument these constructors converted, so any function taking a const
	/// VipNDArray& accepted a string literal and read the file it named, without a
	/// conversion appearing at the call site.
	void arrayIsNotConstructibleFromAStringImplicitly()
	{
		QVERIFY(!(std::is_convertible<const char*, VipNDArray>::value));
		QVERIFY(!(std::is_convertible<QIODevice*, VipNDArray>::value));
		QVERIFY((std::is_constructible<VipNDArray, const char*>::value));
	}

	/// A file that cannot be read leaves a null array rather than a half built one.
	void arrayFromMissingFileIsNull()
	{
		const QByteArray path = (QDir::tempPath() + QStringLiteral("/vip_no_such_file.bin")).toLatin1();
		const VipNDArray array(path.constData());

		QVERIFY(array.isNull());
	}

	/// Reading a bounded number of values from a text stream must leave the stream
	/// where those values end. The reader took the whole stream and then tried to
	/// put the position back with a value that is -1 whenever the loop stopped on a
	/// failed extraction, which is the normal case: the stream stayed at the end and
	/// everything after the values was silently unreadable.
	void textReaderLeavesTheRestReadable()
	{
		QString text = QStringLiteral("header 1.5 2.5 3.5 tail");
		QTextStream stream(&text, QIODevice::ReadOnly);

		// Start somewhere other than zero: the restored position is an absolute one,
		// and at zero a wrong one and a right one agree.
		QString header;
		stream >> header;
		QCOMPARE(header, QStringLiteral("header"));

		QVector<vip_long_double> values(3);
		QCOMPARE(vipReadNLongDouble(stream, values), 3);
		QCOMPARE(static_cast<double>(values[2]), 3.5);

		const QString rest = stream.readAll().trimmed();
		QCOMPARE(rest, QStringLiteral("tail"));
	}

	/// Same for the appending reader, which stops when the values run out.
	void appendingTextReaderLeavesTheRestReadable()
	{
		QString text = QStringLiteral("10 20 end of line");
		QTextStream stream(&text, QIODevice::ReadOnly);

		QVector<vip_long_double> values;
		QCOMPARE(vipReadNLongDoubleAppend(stream, values, 8), 2);
		QCOMPARE(values.size(), 2);

		const QString rest = stream.readAll().trimmed();
		QCOMPARE(rest, QStringLiteral("end of line"));
	}

	/// The iterator that walks an array while skipping one dimension computed its
	/// strides from the current position instead of from the shape. The constructor
	/// zeroes that position, so every stride but the last was zero and the first
	/// division of the first call divided by zero. Two dimensions hid it, the single
	/// stride being 1; three did not. This runs on the parallel path of resizing,
	/// once per thread.
	void skippingIteratorPlacesItselfOnAThreeDimensionalShape()
	{
		const VipNDArrayShape shape = vipVector(2, 3, 4);
		const qsizetype skip = 1;

		for (qsizetype flat = 0; flat < 2 * 4; ++flat) {
			detail::CIteratorFMajorSkipDim<VipNDArrayShape> it(shape, skip);
			it.setFlatPosition(flat);

			QCOMPARE(it.pos[skip], (qsizetype)0);
			QCOMPARE(it.pos[0] * 4 + it.pos[2], flat);
			QVERIFY(it.pos[0] < 2);
			QVERIFY(it.pos[2] < 4);
		}
	}

	/// The two dimensional case, which already worked, still does.
	void skippingIteratorPlacesItselfOnATwoDimensionalShape()
	{
		const VipNDArrayShape shape = vipVector(3, 5);

		for (qsizetype flat = 0; flat < 5; ++flat) {
			detail::CIteratorFMajorSkipDim<VipNDArrayShape> it(shape, 0);
			it.setFlatPosition(flat);

			QCOMPARE(it.pos[0], (qsizetype)0);
			QCOMPARE(it.pos[1], flat);
		}
	}
};

VIP_TEST_MAIN(TestSerialization)
#include "TestSerialization.moc"
