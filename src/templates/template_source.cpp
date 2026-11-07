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

#include "template_source.h"

#include <QCoreApplication>
#include <QFile>
#include <QFileInfo>
#include <QIODevice>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QLatin1String>
#include <QStringList>
#include <QXmlStreamAttributes>
#include <QXmlStreamReader>
#include <QXmlStreamWriter>

#ifdef MAPPER_USE_GDAL
#  include "gdal/gdal_file.h"
#endif


namespace OpenOrienteering {

namespace {

namespace literal {
	const QLatin1String source("source");
	const QLatin1String provider("provider");
	const QLatin1String title("title");
	const QLatin1String captured("captured");
	const QLatin1String acquired("acquired");
	const QLatin1String license("license");
	const QLatin1String reference("reference");
	const QLatin1String url("url");
	const QLatin1String attribution("attribution");
	const QLatin1String permitted_use("permitted_use");
	const QLatin1String notes("notes");
}  // namespace literal

void writeAttributeIfSet(QXmlStreamWriter& xml, QLatin1String name, const QString& value)
{
	if (!value.isEmpty())
		xml.writeAttribute(name, value);
}

void writeElementIfSet(QXmlStreamWriter& xml, QLatin1String name, const QString& value)
{
	if (!value.isEmpty())
		xml.writeTextElement(name, value);
}

/// Returns the date part of an ISO 8601 or EXIF/TIFF date-time ("YYYY:MM:DD HH:MM:SS").
QString datePart(QString text)
{
	text = text.trimmed().left(10);
	if (text.length() == 10 && text.at(4) == QLatin1Char(':') && text.at(7) == QLatin1Char(':'))
	{
		text[4] = QLatin1Char('-');
		text[7] = QLatin1Char('-');
	}
	return text;
}

QString jsonText(const QJsonValue& value)
{
	if (value.isArray())
	{
		QStringList items;
		for (auto const& item : value.toArray())
			items.append(item.toString());
		items.removeAll(QString{});
		return items.join(QLatin1String(", "));
	}
	return value.toString().trimmed();
}

/// Reads a sidecar file "<data file>.source.json".
bool readSidecar(const QString& path, TemplateSource& source)
{
	QFile file(path + QLatin1String(".source.json"));
	if (!file.open(QIODevice::ReadOnly))
		return false;
	auto const json = QJsonDocument::fromJson(file.read(1 << 20)).object();
	if (json.isEmpty())
		return false;
	source.provider = jsonText(json.value(literal::provider));
	source.title = jsonText(json.value(literal::title));
	source.captured = jsonText(json.value(literal::captured));
	source.acquired = jsonText(json.value(literal::acquired));
	source.license = jsonText(json.value(literal::license));
	source.attribution = jsonText(json.value(literal::attribution));
	source.permitted_use = jsonText(json.value(literal::permitted_use));
	source.reference = jsonText(json.value(literal::reference));
	source.url = jsonText(json.value(literal::url));
	source.notes = jsonText(json.value(literal::notes));
	return !source.isEmpty();
}

/// Reads the header of an OpenStreetMap XML file.
void readOsm(const QString& path, TemplateSource& source)
{
	QFile file(path);
	if (!file.open(QIODevice::ReadOnly))
		return;
	QXmlStreamReader xml(&file);
	if (!xml.readNextStartElement() || xml.name() != QLatin1String("osm"))
		return;
	auto const attributes = xml.attributes();
	auto const copyright = attributes.value(QLatin1String("copyright")).toString();
	auto const license = attributes.value(QLatin1String("license")).toString();
	if (copyright.contains(QLatin1String("OpenStreetMap")))
	{
		source.provider = QStringLiteral("OpenStreetMap contributors");
		source.title = QStringLiteral("OpenStreetMap");
		source.attribution = QStringLiteral("© OpenStreetMap contributors");
	}
	else
	{
		source.attribution = copyright;
	}
	if (license.contains(QLatin1String("odbl")))
		source.license = QStringLiteral("ODbL 1.0");
	else
		source.license = license;
	source.url = attributes.value(QLatin1String("attribution")).toString();
}

/// Reads the metadata of a GPX file, and the time of the first point.
void readGpx(const QString& path, TemplateSource& source)
{
	QFile file(path);
	if (!file.open(QIODevice::ReadOnly))
		return;
	QXmlStreamReader xml(&file);
	if (!xml.readNextStartElement() || xml.name() != QLatin1String("gpx"))
		return;
	auto const creator = xml.attributes().value(QLatin1String("creator")).toString().trimmed();
	QString author;
	
	// The metadata is at the beginning. Stop at the first point's time stamp.
	QStringList parents;
	for (int elements = 0; elements < 10000 && !xml.atEnd(); )
	{
		auto const token = xml.readNext();
		if (token == QXmlStreamReader::EndElement)
		{
			if (!parents.isEmpty())
				parents.removeLast();
			continue;
		}
		if (token != QXmlStreamReader::StartElement)
			continue;
		++elements;
		
		auto const name = xml.name().toString();
		auto const parent = parents.isEmpty() ? QString{} : parents.last();
		if (name == QLatin1String("time"))
		{
			auto const date = datePart(xml.readElementText());
			if (source.captured.isEmpty())
				source.captured = date;
			if (parent == QLatin1String("trkpt") || parent == QLatin1String("rtept") || parent == QLatin1String("wpt"))
				break;  // Past the metadata
			continue;  // end element consumed
		}
		if (name == QLatin1String("name"))
		{
			auto const text = xml.readElementText().trimmed();
			if (parent == QLatin1String("author"))
				author = text;
			else if (source.title.isEmpty() && (parent == QLatin1String("metadata") || parent == QLatin1String("trk") || parent == QLatin1String("rte")))
				source.title = text;
			continue;  // end element consumed
		}
		if (name == QLatin1String("license"))
		{
			source.license = xml.readElementText().trimmed();
			continue;  // end element consumed
		}
		if (name == QLatin1String("copyright"))
		{
			auto const holder = xml.attributes().value(QLatin1String("author")).toString().trimmed();
			if (!holder.isEmpty())
				source.attribution = QStringLiteral("© ") + holder;
		}
		parents.append(name);
	}
	source.provider = author.isEmpty() ? creator : author;
}

#ifdef MAPPER_USE_GDAL
/// Reads TIFF tags (or equivalent metadata) of a raster file, via GDAL.
void readGdalMetadata(const QString& path, TemplateSource& source)
{
	auto const metadata = GdalFile::rasterMetadata(path.toUtf8());
	auto const item = [&metadata](QLatin1String key) {
		for (auto const& entry : metadata)
		{
			if (entry.startsWith(key) && entry.midRef(key.size()).startsWith(QLatin1Char('=')))
				return entry.mid(key.size() + 1).trimmed();
		}
		return QString{};
	};
	auto const copyright = item(QLatin1String("TIFFTAG_COPYRIGHT"));
	auto const artist = item(QLatin1String("TIFFTAG_ARTIST"));
	source.provider = artist.isEmpty() ? copyright : artist;
	source.attribution = copyright;
	source.captured = datePart(item(QLatin1String("TIFFTAG_DATETIME")));
	source.title = item(QLatin1String("TIFFTAG_DOCUMENTNAME"));
}
#endif

}  // namespace



bool TemplateSource::isEmpty() const
{
	return provider.isEmpty()
	       && title.isEmpty()
	       && captured.isEmpty()
	       && acquired.isEmpty()
	       && license.isEmpty()
	       && attribution.isEmpty()
	       && permitted_use.isEmpty()
	       && reference.isEmpty()
	       && url.isEmpty()
	       && notes.isEmpty();
}


QString TemplateSource::summary() const
{
	QStringList lines;
	auto heading = QStringList{provider, title};
	heading.removeAll(QString{});
	if (!heading.isEmpty())
		lines.append(heading.join(QLatin1String(" - ")));
	if (!captured.isEmpty())
		lines.append(QCoreApplication::translate("OpenOrienteering::TemplateSource", "Captured: %1").arg(captured));
	if (!license.isEmpty())
		lines.append(QCoreApplication::translate("OpenOrienteering::TemplateSource", "License: %1").arg(license));
	if (!permitted_use.isEmpty())
		lines.append(QCoreApplication::translate("OpenOrienteering::TemplateSource", "Permitted use: %1").arg(permitted_use));
	if (!attribution.isEmpty())
		lines.append(QCoreApplication::translate("OpenOrienteering::TemplateSource", "Attribution: %1").arg(attribution));
	return lines.join(QLatin1Char('\n'));
}


void TemplateSource::save(QXmlStreamWriter& xml) const
{
	if (isEmpty())
		return;

	xml.writeStartElement(literal::source);
	writeAttributeIfSet(xml, literal::provider, provider);
	writeAttributeIfSet(xml, literal::title, title);
	writeAttributeIfSet(xml, literal::captured, captured);
	writeAttributeIfSet(xml, literal::acquired, acquired);
	writeAttributeIfSet(xml, literal::license, license);
	writeAttributeIfSet(xml, literal::reference, reference);
	writeAttributeIfSet(xml, literal::url, url);
	writeElementIfSet(xml, literal::attribution, attribution);
	writeElementIfSet(xml, literal::permitted_use, permitted_use);
	writeElementIfSet(xml, literal::notes, notes);
	xml.writeEndElement(/*source*/);
}


TemplateSource TemplateSource::load(QXmlStreamReader& xml)
{
	Q_ASSERT(xml.name() == literal::source);

	TemplateSource source;
	auto const attributes = xml.attributes();
	source.provider = attributes.value(literal::provider).toString();
	source.title = attributes.value(literal::title).toString();
	source.captured = attributes.value(literal::captured).toString();
	source.acquired = attributes.value(literal::acquired).toString();
	source.license = attributes.value(literal::license).toString();
	source.reference = attributes.value(literal::reference).toString();
	source.url = attributes.value(literal::url).toString();
	while (xml.readNextStartElement())
	{
		if (xml.name() == literal::attribution)
			source.attribution = xml.readElementText();
		else if (xml.name() == literal::permitted_use)
			source.permitted_use = xml.readElementText();
		else if (xml.name() == literal::notes)
			source.notes = xml.readElementText();
		else
			xml.skipCurrentElement();  // unsupported
	}
	return source;
}


// static
TemplateSource TemplateSource::detect(const QString& path)
{
	TemplateSource source;
	if (readSidecar(path, source))
		return source;
	
	auto const suffix = QFileInfo(path).suffix().toLower();
	if (suffix == QLatin1String("osm"))
		readOsm(path, source);
	else if (suffix == QLatin1String("gpx"))
		readGpx(path, source);
#ifdef MAPPER_USE_GDAL
	else
		readGdalMetadata(path, source);
#endif
	return source;
}


bool operator==(const TemplateSource& lhs, const TemplateSource& rhs)
{
	return lhs.provider == rhs.provider
	       && lhs.title == rhs.title
	       && lhs.captured == rhs.captured
	       && lhs.acquired == rhs.acquired
	       && lhs.license == rhs.license
	       && lhs.attribution == rhs.attribution
	       && lhs.permitted_use == rhs.permitted_use
	       && lhs.reference == rhs.reference
	       && lhs.url == rhs.url
	       && lhs.notes == rhs.notes;
}


}  // namespace OpenOrienteering
