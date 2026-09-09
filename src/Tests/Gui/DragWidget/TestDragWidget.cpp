/// @file TestDragWidget.cpp
///
/// Characterisation tests for the part of the workspace layout that is rebuilt
/// from a session file: counts that drive widget creation, and integers cast
/// into enumerations.

#include <QTest>

#include "vip_test_main.h"

#include "VipDragWidget.h"
#include "VipXmlArchive.h"

class TestDragWidget : public QObject
{
	Q_OBJECT

	/// The XML archive only reads back a document with a single top-level node,
	/// so every fixture writes its fields under one.
	static QString underRoot(const std::function<void(VipXOStringArchive&)>& write)
	{
		VipXOStringArchive out;
		out.start("root");
		write(out);
		out.end();
		return out.toString();
	}

private Q_SLOTS:

	/// A layout round trips: one row, one column, no widget.
	void emptyLayoutRoundTrip()
	{
		VipMultiDragWidget source;
		source.mainResize(2);
		source.subResize(0, 1);

		VipXOStringArchive out;
		out.start("root");
		out.content(&source);
		out.end();

		VipXIStringArchive in(out.toString());
		QVERIFY(in.isOpen());
		QVERIFY(in.start("root"));

		VipMultiDragWidget read;
		in.content(&read);

		QCOMPARE(read.mainCount(), 2);
	}

	/// Every row builds a splitter with its tab widget, every column a tab
	/// widget, and every tab a widget. None of the three counts was bounded, so a
	/// session file announcing a height of 100000 built hundreds of thousands of
	/// widgets and froze the interface. An implausible count must be refused.
	void implausibleRowCountIsRefused()
	{
		const QString xml = underRoot([](VipXOStringArchive& out) {
			out.content("pos", QPoint(0, 0));
			out.content("size", QSize(100, 100));
			out.content("saved_geometry", QRect(0, 0, 100, 100));
			out.content("state", QByteArray());
			out.content("height", 100000);
			out.content("visibility", 0);
		});

		VipXIStringArchive in(xml);
		QVERIFY(in.isOpen());
		QVERIFY(in.start("root"));

		// A fresh widget already carries its sentinel row: the read must add none.
		const VipMultiDragWidget untouched;
		VipMultiDragWidget read;
		operator>>(static_cast<VipArchive&>(in), &read);

		QCOMPARE(read.mainCount(), untouched.mainCount());
	}

	/// Same guard on the negative side.
	void negativeRowCountIsRefused()
	{
		const QString xml = underRoot([](VipXOStringArchive& out) {
			out.content("pos", QPoint(0, 0));
			out.content("size", QSize(100, 100));
			out.content("saved_geometry", QRect(0, 0, 100, 100));
			out.content("state", QByteArray());
			out.content("height", -5);
			out.content("visibility", 0);
		});

		VipXIStringArchive in(xml);
		QVERIFY(in.start("root"));

		// A fresh widget already carries its sentinel row: the read must add none.
		const VipMultiDragWidget untouched;
		VipMultiDragWidget read;
		operator>>(static_cast<VipArchive&>(in), &read);

		QCOMPARE(read.mainCount(), untouched.mainCount());
	}

	/// VisibilityState defines three values. Converting any other integer is
	/// undefined, and in practice produced a state no branch recognises, which
	/// left the window stuck. An unknown state must fall back to Normal.
	void visibilityOutsideEnumerationFallsBack()
	{
		const QString xml = underRoot([](VipXOStringArchive& out) {
			out.content("id", 1);
			out.content("title", QString("title"));
			out.content("operations", static_cast<int>(VipBaseDragWidget::AllOperations));
			out.content("visibility", 99);
		});

		VipXIStringArchive in(xml);
		QVERIFY(in.start("root"));

		VipDragWidget widget;
		operator>>(static_cast<VipArchive&>(in), static_cast<VipBaseDragWidget*>(&widget));

		QCOMPARE(widget.visibility(), VipBaseDragWidget::Normal);
	}

	/// Operation bits that the enumeration does not define must not survive the
	/// read either.
	void undefinedOperationBitsAreDropped()
	{
		const QString xml = underRoot([](VipXOStringArchive& out) {
			out.content("id", 2);
			out.content("title", QString("title"));
			out.content("operations", static_cast<int>(0x7fffffff));
			out.content("visibility", 0);
		});

		VipXIStringArchive in(xml);
		QVERIFY(in.start("root"));

		VipDragWidget widget;
		operator>>(static_cast<VipArchive&>(in), static_cast<VipBaseDragWidget*>(&widget));

		constexpr int defined = VipBaseDragWidget::AllOperations | VipBaseDragWidget::NoHideOnMaximize;
		QCOMPARE(static_cast<int>(widget.supportedOperations()) & ~defined, 0);
	}

	/// A known state still round trips.
	void knownVisibilityIsKept()
	{
		const QString xml = underRoot([](VipXOStringArchive& out) {
			out.content("id", 3);
			out.content("title", QString("title"));
			out.content("operations", static_cast<int>(VipBaseDragWidget::AllOperations));
			out.content("visibility", static_cast<int>(VipBaseDragWidget::Minimized));
		});

		VipXIStringArchive in(xml);
		QVERIFY(in.start("root"));

		VipDragWidget widget;
		operator>>(static_cast<VipArchive&>(in), static_cast<VipBaseDragWidget*>(&widget));

		QCOMPARE(widget.visibility(), VipBaseDragWidget::Minimized);
	}
};

VIP_TEST_MAIN(TestDragWidget)
#include "TestDragWidget.moc"
