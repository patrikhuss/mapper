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
#include <QLatin1String>
#include <QStringList>
#include <QXmlStreamAttributes>
#include <QXmlStreamReader>
#include <QXmlStreamWriter>


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
