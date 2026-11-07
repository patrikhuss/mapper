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

#ifndef OPENORIENTEERING_TEMPLATE_COLLECTOR_H
#define OPENORIENTEERING_TEMPLATE_COLLECTOR_H

#include <vector>

#include <QHash>
#include <QString>
#include <QStringList>

namespace OpenOrienteering {

class Map;
class Template;


/**
 * Copies the template files of a map into a folder next to the map file.
 *
 * Mapper stores templates by reference. When a map is shared or put under
 * version control, template files from elsewhere on the disk are missing.
 * This class collects them into a subfolder of the map's directory
 * (by default "templates"), together with their companion files
 * (world files, projection files, GDAL auxiliary files, shapefile parts),
 * and updates the template paths. The map must be saved afterwards.
 *
 * Usage: Create the collector, inspect plan(), then call collect().
 */
class TemplateCollector
{
public:
	/// What happens, or happened, to a template.
	enum Status
	{
		Copy,           ///< The file will be copied / was copied.
		Reuse,          ///< An identical file already exists in the target folder.
		WriteOnSave,    ///< The file does not exist yet, it will be written there when the map is saved.
		InMapFolder,    ///< The file is already in the map's folder; nothing to do.
		Missing,        ///< The template file cannot be found.
		Unsupported,    ///< The file refers to other files (e.g. GDAL VRT) and cannot be copied safely.
		Failed,         ///< Copying failed.
	};

	struct Item
	{
		Template* temp;
		Status status;
		QString source_path;      ///< The current template file
		QString target_path;      ///< The new template file
		QStringList companions;   ///< Companion files which are copied, too (source paths)
		QString error;            ///< Error message for Failed
	};

	/**
	 * Plans the collection of all open templates of the map.
	 *
	 * @param map       The map.
	 * @param map_path  The path of the map file. Must not be empty.
	 * @param subfolder The folder for templates, relative to the map's directory.
	 */
	TemplateCollector(Map& map, const QString& map_path, const QString& subfolder = QStringLiteral("templates"));

	/// The planned (or, after collect(), the actual) result for each template.
	const std::vector<Item>& items() const { return plan_items; }

	/// Returns the number of templates which will be moved to the folder.
	int pendingCount() const;

	/// The absolute path of the target folder.
	const QString& targetFolder() const { return target_folder; }

	/**
	 * Copies the files and updates the template paths.
	 *
	 * @return true if all pending templates were collected.
	 */
	bool collect();

	/**
	 * Returns the companion files of a template file which exist on disk.
	 *
	 * Companion files have the same base name, e.g. "image.pgw", "image.prj",
	 * "image.png.aux.xml" for "image.png".
	 */
	static QStringList companionFiles(const QString& path);

private:
	Item planItem(Template* temp);

	Map& map;
	QString map_folder;
	QString target_folder;
	std::vector<Item> plan_items;
	QHash<QString, QString> planned_targets;  ///< Planned target path for each source path
};


}  // namespace OpenOrienteering

#endif // OPENORIENTEERING_TEMPLATE_COLLECTOR_H
