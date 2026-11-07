---
title: Templates Menu
authors:
  - Peter Hoban
  - Thomas Schoeps
  - Kai Pastor
keywords: Menus
parent: Menus
grand_parent: Reference
nav_order: 0.7
last_modified_date: 20 January 2018
---

#### ![ ](../mapper-images/templates.png) Template setup window
**Ctrl+Shift+9**

Switches the display of the [template setup window](templates.md#setup) on the map screen. This provides access to the functions required to open, scale, and position a template, and to close it.


---

#### Open template...

Opens a template file directly without going through the setup window.


#### Reopen template...

Shows the template reopen dialog where templates which have been opened and then closed before can be restored.




---

#### Collect templates into map folder...

Copies the files of all templates into the folder `templates` next to the map file, and changes the templates to use the copies. Companion files such as world files (e.g. `.pgw`), projection files (`.prj`), GDAL auxiliary files (`.aux.xml`) and the parts of shapefiles are copied, too. Afterwards, the map folder contains everything needed to open the map with its templates, e.g. to share it with others or to keep it in a version control system.

The map must have been saved before. A list of the files to be copied is shown for confirmation. Templates which are already in the map's folder are not changed. Files which are not found, and files which refer to other files (such as GDAL virtual rasters, `.vrt`), are not collected. Save the map afterwards to keep the new template locations.
