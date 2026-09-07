/// @file TestArchive.cpp
///
/// Characterisation tests for VipArchive: they capture the CURRENT behaviour of
/// the serialisation surface, flaws included. Their purpose is to catch a
/// regression, not to validate a specification.

#include <QTest>

#include "vip_test_main.h"

#include "VipArchive.h"
#include "VipXmlArchive.h"

class TestArchive : public QObject
{
	Q_OBJECT

private Q_SLOTS:

	/// Round trip of a single top-level content, which is what the only
	/// in-tree consumer does.
	void xmlRoundTripSingleContent()
	{
		VipXOStringArchive out;
		QVERIFY(out.content("number", 42));

		VipXIStringArchive in(out.toString());
		QVERIFY2(in.isOpen(), "a single content yields a well formed, single-root document");

		int number = 0;
		QVERIFY(in.content("number", number));
		QCOMPARE(number, 42);
	}

	/// Round trip of several top-level contents.
	///
	/// EXPECTED FAILURE. VipXOStringArchive emits one sibling element per content
	/// and NO root element, so the document is not well formed from the second
	/// content on and VipXIStringArchive::open() returns false. This test states
	/// the EXPECTED behaviour, not the current one: it must stay red until the
	/// defect is fixed, at which point QTest reports XPASS and fails the test,
	/// forcing the marker below to be removed.
	void xmlRoundTripMultipleContents()
	{
		VipXOStringArchive out;
		QVERIFY(out.content("number", 42));
		QVERIFY(out.content("real", 3.5));
		QVERIFY(out.content("text", QString("hello")));
		QVERIFY(out.content("flag", true));

		VipXIStringArchive in(out.toString());
		QEXPECT_FAIL("", "several top-level contents, no root element, unreadable document", Abort);
		QVERIFY(in.isOpen());

		int number = 0;
		double real = 0.0;
		QString text;
		bool flag = false;
		QVERIFY(in.content("number", number));
		QVERIFY(in.content("real", real));
		QVERIFY(in.content("text", text));
		QVERIFY(in.content("flag", flag));

		QCOMPARE(number, 42);
		QCOMPARE(real, 3.5);
		QCOMPARE(text, QString("hello"));
		QCOMPARE(flag, true);
	}

	/// Counterpart of the previous test: the same values written under an
	/// explicit root node do round trip. This bounds the defect.
	void xmlRoundTripMultipleContentsUnderRoot()
	{
		VipXOStringArchive out;
		QVERIFY(out.start("root"));
		QVERIFY(out.content("number", 42));
		QVERIFY(out.content("real", 3.5));
		QVERIFY(out.content("text", QString("hello")));
		QVERIFY(out.content("flag", true));
		QVERIFY(out.end());

		VipXIStringArchive in(out.toString());
		QVERIFY(in.isOpen());
		QVERIFY(in.start("root"));

		int number = 0;
		double real = 0.0;
		QString text;
		bool flag = false;
		QVERIFY(in.content("number", number));
		QVERIFY(in.content("real", real));
		QVERIFY(in.content("text", text));
		QVERIFY(in.content("flag", flag));
		QVERIFY(in.end());

		QCOMPARE(number, 42);
		QCOMPARE(real, 3.5);
		QCOMPARE(text, QString("hello"));
		QCOMPARE(flag, true);
	}

	/// A QByteArray holding null and non-ASCII bytes: this is what tells a
	/// transparent serialisation from one that goes through a text conversion.
	void xmlRoundTripBinaryPayload()
	{
		QByteArray payload;
		payload.append('\0');
		payload.append('\x01');
		payload.append('\xff');
		payload.append("end", 3);
		QCOMPARE(payload.size(), 6);

		VipXOStringArchive out;
		QVERIFY(out.content("payload", payload));

		VipXIStringArchive in(out.toString());
		QByteArray read;
		QVERIFY(in.content("payload", read));
		QCOMPARE(read, payload);
	}

	/// Nested nodes read back in the same order.
	void xmlNestedNodes()
	{
		VipXOStringArchive out;
		QVERIFY(out.start("root"));
		QVERIFY(out.content("a", 1));
		QVERIFY(out.start("child"));
		QVERIFY(out.content("b", 2));
		QVERIFY(out.end());
		QVERIFY(out.end());

		VipXIStringArchive in(out.toString());
		int a = 0, b = 0;
		QVERIFY(in.start("root"));
		QVERIFY(in.content("a", a));
		QVERIFY(in.start("child"));
		QVERIFY(in.content("b", b));
		QVERIFY(in.end());
		QVERIFY(in.end());
		QCOMPARE(a, 1);
		QCOMPARE(b, 2);
	}

	/// Reading a missing name must set the error flag, and the archive must stay
	/// usable once resetError() is called. Every tolerant reader relies on this.
	void readMissingNameSetsErrorAndRecovers()
	{
		VipXOStringArchive out;
		QVERIFY(out.content("present", 7));

		VipXIStringArchive in(out.toString());
		int missing = -1;
		in.content("missing", missing);
		QVERIFY2(in.hasError(), "reading a missing name must raise the error flag");

		in.resetError();
		QVERIFY(!in.hasError());

		int present = 0;
		QVERIFY(in.content("present", present));
		QCOMPARE(present, 7);
	}

	/// A malformed XML buffer must neither open nor bring the process down.
	void malformedXmlDoesNotOpen()
	{
		VipXIStringArchive in;
		QVERIFY2(!in.open(QString("<root><unclosed>")), "malformed XML must not open");
		QVERIFY(!in.isOpen());
	}

	/// An empty buffer behaves like a malformed one.
	void emptyXmlDoesNotOpen()
	{
		VipXIStringArchive in;
		QVERIFY(!in.open(QString()));
		QVERIFY(!in.isOpen());
	}

	/// Round trip through a binary archive, on the same values as the XML test.
	void binaryRoundTripScalars()
	{
		QByteArray buffer;
		{
			VipBinaryArchive out(&buffer, QIODevice::WriteOnly);
			QVERIFY(out.content("number", 42));
			QVERIFY(out.content("real", 3.5));
			QVERIFY(out.content("text", QString("hello")));
		}

		VipBinaryArchive in(buffer);
		int number = 0;
		double real = 0.0;
		QString text;
		QVERIFY(in.content("number", number));
		QVERIFY(in.content("real", real));
		QVERIFY(in.content("text", text));

		QCOMPARE(number, 42);
		QCOMPARE(real, 3.5);
		QCOMPARE(text, QString("hello"));
	}

	/// Arbitrary bytes fed to a binary archive must not bring the process down.
	/// Nothing is assumed about the result, only that the call returns.
	void binaryGarbageDoesNotCrash()
	{
		QByteArray garbage("\xde\xad\xbe\xef\x00\x01\x02\x03garbage", 15);
		VipBinaryArchive in(garbage);
		int value = 0;
		in.content("anything", value);
		QVERIFY(true); // the call returned
	}
};

VIP_TEST_MAIN(TestArchive)
#include "TestArchive.moc"
