---
title: Official Geodata as Templates
keywords: Templates, GDAL, Orthophoto, Elevation model, Laser scanning
parent: Templates and Data
nav_order: 0.6
last_modified_date: 6 November 2026
---

 - [Introduction](#introduction)
 - [Before downloading](#before-downloading)
 - [Orthophotos](#orthophotos)
 - [Elevation models](#elevation-models)
 - [Laser scanning point clouds](#point-clouds)
 - [Keeping the data with the map](#keeping-data)
 - [Checking for newer data](#newer-data)
 - [Example: Sweden (Lantmäteriet)](#sweden)


### Introduction {#introduction}

Many national mapping agencies publish aerial photographs (orthophotos),
elevation models and laser scanning data, often free of charge.
These data make excellent templates: buildings, roads, paths, clearings
and vegetation boundaries can be traced from an orthophoto,
and contours, cliffs and knolls can be checked against an elevation model.

This page describes how to prepare such data for use in Mapper.
The details of ordering differ from country to country;
an example for Sweden is given [at the end](#sweden).


### Before downloading {#before-downloading}

**Terms of use.**
Read the terms which come with the data.
They may restrict how the data may be used
(e.g. only for internal work, not for printed or published maps),
require an attribution text on the map,
or require you to accept special terms.
Laser scanning data can be classified as personal data in some countries.
Keep a note of the terms together with the data (see [below](#keeping-data)),
so that everyone who works on the map later knows them, too.

**Coordinate reference system.**
The map should be [georeferenced](georeferencing.md) in the
coordinate reference system used by the data,
or in one which can be converted to it.
Then the templates are positioned automatically.

**Area and date.**
Data is usually delivered in tiles.
Determine the map area in the map's coordinate reference system
(e.g. from the coordinates shown in the status bar),
and download all tiles which cover it.
Note the date when the data was captured:
data older than the map's last field check is of limited use for an update.


### Orthophotos {#orthophotos}

Orthophotos are usually delivered as GeoTIFF or Cloud Optimized GeoTIFF (COG) files,
or as JPEG2000.
They can be opened directly via Templates -&gt; Open template...;
see [Geospatial data](gdal.md).
When the map covers several tiles, open each tile as a separate template,
or merge them into a single file first, e.g. with GDAL:

    gdalbuildvrt orthophoto.vrt tile1.tif tile2.tif tile3.tif

Comparing orthophotos from different years shows where the terrain has changed,
for example by forestry or construction.


### Elevation models {#elevation-models}

Mapper displays raster data as images; it does not interpret altitude values.
An elevation model (DEM/DTM) therefore needs to be turned into
something visible before it is used as a template.
With the GDAL command line tools:

    # A hillshade image, raster template
    gdaldem hillshade -z 2 dtm.tif hillshade.tif

    # Contour lines with 2.5 m interval, vector template
    gdal_contour -i 2.5 -a elevation dtm.tif contours.gpkg

Generated contours are a basis for drawing, not a replacement for field work.


### Laser scanning point clouds {#point-clouds}

Point clouds (LAS/LAZ, COPC) cannot be opened in Mapper.
They are processed in other software which generates
raster or vector data from them, such as vegetation density images,
slope images, or preliminary contours.
These results can then be used as templates.


### Keeping the data with the map {#keeping-data}

Mapper stores the location of each template relative to the map file.
Keeping the templates in a folder next to the map lets you move or share
the whole folder without breaking the template links:

    my-map/
        my-map.xmap
        templates/
            orthophoto-2025/
                tile1.tif
                tile2.tif
                SOURCE.txt
            elevation-2021/
                hillshade.tif
                SOURCE.txt

In `SOURCE.txt` (or a similar file), write down:

 - the provider and product name,
 - the date the data was captured, and the date it was downloaded,
 - the terms of use, the attribution text, and what the data may be used for,
 - the order or license reference, and who ordered it.

For sharing a map and its templates within a club,
the folder can be kept in a version control system such as git.
Save the map in the `.xmap` format,
which is designed to work with version control.
Large templates such as orthophotos are better stored with an extension
for large files, such as Git LFS.


### Checking for newer data {#newer-data}

Mapping agencies update their data regularly; orthophotos are often renewed every few years.
Many agencies publish a catalogue of their data,
some of them as a [STAC](https://stacspec.org) catalogue which can be searched by area.
Comparing the dates in the catalogue with the dates noted in `SOURCE.txt`
shows when newer data covering the map is available.


### Example: Sweden (Lantmäteriet) {#sweden}

The following information describes the situation at the time of writing.
Check Lantmäteriet's pages for the current products and terms.

Lantmäteriet's data is ordered in the [Geotorget](https://www.lantmateriet.se/sv/geodata/geotorget/) portal.
Organisations can register an account, so that ordered data can be used within the organisation.
Orders give access to a product's download service, not to individual tiles.

Products useful for orienteering maps:

 - **Ortofoto Nedladdning** - orthophotos as Cloud Optimized GeoTIFF,
   renewed every two years in most areas.
   Free of charge, but each order is subject to a legal review.
 - **Markhöjdmodell Nedladdning** - elevation model, for hillshades and contours.
 - **Laserdata Nedladdning, skog** - laser point clouds.
   Classified as personal data; special terms must be accepted.

The data is in SWEREF 99 TM (EPSG:3006), and the map should be georeferenced in the same system.

The download services are STAC catalogues.
They can be browsed by area without logging in,
which shows which tiles and years cover a map before ordering:

 - Orthophotos: `https://api.lantmateriet.se/stac-bild/v1`
 - Elevation and laser data: `https://api.lantmateriet.se/stac-hojd/v1`

Lantmäteriet's guide
[Använda våra data i STAC-browser](https://www.lantmateriet.se/sv/geotorget-produktstod/guider/anvanda-vara-data-i-stac-browser/)
describes how to find and download tiles.
Each tile's catalogue entry has a capture date which can be noted in `SOURCE.txt`.
