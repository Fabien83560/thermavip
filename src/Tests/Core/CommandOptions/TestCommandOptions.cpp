/// @file TestCommandOptions.cpp
///
/// Characterisation tests for the command line parser, which is the first
/// untrusted input the application handles.

#include <QTest>

#include "vip_test_main.h"

#include "VipCommandOptions.h"

class TestCommandOptions : public QObject
{
	Q_OBJECT

private Q_SLOTS:

	/// An empty argument reaches the parser whenever a script interpolates an
	/// unset variable between quotes. The first character of every argument was
	/// read before anything checked the length, which is out of bounds on an
	/// empty string: an assertion in debug builds, an indeterminate read
	/// otherwise. Parsing must simply return.
	void emptyArgumentIsNotIndexed()
	{
		VipCommandOptions& options = VipCommandOptions::instance();
		options.add("help");

		options.parse(QStringList() << "app" << QString() << "--help");

		QVERIFY2(options.count("help") > 0, "the flag after the empty argument must still be seen");
		QVERIFY2(options.positional().contains(QString()), "the empty argument is positional");
	}

	/// Same argument in the position where an optional value is looked for.
	void emptyArgumentAfterOptionIsNotIndexed()
	{
		VipCommandOptions& options = VipCommandOptions::instance();
		options.add("output", QString(), VipCommandOptions::ValueOptional);

		options.parse(QStringList() << "app" << "--output" << QString());

		QVERIFY(options.count("output") > 0);
	}

	/// A lone prefix is neither a flag nor a value, and must stay positional.
	void lonePrefixStaysPositional()
	{
		VipCommandOptions& options = VipCommandOptions::instance();

		options.parse(QStringList() << "app" << "-");

		QVERIFY(options.positional().contains(QStringLiteral("-")));
	}
};

VIP_TEST_MAIN(TestCommandOptions)
#include "TestCommandOptions.moc"
