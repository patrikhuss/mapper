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

#ifndef OPENORIENTEERING_TEMPLATE_SOURCE_H
#define OPENORIENTEERING_TEMPLATE_SOURCE_H

#include <QString>

class QXmlStreamReader;
class QXmlStreamWriter;

namespace OpenOrienteering {


/**
 * Information about where a template's data comes from and how it may be used.
 *
 * Templates are often based on data from third parties, e.g. orthophotos
 * from a national mapping agency, or older maps from another club.
 * Such data comes with terms of use, may require an attribution, and
 * becomes outdated. This information is stored with the template in the map
 * file, so that everyone who works on the map knows about it.
 *
 * All fields are free text. Dates are expected in ISO 8601 format (YYYY-MM-DD,
 * YYYY-MM or YYYY), but this is not enforced.
 */
struct TemplateSource
{
	QString provider;     ///< Who provides the data, e.g. a mapping agency.
	QString title;        ///< The product or dataset, e.g. "Orthophoto 2025".
	QString captured;     ///< When the data was captured (e.g. date of the aerial photo).
	QString acquired;     ///< When the data was obtained from the provider.
	QString license;      ///< License or terms of use.
	QString attribution;  ///< Attribution text required by the provider.
	QString permitted_use;///< What the data may be used for (e.g. training maps, print).
	QString reference;    ///< Order number, agreement, or contact for the data.
	QString url;          ///< Where the data or its terms can be found.
	QString notes;        ///< Anything else.

	/// Returns true if no field is set.
	bool isEmpty() const;

	/**
	 * Returns a short, human-readable summary, e.g. for tooltips.
	 *
	 * Returns an empty string if the source is empty.
	 */
	QString summary() const;

	/// Writes a "source" element. Nothing is written if the source is empty.
	void save(QXmlStreamWriter& xml) const;

	/// Reads a "source" element. The reader must be positioned at its start.
	static TemplateSource load(QXmlStreamReader& xml);
};

bool operator==(const TemplateSource& lhs, const TemplateSource& rhs);

inline bool operator!=(const TemplateSource& lhs, const TemplateSource& rhs)
{
	return !(lhs == rhs);
}


}  // namespace OpenOrienteering

#endif // OPENORIENTEERING_TEMPLATE_SOURCE_H
