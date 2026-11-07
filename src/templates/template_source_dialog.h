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

#ifndef OPENORIENTEERING_TEMPLATE_SOURCE_DIALOG_H
#define OPENORIENTEERING_TEMPLATE_SOURCE_DIALOG_H

#include <QDialog>
#include <QObject>
#include <QString>

#include "templates/template_source.h"

class QCheckBox;
class QLineEdit;
class QPlainTextEdit;
class QWidget;

namespace OpenOrienteering {


/**
 * A dialog for editing the source and terms of use of a template.
 */
class TemplateSourceDialog : public QDialog
{
Q_OBJECT
public:
	/// The situation in which the dialog is shown.
	enum Mode
	{
		Edit,     ///< Editing the source of an existing template
		Opening,  ///< Right after opening a template: may be skipped, or disabled
	};

	TemplateSourceDialog(const QString& template_name, const TemplateSource& source, QWidget* parent = nullptr, Mode mode = Edit);
	~TemplateSourceDialog() override;

	TemplateSource source() const;

	/// In Opening mode, returns true if the user doesn't want to be asked again.
	bool dontAskAgain() const;

private:
	QCheckBox* dont_ask_check = nullptr;
	QLineEdit* provider_edit;
	QLineEdit* title_edit;
	QLineEdit* captured_edit;
	QLineEdit* acquired_edit;
	QLineEdit* license_edit;
	QLineEdit* reference_edit;
	QLineEdit* url_edit;
	QPlainTextEdit* attribution_edit;
	QPlainTextEdit* permitted_use_edit;
	QPlainTextEdit* notes_edit;
};


}  // namespace OpenOrienteering

#endif // OPENORIENTEERING_TEMPLATE_SOURCE_DIALOG_H
