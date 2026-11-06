/*
 *    Copyright 2026 Patrik Huss
 *
 *    This file is part of OpenOrienteering.
 *
 *    OpenOrienteering is free software: you can redistribute it and/or modify
 *    it under the terms of the GNU General Public License as published by
 *    the Free Software Foundation, either version 3 of the License, or
 *    (at your option) any later version.
 *
 *    OpenOrienteering is distributed in the hope that it will be useful,
 *    but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *    GNU General Public License for more details.
 *
 *    You should have received a copy of the GNU General Public License
 *    along with OpenOrienteering.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "map_diff_t.h"

#include <algorithm>

#include <QtTest>
#include <QColor>
#include <QDir>
#include <QImage>
#include <QLatin1String>
#include <QRectF>
#include <QString>

#include "test_config.h"

#include "global.h"
#include "core/map.h"
#include "core/map_part.h"
#include "core/objects/object.h"
#include "core/objects/text_object.h"
#include "core/symbols/symbol.h"
#include "map_diff/map_diff.h"

using namespace OpenOrienteering;


namespace {

/// A map with 12 objects: 3 lines, 3 areas, 3 points, 3 texts,
/// each with its own symbol.
QString testMapPath()
{
	return QDir(QString::fromUtf8(MAPPER_TEST_SOURCE_DIR))
	       .absoluteFilePath(QStringLiteral("data/helper-hidden-symbols.omap"));
}

Object* findObject(Map& map, const char* symbol_number)
{
	auto* part = map.getPart(0);
	for (int i = 0; i < part->getNumObjects(); ++i)
	{
		auto* object = part->getObject(i);
		if (object->getSymbol()->getNumberAsString() == QLatin1String(symbol_number))
			return object;
	}
	return nullptr;
}

Symbol* findSymbol(Map& map, const char* symbol_number)
{
	for (int i = 0; i < map.getNumSymbols(); ++i)
	{
		if (map.getSymbol(i)->getNumberAsString() == QLatin1String(symbol_number))
			return map.getSymbol(i);
	}
	return nullptr;
}

int count(const MapDiff& diff, MapDiff::Kind kind)
{
	return int(std::count_if(begin(diff.objects), end(diff.objects), [kind](const auto& change) {
		return change.kind == kind;
	}));
}

/// Applies a known set of edits to the test map.
void editMap(Map& map)
{
	// Removed
	auto* text_1 = findObject(map, "603.1");
	QVERIFY(text_1);
	map.deleteObject(text_1);

	// Modified: moved by 0.3 mm
	auto* area_1 = findObject(map, "410.1");
	QVERIFY(area_1);
	area_1->move(300, 300);

	// Modified: new text
	auto* text_2 = static_cast<TextObject*>(findObject(map, "603.2"));
	QVERIFY(text_2);
	text_2->setText(QStringLiteral("Changed"));

	// Added: a copy of a point, far away from the original
	auto* point = findObject(map, "526");
	QVERIFY(point);
	auto* copy = point->duplicate();
	copy->move(50000, 50000);
	map.addObject(copy);

	// Modified symbol
	auto* symbol = findSymbol(map, "530");
	QVERIFY(symbol);
	symbol->setName(QStringLiteral("Renamed point"));
}

}  // namespace



void MapDiffTest::initTestCase()
{
	Q_INIT_RESOURCE(resources);
	doStaticInitializations();
}


void MapDiffTest::identicalMaps()
{
	Map old_map;
	Map new_map;
	QVERIFY(old_map.loadFrom(testMapPath()));
	QVERIFY(new_map.loadFrom(testMapPath()));
	QCOMPARE(old_map.getNumObjects(), 12);

	auto const diff = MapDiff::compare(old_map, new_map);
	QVERIFY(diff.isEmpty());
	QCOMPARE(diff.unchanged_objects, 12);
}


void MapDiffTest::detectsChanges()
{
	Map old_map;
	Map new_map;
	QVERIFY(old_map.loadFrom(testMapPath()));
	QVERIFY(new_map.loadFrom(testMapPath()));
	editMap(new_map);

	auto const diff = MapDiff::compare(old_map, new_map);
	QCOMPARE(count(diff, MapDiff::Removed), 1);
	QCOMPARE(count(diff, MapDiff::Modified), 2);
	QCOMPARE(count(diff, MapDiff::Added), 1);
	QCOMPARE(diff.unchanged_objects, 9);

	for (const auto& change : diff.objects)
	{
		const auto* object = change.new_object ? change.new_object : change.old_object;
		auto const number = object->getSymbol()->getNumberAsString();
		switch (change.kind)
		{
		case MapDiff::Removed:
			QCOMPARE(number, QStringLiteral("603.1"));
			break;
		case MapDiff::Modified:
			QVERIFY2(number == QLatin1String("410.1") || number == QLatin1String("603.2"), qPrintable(number));
			break;
		case MapDiff::Added:
			QCOMPARE(number, QStringLiteral("526"));
			break;
		}
	}

	QCOMPARE(int(diff.symbols.size()), 1);
	QCOMPARE(diff.symbols.front().kind, MapDiff::Modified);
	QCOMPARE(diff.symbols.front().new_symbol->getNumberAsString(), QStringLiteral("530"));
	QVERIFY(diff.colors.empty());

	auto const report = diff.report(new_map);
	QVERIFY(report.contains(QLatin1String("1 added, 1 removed, 2 modified, 9 unchanged")));
	QVERIFY(report.contains(QLatin1String("text \"")));
}


void MapDiffTest::rendersChanges()
{
	Map old_map;
	Map new_map;
	QVERIFY(old_map.loadFrom(testMapPath()));
	QVERIFY(new_map.loadFrom(testMapPath()));
	editMap(new_map);

	auto const diff = MapDiff::compare(old_map, new_map);
	auto area = diff.changedExtent();
	QVERIFY(area.isValid());
	area.adjust(-5, -5, 5, 5);

	auto const image = diff.render(old_map, new_map, area, 4.0).convertToFormat(QImage::Format_RGB32);
	QCOMPARE(image.width(), qCeil(area.width() * 4.0));
	QCOMPARE(image.height(), qCeil(area.height() * 4.0));

	// The colors of added, removed and modified objects must appear.
	auto const contains_color = [&image](QRgb rgb) {
		for (int y = 0; y < image.height(); ++y)
		{
			auto const* line = reinterpret_cast<const QRgb*>(image.constScanLine(y));
			if (std::any_of(line, line + image.width(), [rgb](QRgb pixel) { return (pixel & 0xffffff) == (rgb & 0xffffff); }))
				return true;
		}
		return false;
	};
	QVERIFY(contains_color(QColor(0x1a, 0x98, 0x50).rgb()));  // added
	QVERIFY(contains_color(QColor(0xd7, 0x30, 0x27).rgb()));  // removed
	QVERIFY(contains_color(QColor(0xf2, 0x8e, 0x00).rgb()));  // modified
}


/*
 * We don't need a real GUI window.
 */
#ifndef Q_OS_MACOS
namespace  {
	auto Q_DECL_UNUSED qpa_selected = qputenv("QT_QPA_PLATFORM", "offscreen");  // clazy:exclude=non-pod-global-static
}
#endif


QTEST_MAIN(MapDiffTest)
