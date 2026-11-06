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

/*
 * mapper-diff: Compare two versions of an OpenOrienteering Mapper map.
 *
 * Exit status as for diff(1): 0 = no differences, 1 = differences, 2 = error.
 */

#include <cmath>

#include <QtGlobal>
#include <QApplication>
#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QImage>
#include <QRectF>
#include <QString>
#include <QTextStream>

#include "global.h"
#include "core/map.h"
#include "map_diff/map_diff.h"

using namespace OpenOrienteering;


namespace {

QTextStream& err()
{
	static QTextStream stream(stderr);
	return stream;
}

bool loadMap(Map& map, const QString& path)
{
	if (map.loadFrom(path))
		return true;
	err() << "mapper-diff: cannot load " << path << Qt::endl;
	return false;
}

}  // namespace


int main(int argc, char* argv[])
{
	// Rendering needs a GUI application, but no display.
	if (qEnvironmentVariableIsEmpty("QT_QPA_PLATFORM"))
		qputenv("QT_QPA_PLATFORM", "offscreen");

	QApplication app(argc, argv);
	QApplication::setApplicationName(QStringLiteral("mapper-diff"));

	QCommandLineParser parser;
	parser.setApplicationDescription(QStringLiteral(
	  "Compare two versions of an OpenOrienteering Mapper map (.omap, .xmap, .ocd).\n"
	  "Exit status: 0 = no differences, 1 = differences, 2 = error."));
	parser.addHelpOption();
	parser.addPositionalArgument(QStringLiteral("old"), QStringLiteral("The old version of the map."));
	parser.addPositionalArgument(QStringLiteral("new"), QStringLiteral("The new version of the map."));
	QCommandLineOption output_option({ QStringLiteral("o"), QStringLiteral("output") },
	  QStringLiteral("Write an image of the changes to <file> (PNG, JPEG, ...)."), QStringLiteral("file"));
	QCommandLineOption resolution_option(QStringLiteral("resolution"),
	  QStringLiteral("Image resolution in pixels per map millimeter (default: 10)."), QStringLiteral("px/mm"),
	  QStringLiteral("10"));
	QCommandLineOption area_option(QStringLiteral("area"),
	  QStringLiteral("Image area: 'map' (whole map, default) or 'changes'."), QStringLiteral("area"),
	  QStringLiteral("map"));
	QCommandLineOption margin_option(QStringLiteral("margin"),
	  QStringLiteral("Margin around the image area in map millimeters (default: 5)."), QStringLiteral("mm"),
	  QStringLiteral("5"));
	QCommandLineOption quiet_option({ QStringLiteral("q"), QStringLiteral("quiet") },
	  QStringLiteral("Do not print the report."));
	parser.addOptions({ output_option, resolution_option, area_option, margin_option, quiet_option });
	parser.process(app);

	auto const args = parser.positionalArguments();
	if (args.size() != 2)
	{
		err() << parser.helpText() << Qt::flush;
		return 2;
	}

	auto ok = true;
	auto const resolution = parser.value(resolution_option).toDouble(&ok);
	if (!ok || resolution <= 0 || resolution > 100)
	{
		err() << "mapper-diff: invalid resolution" << Qt::endl;
		return 2;
	}
	auto const margin = parser.value(margin_option).toDouble(&ok);
	if (!ok || margin < 0)
	{
		err() << "mapper-diff: invalid margin" << Qt::endl;
		return 2;
	}
	auto const area_mode = parser.value(area_option);
	if (area_mode != QLatin1String("map") && area_mode != QLatin1String("changes"))
	{
		err() << "mapper-diff: --area must be 'map' or 'changes'" << Qt::endl;
		return 2;
	}

	Q_INIT_RESOURCE(resources);
	doStaticInitializations();

	Map old_map;
	Map new_map;
	if (!loadMap(old_map, args[0]) || !loadMap(new_map, args[1]))
		return 2;

	auto const diff = MapDiff::compare(old_map, new_map);

	if (!parser.isSet(quiet_option))
		QTextStream(stdout) << diff.report(new_map);

	if (parser.isSet(output_option))
	{
		QRectF area = (area_mode == QLatin1String("changes") && !diff.objects.empty())
		              ? diff.changedExtent()
		              : old_map.calculateExtent(true).united(new_map.calculateExtent(true));
		area.adjust(-margin, -margin, margin, margin);

		// Keep the image at a sane size (max. 100 megapixels).
		auto pixels_per_mm = resolution;
		auto const megapixels = area.width() * area.height() * pixels_per_mm * pixels_per_mm / 1e6;
		if (megapixels > 100)
		{
			pixels_per_mm *= std::sqrt(100 / megapixels);
			err() << "mapper-diff: reducing resolution to " << pixels_per_mm << " px/mm" << Qt::endl;
		}

		auto const image = diff.render(old_map, new_map, area, pixels_per_mm);
		auto const path = parser.value(output_option);
		if (!image.save(path))
		{
			err() << "mapper-diff: cannot write " << path << Qt::endl;
			return 2;
		}
	}

	return diff.isEmpty() ? 0 : 1;
}
