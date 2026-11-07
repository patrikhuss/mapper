---
title: GPS Tracks from Watches and Phones
keywords: Templates, GPS, GNSS, GPX, Tracks
parent: Templates and Data
nav_order: 0.7
last_modified_date: 7 November 2026
---

 - [Introduction](#introduction)
 - [Getting a GPX file](#getting-a-gpx-file)
 - [Opening a track in Mapper](#opening-a-track)
 - [Combining many tracks](#combining-tracks)
 - [Drawing paths from tracks](#drawing-paths)
 - [Privacy](#privacy)


### Introduction  {#introduction}

Tracks recorded with a sports watch, a handheld GPS receiver or a phone app
show where you actually went. They are useful for finding and
drawing paths and trails, for checking the position of features in the
terrain, and for planning fieldwork.

Mapper can display such tracks as [templates](templates.md#track-templates).
The common exchange format for tracks is GPX. This page explains how to get
GPX files from typical devices and services, and how to make good use of them.


### Getting a GPX file  {#getting-a-gpx-file}

#### Garmin Connect

 1. Open the activity on the Garmin Connect website.
 2. Click the gear icon in the upper right corner of the activity page.
 3. Choose "Export to GPX".

Do not choose "Export Original": it gives a FIT file (see below).

#### Strava and other training services

Most training services can export a single activity as GPX. In Strava, open
the activity, click the "..." button and choose "Export GPX". Other services
offer similar functions, sometimes only in their web version.

#### Directly from the device

Many Garmin watches appear as a USB drive when connected to a computer. The
activities are stored in the folder `GARMIN/Activity` as FIT files. Mapper
cannot read FIT files directly, but they can be converted to GPX with the free
program [GPSBabel](https://www.gpsbabel.org/):

    gpsbabel -i garmin_fit -f activity.fit -o gpx -F activity.gpx

GPSBabel can also read the formats of many other devices.

#### Phone apps

Many outdoor and orienteering apps can record tracks and share them as GPX.
Mapper itself can record tracks on Android, see
[Touch mode](touch-mode.md).


### Opening a track in Mapper  {#opening-a-track}

Use Templates -&gt; Open template... and select the GPX file.

Tracks are recorded in geographic coordinates (latitude and longitude). To
show them at the right place, the map must be
[georeferenced](georeferencing.md). If the map is not georeferenced yet,
Mapper will ask for the georeferencing when the track is opened. When you
start a new map from tracks only, choosing UTM is fine.

The track is shown on top of the map. Like other templates, it can be hidden,
made transparent and moved in the drawing order in the
[template setup window](templates.md#setup).

Via Edit -&gt; Import and remove in the template setup window, the track can be
converted to objects of the map. This is useful when you want to edit the
track lines directly, but usually it is better to keep the track as a template
and draw new objects on top of it.


### Combining many tracks  {#combining-tracks}

A single track is often inaccurate. When many runs along the same paths are
shown together, the paths stand out clearly where the tracks overlap, and
single outliers become obvious.

You can open several GPX files as separate templates. For many files, it is
easier to combine them into one file. This can be done with the `ogr2ogr`
program which comes with [GDAL](https://gdal.org/):

    ogr2ogr -f GPKG runs.gpkg run1.gpx tracks
    ogr2ogr -f GPKG -update -append runs.gpkg run2.gpx tracks -nln tracks

Repeat the second command for each further file. The resulting GeoPackage
file `runs.gpkg` can be opened as a template like other
[geospatial data](gdal.md).


### Drawing paths from tracks  {#drawing-paths}

Consumer devices are typically accurate to about 3 to 10 meters in open
terrain. In dense forest, close to cliffs and between buildings, errors can
be considerably larger. Some devices also smooth or cut corners.

So tracks tell where a path goes and how it connects to other paths, but not
its exact position. Good practice:

 - Prefer the middle of several overlapping tracks to a single track.
 - Where available, check the position against orthophotos or
   laser scanning data, see [Geospatial data](gdal.md).
 - Do not draw features from tracks alone where accuracy matters, e.g.
   close to controls.
 - Record the source of the track data, e.g. in the template name, so that
   others know where the drawn objects come from.


### Privacy  {#privacy}

Tracks show where and when someone was. Recordings starting and ending at
home reveal the address of the runner. Before sharing tracks with others, or
storing them together with a map which is shared, consider:

 - asking the people who recorded the tracks for permission,
 - removing the start and the end of each track, or using tracks which
   start away from home,
 - removing time stamps and other personal data. The `ogr2ogr` commands in
   [Combining many tracks](#combining-tracks) copy only the track lines, not
   the time stamps of the single track points.
