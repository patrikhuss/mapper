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

#include "template_collector.h"

#include <algorithm>
#include <iterator>

#include <QtGlobal>
#include <QByteArray>
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QIODevice>
#include <QLatin1Char>
#include <QLatin1String>

#include "core/map.h"
#include "templates/template.h"


namespace OpenOrienteering {

namespace {

/// Suffixes of companion files, appended to the template file's base name.
const char* const base_suffixes[] = {
    // World files
    ".wld", ".tfw", ".tifw", ".tiffw", ".jgw", ".jpgw", ".jpegw", ".pgw", ".pngw",
    ".gfw", ".gifw", ".bpw", ".bmpw", ".j2w",
    // Projection
    ".prj",
    // Shapefile parts
    ".shx", ".dbf", ".cpg", ".qix", ".sbn", ".sbx",
};

/// Suffixes of companion files, appended to the template file's full name.
const char* const name_suffixes[] = {
    ".aux.xml", ".ovr", ".msk",
};


/// Returns the part of the file name after the complete base name, e.g. ".png".
QString extensionPart(const QFileInfo& info)
{
	return info.fileName().mid(info.completeBaseName().length());
}

bool isInside(const QString& path, const QString& folder)
{
	auto const relative = QDir(folder).relativeFilePath(path);
	return !relative.startsWith(QLatin1String("..")) && !QDir::isAbsolutePath(relative);
}

bool sameContent(const QString& path1, const QString& path2)
{
	QFile file1(path1);
	QFile file2(path2);
	if (file1.size() != file2.size())
		return false;
	if (!file1.open(QIODevice::ReadOnly) || !file2.open(QIODevice::ReadOnly))
		return false;
	constexpr qint64 block_size = 1 << 20;
	while (!file1.atEnd())
	{
		if (file1.read(block_size) != file2.read(block_size))
			return false;
	}
	return true;
}

}  // namespace



// static
QStringList TemplateCollector::companionFiles(const QString& path)
{
	QFileInfo const info(path);
	auto const base = info.completeBaseName();
	auto const name = info.fileName();

	QStringList candidates;
	for (auto const* suffix : base_suffixes)
		candidates.append((base + QLatin1String(suffix)).toLower());
	for (auto const* suffix : name_suffixes)
		candidates.append((name + QLatin1String(suffix)).toLower());

	QStringList result;
	auto const dir = info.absoluteDir();
	auto const entries = dir.entryList(QDir::Files);
	for (auto const& entry : entries)
	{
		if (entry != name && candidates.contains(entry.toLower()))
			result.append(dir.absoluteFilePath(entry));
	}
	return result;
}



TemplateCollector::TemplateCollector(Map& map, const QString& map_path, const QString& subfolder)
: map(map)
{
	Q_ASSERT(!map_path.isEmpty());
	// The map file may not exist yet, but its folder must.
	auto const absolute_folder = QFileInfo(map_path).absolutePath();
	map_folder = QFileInfo(absolute_folder).canonicalFilePath();
	if (map_folder.isEmpty())
		map_folder = absolute_folder;
	target_folder = QDir::cleanPath(QDir(map_folder).absoluteFilePath(subfolder));

	for (int i = 0; i < map.getNumTemplates(); ++i)
		plan_items.push_back(planItem(map.getTemplate(i)));
}


int TemplateCollector::pendingCount() const
{
	return int(std::count_if(plan_items.cbegin(), plan_items.cend(), [](auto const& item) {
		return item.status == Copy || item.status == Reuse || item.status == WriteOnSave;
	}));
}


TemplateCollector::Item TemplateCollector::planItem(Template* temp)
{
	Item item { temp, Missing, temp->getTemplatePath(), {}, {}, {} };

	QFileInfo const info(item.source_path);
	if (info.exists())
	{
		item.source_path = info.canonicalFilePath();
		if (isInside(item.source_path, map_folder))
		{
			item.status = InMapFolder;
			return item;
		}
		if (info.suffix().compare(QLatin1String("vrt"), Qt::CaseInsensitive) == 0)
		{
			item.status = Unsupported;
			return item;
		}
		item.companions = companionFiles(item.source_path);
		item.status = Copy;
	}
	else if (temp->getTemplateState() != Template::Invalid && temp->hasUnsavedChanges())
	{
		item.status = WriteOnSave;
	}
	else
	{
		return item;
	}

	// The same file used by several templates is copied once.
	auto const planned = planned_targets.constFind(item.source_path);
	if (planned != planned_targets.constEnd())
	{
		item.target_path = *planned;
		item.status = Reuse;
		return item;
	}

	// Find a free name in the target folder, or an identical file.
	auto const base = info.completeBaseName();
	auto const extension = extensionPart(info);
	auto const target_dir = QDir(target_folder);
	for (int n = 1; ; ++n)
	{
		auto const new_base = n == 1 ? base : base + QLatin1Char('-') + QString::number(n);
		auto const target = target_dir.absoluteFilePath(new_base + extension);
		if (std::find(planned_targets.cbegin(), planned_targets.cend(), target) != planned_targets.cend())
			continue;
		if (QFileInfo::exists(target))
		{
			if (item.status == Copy && sameContent(target, item.source_path))
			{
				item.status = Reuse;
				item.target_path = target;
				break;
			}
			continue;
		}
		auto const companion_exists = std::any_of(item.companions.cbegin(), item.companions.cend(), [&](auto const& companion) {
			auto const suffix = QFileInfo(companion).fileName().mid(base.length());
			return QFileInfo::exists(target_dir.absoluteFilePath(new_base + suffix));
		});
		if (companion_exists)
			continue;
		item.target_path = target;
		break;
	}
	planned_targets.insert(item.source_path, item.target_path);
	return item;
}


bool TemplateCollector::collect()
{
	bool success = true;
	for (auto& item : plan_items)
	{
		if (item.status != Copy && item.status != Reuse && item.status != WriteOnSave)
			continue;

		if (!QDir().mkpath(target_folder))
		{
			item.status = Failed;
			item.error = QCoreApplication::translate("OpenOrienteering::TemplateCollector", "Cannot create folder %1").arg(target_folder);
			success = false;
			continue;
		}

		if (item.status == Copy && !QFileInfo::exists(item.target_path))
		{
			QFile source(item.source_path);
			if (!source.copy(item.target_path))
			{
				item.status = Failed;
				item.error = source.errorString();
				success = false;
				continue;
			}
		}

		// Companion files: copy those which are not there yet.
		QFileInfo const source_info(item.source_path);
		QFileInfo const target_info(item.target_path);
		auto const base_length = source_info.completeBaseName().length();
		for (auto const& companion : qAsConst(item.companions))
		{
			auto const suffix = QFileInfo(companion).fileName().mid(base_length);
			auto const target = target_info.absoluteDir().absoluteFilePath(target_info.completeBaseName() + suffix);
			if (!QFileInfo::exists(target))
				QFile::copy(companion, target);
		}

		item.temp->setTemplatePath(item.target_path);
		item.temp->setTemplateRelativePath(QDir(map_folder).relativeFilePath(item.target_path));
		map.emitTemplateChanged(item.temp);
	}
	return success;
}


}  // namespace OpenOrienteering
