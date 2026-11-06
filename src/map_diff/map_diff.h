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

#ifndef OPENORIENTEERING_MAP_DIFF_H
#define OPENORIENTEERING_MAP_DIFF_H

#include <vector>

#include <QRectF>
#include <QString>

class QImage;

namespace OpenOrienteering {

class Map;
class MapColor;
class Object;
class Symbol;


/**
 * The differences between two versions of a map.
 *
 * Objects have no persistent identity in Mapper's file formats, so objects are
 * matched heuristically:
 *
 *  1. An old and a new object with the same symbol (by number) and equal
 *     geometry, rotation and text are unchanged.
 *  2. Remaining objects with the same symbol whose extents overlap enough are
 *     paired as "modified" (best overlap first).
 *  3. Everything else is "added" or "removed".
 *
 * Symbols are matched by number, colors by name.
 *
 * The diff refers to objects owned by the compared maps, so it must not
 * outlive them.
 */
class MapDiff
{
public:
	enum Kind { Added, Removed, Modified };

	struct ObjectChange
	{
		Kind kind;
		const Object* old_object;  ///< nullptr for Added
		const Object* new_object;  ///< nullptr for Removed
	};

	struct SymbolChange
	{
		Kind kind;
		const Symbol* old_symbol;
		const Symbol* new_symbol;
	};

	struct ColorChange
	{
		Kind kind;
		const MapColor* old_color;
		const MapColor* new_color;
	};

	/// Compares two maps.
	static MapDiff compare(const Map& old_map, const Map& new_map);

	bool isEmpty() const;

	/// The bounding box (in map coordinates) of all changed objects.
	QRectF changedExtent() const;

	/// A plain-text summary and list of all changes.
	QString report(const Map& new_map) const;

	/**
	 * Renders an overview image of the changes.
	 *
	 * The new map is drawn faintly as context. Removed objects are drawn in
	 * red, added objects in green, and modified objects in orange (new state)
	 * over red (old state). Each change is also marked by a circle, so that
	 * small changes are visible at overview scale.
	 *
	 * @param area           The area to render, in map coordinates (mm).
	 * @param pixels_per_mm  The resolution.
	 */
	QImage render(const Map& old_map, Map& new_map, const QRectF& area, qreal pixels_per_mm) const;

	std::vector<ObjectChange> objects;
	std::vector<SymbolChange> symbols;
	std::vector<ColorChange> colors;
	int unchanged_objects = 0;
};


}  // namespace OpenOrienteering

#endif  // OPENORIENTEERING_MAP_DIFF_H
