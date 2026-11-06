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

#include "map_diff.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <memory>
#include <tuple>

#include <QtGlobal>
#include <QColor>
#include <QFont>
#include <QHash>
#include <QImage>
#include <QLineF>
#include <QPainter>
#include <QPen>
#include <QPointF>
#include <QSize>
#include <QtMath>
#include <QStringList>

#include "core/georeferencing.h"
#include "core/map.h"
#include "core/map_color.h"
#include "core/map_coord.h"
#include "core/map_part.h"
#include "core/objects/object.h"
#include "core/objects/text_object.h"
#include "core/renderables/renderable.h"
#include "core/symbols/symbol.h"


namespace OpenOrienteering {

namespace {

const QColor added_color    { 0x1a, 0x98, 0x50 };
const QColor removed_color  { 0xd7, 0x30, 0x27 };
const QColor modified_color { 0xf2, 0x8e, 0x00 };

/// Minimum overlap (intersection over union of the extents) for pairing
/// an old and a new object as "modified".
constexpr qreal min_overlap = 0.25;

/// Extents are inflated by this amount (mm) before computing the overlap,
/// so that small or thin objects which moved a little are still paired.
constexpr qreal overlap_margin = 0.5;

/// Rotation differences below this (radians, = 0.01 degrees) are ignored.
/// Object::equals() uses 1e-6 rad which is right within one file, but
/// converting between file formats (e.g. OCD to XML) causes larger rounding.
const qreal rotation_tolerance = qDegreesToRadians(0.01);


std::vector<const Object*> collectObjects(const Map& map)
{
	std::vector<const Object*> objects;
	objects.reserve(std::size_t(map.getNumObjects()));
	for (int p = 0; p < map.getNumParts(); ++p)
	{
		const auto* part = map.getPart(std::size_t(p));
		for (int i = 0; i < part->getNumObjects(); ++i)
		{
			const auto* object = part->getObject(i);
			object->update();  // needed for the extent
			objects.push_back(object);
		}
	}
	return objects;
}

QString symbolKey(const Object* object)
{
	const auto* symbol = object->getSymbol();
	return (symbol ? symbol->getNumberAsString() : QStringLiteral("-"))
	       + QLatin1Char('|') + QString::number(int(object->getType()));
}

/// A key which is equal for objects which may be equal.
/// It must match what Object::equals() compares.
QString exactKey(const Object* object)
{
	const auto& coords = object->getRawCoordinateVector();
	auto key = symbolKey(object);
	if (coords.empty())
		return key;
	auto const size = object->getType() == Object::Text ? std::size_t(0) : coords.size();
	return key + QLatin1Char('|') + QString::number(size)
	       + QLatin1Char('|') + QString::number(coords.front().nativeX())
	       + QLatin1Char('|') + QString::number(coords.front().nativeY());
}

/// Like Object::equals() without comparing the symbol,
/// but ignoring tiny rotation differences.
bool sameObject(const Object* a, const Object* b)
{
	if (a->equals(b, false))
		return true;
	auto const rotation_change = std::abs(a->getRotation() - b->getRotation());
	if (rotation_change == 0 || rotation_change > rotation_tolerance)
		return false;
	auto const copy = std::unique_ptr<Object>(b->duplicate());
	copy->setRotation(a->getRotation());
	return a->equals(copy.get(), false);
}

qreal overlap(QRectF a, QRectF b)
{
	a.adjust(-overlap_margin, -overlap_margin, overlap_margin, overlap_margin);
	b.adjust(-overlap_margin, -overlap_margin, overlap_margin, overlap_margin);
	auto const intersection = a.intersected(b);
	if (intersection.isEmpty())
		return 0;
	auto const area = [](const QRectF& r) { return r.width() * r.height(); };
	auto const intersection_area = area(intersection);
	return intersection_area / (area(a) + area(b) - intersection_area);
}

QString describe(const Symbol* symbol)
{
	if (!symbol)
		return QStringLiteral("(no symbol)");
	return symbol->getNumberAsString() + QLatin1Char(' ') + symbol->getPlainTextName();
}

QString kindLabel(MapDiff::Kind kind)
{
	switch (kind)
	{
	case MapDiff::Added:    return QStringLiteral("+ added   ");
	case MapDiff::Removed:  return QStringLiteral("- removed ");
	case MapDiff::Modified: return QStringLiteral("~ modified");
	}
	Q_UNREACHABLE();
}

/**
 * Pairs items of two lists by key, in order.
 *
 * Items which are not equal are reported as modified. This handles duplicate
 * keys (e.g. colors with the same name) without losing items.
 */
template <class T, class KeyFunction, class EqualFunction, class Change>
void pairByKey(const std::vector<const T*>& old_items, const std::vector<const T*>& new_items,
               KeyFunction key, EqualFunction equal, std::vector<Change>& changes)
{
	std::vector<bool> new_matched(new_items.size(), false);
	for (const auto* old_item : old_items)
	{
		// Prefer an equal item with the same key, then any item with the same key.
		auto found = new_items.size();
		for (std::size_t j = 0; j < new_items.size(); ++j)
		{
			if (new_matched[j] || key(new_items[j]) != key(old_item))
				continue;
			if (equal(old_item, new_items[j]))
			{
				found = j;
				break;
			}
			if (found == new_items.size())
				found = j;
		}
		if (found == new_items.size())
		{
			changes.push_back({ MapDiff::Removed, old_item, nullptr });
			continue;
		}
		new_matched[found] = true;
		if (!equal(old_item, new_items[found]))
			changes.push_back({ MapDiff::Modified, old_item, new_items[found] });
	}
	for (std::size_t j = 0; j < new_items.size(); ++j)
	{
		if (!new_matched[j])
			changes.push_back({ MapDiff::Added, nullptr, new_items[j] });
	}
}

}  // namespace



// static
MapDiff MapDiff::compare(const Map& old_map, const Map& new_map)
{
	MapDiff diff;

	// Objects, pass 1: unchanged objects
	auto const old_objects = collectObjects(old_map);
	auto const new_objects = collectObjects(new_map);

	QHash<QString, std::vector<std::size_t>> new_by_key;
	for (std::size_t j = 0; j < new_objects.size(); ++j)
		new_by_key[exactKey(new_objects[j])].push_back(j);

	std::vector<bool> new_matched(new_objects.size(), false);
	std::vector<std::size_t> unmatched_old;
	for (std::size_t i = 0; i < old_objects.size(); ++i)
	{
		auto const* old_object = old_objects[i];
		auto found = false;
		auto const candidates = new_by_key.constFind(exactKey(old_object));
		if (candidates != new_by_key.constEnd())
		{
			for (auto j : *candidates)
			{
				if (!new_matched[j] && sameObject(old_object, new_objects[j]))
				{
					new_matched[j] = true;
					found = true;
					break;
				}
			}
		}
		if (found)
			++diff.unchanged_objects;
		else
			unmatched_old.push_back(i);
	}

	// Objects, pass 2: modified objects (same symbol, overlapping extent)
	QHash<QString, std::vector<std::size_t>> unmatched_new_by_symbol;
	for (std::size_t j = 0; j < new_objects.size(); ++j)
	{
		if (!new_matched[j])
			unmatched_new_by_symbol[symbolKey(new_objects[j])].push_back(j);
	}

	std::vector<std::tuple<qreal, std::size_t, std::size_t>> pairs;
	for (auto i : unmatched_old)
	{
		auto const candidates = unmatched_new_by_symbol.constFind(symbolKey(old_objects[i]));
		if (candidates == unmatched_new_by_symbol.constEnd())
			continue;
		for (auto j : *candidates)
		{
			auto const score = overlap(old_objects[i]->getExtent(), new_objects[j]->getExtent());
			if (score >= min_overlap)
				pairs.emplace_back(score, i, j);
		}
	}
	std::sort(begin(pairs), end(pairs), [](const auto& a, const auto& b) {
		return std::get<0>(a) > std::get<0>(b);
	});

	std::vector<bool> old_paired(old_objects.size(), false);
	for (const auto& pair : pairs)
	{
		auto const i = std::get<1>(pair);
		auto const j = std::get<2>(pair);
		if (old_paired[i] || new_matched[j])
			continue;
		old_paired[i] = true;
		new_matched[j] = true;
		diff.objects.push_back({ Modified, old_objects[i], new_objects[j] });
	}

	// Objects, pass 3: the rest
	for (auto i : unmatched_old)
	{
		if (!old_paired[i])
			diff.objects.push_back({ Removed, old_objects[i], nullptr });
	}
	for (std::size_t j = 0; j < new_objects.size(); ++j)
	{
		if (!new_matched[j])
			diff.objects.push_back({ Added, nullptr, new_objects[j] });
	}
	std::stable_sort(begin(diff.objects), end(diff.objects), [](const auto& a, const auto& b) {
		return a.kind < b.kind;
	});

	// Symbols, by number
	std::vector<const Symbol*> old_symbols, new_symbols;
	for (int i = 0; i < old_map.getNumSymbols(); ++i)
		old_symbols.push_back(old_map.getSymbol(i));
	for (int i = 0; i < new_map.getNumSymbols(); ++i)
		new_symbols.push_back(new_map.getSymbol(i));
	pairByKey(old_symbols, new_symbols,
	          [](const Symbol* s) { return s->getNumberAsString(); },
	          [](const Symbol* a, const Symbol* b) { return a->equals(b); },
	          diff.symbols);

	// Colors, by name (priority changes are ignored)
	std::vector<const MapColor*> old_colors, new_colors;
	for (int i = 0; i < old_map.getNumColors(); ++i)
		old_colors.push_back(old_map.getColor(i));
	for (int i = 0; i < new_map.getNumColors(); ++i)
		new_colors.push_back(new_map.getColor(i));
	pairByKey(old_colors, new_colors,
	          [](const MapColor* c) { return c->getName(); },
	          [](const MapColor* a, const MapColor* b) { return a->equals(*b); },  // ignores priority
	          diff.colors);

	return diff;
}


bool MapDiff::isEmpty() const
{
	return objects.empty() && symbols.empty() && colors.empty();
}


QRectF MapDiff::changedExtent() const
{
	QRectF extent;
	for (const auto& change : objects)
	{
		for (const auto* object : { change.old_object, change.new_object })
		{
			if (object)
				extent = extent.isNull() ? object->getExtent() : extent.united(object->getExtent());
		}
	}
	return extent;
}


QString MapDiff::report(const Map& new_map) const
{
	int counts[3] = {};
	for (const auto& change : objects)
		++counts[change.kind];

	QStringList lines;
	lines << QStringLiteral("Objects: %1 added, %2 removed, %3 modified, %4 unchanged")
	         .arg(counts[Added]).arg(counts[Removed]).arg(counts[Modified]).arg(unchanged_objects);
	lines << QStringLiteral("Symbols: %1 changed, Colors: %2 changed")
	         .arg(symbols.size()).arg(colors.size());

	const auto& georef = new_map.getGeoreferencing();
	auto const geospatial = georef.getState() == Georeferencing::Geospatial;
	auto const position = [&](const Object* object) {
		auto const center = MapCoordF(object->getExtent().center());
		if (!geospatial)
			return QStringLiteral("at map x %1 y %2 mm")
			       .arg(center.x(), 0, 'f', 1).arg(center.y(), 0, 'f', 1);
		auto const projected = georef.toProjectedCoords(center);
		return QStringLiteral("at E %1 N %2")
		       .arg(projected.x(), 0, 'f', 0).arg(projected.y(), 0, 'f', 0);
	};

	if (!objects.empty())
	{
		lines << QString{} << QStringLiteral("Objects:");
		for (const auto& change : objects)
		{
			const auto* object = change.new_object ? change.new_object : change.old_object;
			auto line = QStringLiteral("  %1  %2  %3")
			            .arg(kindLabel(change.kind), describe(object->getSymbol()), position(object));
			if (change.kind != Modified && object->getType() == Object::Text)
			{
				line += QStringLiteral(" \"%1\"").arg(static_cast<const TextObject*>(object)->getText().left(40));
				if (object->getRotation() != 0)
					line += QStringLiteral(" rotated %1°").arg(qRadiansToDegrees(object->getRotation()), 0, 'f', 1);
			}
			if (change.kind == Modified)
			{
				QStringList details;
				auto const old_size = change.old_object->getRawCoordinateVector().size();
				auto const new_size = change.new_object->getRawCoordinateVector().size();
				if (old_size != new_size)
				{
					details << QStringLiteral("%1 -> %2 coords").arg(old_size).arg(new_size);
				}
				else
				{
					// Same number of coords: report how much they differ.
					auto const& old_coords = change.old_object->getRawCoordinateVector();
					auto const& new_coords = change.new_object->getRawCoordinateVector();
					qreal max_shift = 0;
					int flag_changes = 0;
					for (std::size_t i = 0; i < old_size; ++i)
					{
						max_shift = std::max(max_shift, QLineF(QPointF(old_coords[i]), QPointF(new_coords[i])).length());
						if (old_coords[i].flags() != new_coords[i].flags())
							++flag_changes;
					}
					if (max_shift > 0)
						details << QStringLiteral("coords shifted up to %1 mm").arg(max_shift, 0, 'g', 3);
					if (flag_changes > 0)
						details << QStringLiteral("%1 coord flags changed").arg(flag_changes);
				}
				auto const rotation_change = change.new_object->getRotation() - change.old_object->getRotation();
				if (rotation_change != 0)
					details << QStringLiteral("rotation %1°").arg(qRadiansToDegrees(rotation_change), 0, 'g', 3);
				if (object->getType() == Object::Text)
				{
					auto const& old_text = static_cast<const TextObject*>(change.old_object)->getText();
					auto const& new_text = static_cast<const TextObject*>(change.new_object)->getText();
					if (old_text != new_text)
						details << QStringLiteral("text \"%1\" -> \"%2\"").arg(old_text, new_text);
				}
				auto const shift = QLineF(change.old_object->getExtent().center(),
				                          change.new_object->getExtent().center()).length();
				if (shift >= 0.05)
					details << QStringLiteral("moved %1 mm").arg(shift, 0, 'f', 1);
				if (!details.empty())
					line += QStringLiteral(" (") + details.join(QStringLiteral(", ")) + QLatin1Char(')');
			}
			lines << line;
		}
	}

	if (!symbols.empty())
	{
		lines << QString{} << QStringLiteral("Symbols:");
		for (const auto& change : symbols)
			lines << QStringLiteral("  %1  %2").arg(kindLabel(change.kind),
			         describe(change.new_symbol ? change.new_symbol : change.old_symbol));
	}

	if (!colors.empty())
	{
		lines << QString{} << QStringLiteral("Colors:");
		for (const auto& change : colors)
			lines << QStringLiteral("  %1  %2").arg(kindLabel(change.kind),
			         (change.new_color ? change.new_color : change.old_color)->getName());
	}

	return lines.join(QLatin1Char('\n')) + QLatin1Char('\n');
}


QImage MapDiff::render(const Map& old_map, Map& new_map, const QRectF& area, qreal pixels_per_mm) const
{
	auto const size = QSize(qCeil(area.width() * pixels_per_mm), qCeil(area.height() * pixels_per_mm));
	QImage image(size, QImage::Format_ARGB32_Premultiplied);
	image.fill(Qt::white);

	QPainter painter(&image);
	painter.setRenderHint(QPainter::Antialiasing);
	painter.scale(pixels_per_mm, pixels_per_mm);
	painter.translate(-area.topLeft());

	// Context: the new map, faded
	new_map.draw(&painter, { new_map, area, pixels_per_mm, RenderConfig::NoOptions, 1.0 });
	painter.save();
	painter.resetTransform();
	painter.fillRect(image.rect(), QColor(255, 255, 255, 200));
	painter.restore();

	auto const draw_object = [&](const Map& map, const Object* object, const QColor& color) {
		auto const config = RenderConfig { map, area, pixels_per_mm, RenderConfig::HelperSymbols, 1.0 };
		for (int c = map.getNumColors() - 1; c >= 0; --c)
			object->renderables().draw(c, color, &painter, config);
	};

	auto const faded_removed = QColor(removed_color.red(), removed_color.green(), removed_color.blue(), 140);
	for (const auto& change : objects)
	{
		if (change.kind == Removed)
			draw_object(old_map, change.old_object, removed_color);
		else if (change.kind == Modified)
			draw_object(old_map, change.old_object, faded_removed);
	}
	for (const auto& change : objects)
	{
		if (change.kind == Modified)
			draw_object(new_map, change.new_object, modified_color);
		else if (change.kind == Added)
			draw_object(new_map, change.new_object, added_color);
	}

	// Markers, so that small changes are visible at overview resolution
	auto const pixel = 1.0 / pixels_per_mm;
	for (const auto& change : objects)
	{
		const auto* object = change.new_object ? change.new_object : change.old_object;
		auto const extent = object->getExtent();
		auto const radius = std::max(std::hypot(extent.width(), extent.height()) / 2 + 4 * pixel, 10 * pixel);
		auto color = change.kind == Added ? added_color : change.kind == Removed ? removed_color : modified_color;
		painter.setPen(QPen(color, 2 * pixel));
		painter.setBrush(Qt::NoBrush);
		painter.drawEllipse(extent.center(), radius, radius);
	}

	// Legend
	int counts[3] = {};
	for (const auto& change : objects)
		++counts[change.kind];
	painter.resetTransform();
	QFont font = painter.font();
	font.setPixelSize(std::max(12, size.height() / 60));
	painter.setFont(font);
	auto const line_height = painter.fontMetrics().height();
	auto const legend = std::vector<std::pair<QColor, QString>> {
	    { added_color,    QStringLiteral("added: %1").arg(counts[Added]) },
	    { removed_color,  QStringLiteral("removed: %1").arg(counts[Removed]) },
	    { modified_color, QStringLiteral("modified: %1").arg(counts[Modified]) },
	};
	painter.setPen(Qt::NoPen);
	painter.setBrush(QColor(255, 255, 255, 230));
	painter.drawRect(QRectF(0, 0, line_height * 9, line_height * (legend.size() + 0.5)));
	for (std::size_t i = 0; i < legend.size(); ++i)
	{
		auto const y = line_height * (i + 0.25);
		painter.setPen(Qt::NoPen);
		painter.setBrush(legend[i].first);
		painter.drawRect(QRectF(line_height * 0.25, y + line_height * 0.2, line_height * 0.6, line_height * 0.6));
		painter.setPen(Qt::black);
		painter.drawText(QPointF(line_height * 1.1, y + painter.fontMetrics().ascent()), legend[i].second);
	}

	return image;
}


}  // namespace OpenOrienteering
