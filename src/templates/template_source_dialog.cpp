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

#include "template_source_dialog.h"

#include <QCheckBox>
#include <QDialogButtonBox>
#include <QPushButton>
#include <QFontMetrics>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>


namespace OpenOrienteering {

namespace {

QPlainTextEdit* createTextEdit(const QString& text, int lines)
{
	auto* edit = new QPlainTextEdit(text);
	edit->setTabChangesFocus(true);
	edit->setFixedHeight(edit->fontMetrics().lineSpacing() * lines + 12);
	return edit;
}

QString trimmed(const QPlainTextEdit* edit)
{
	return edit->toPlainText().trimmed();
}

QString trimmed(const QLineEdit* edit)
{
	return edit->text().trimmed();
}

}  // namespace



TemplateSourceDialog::TemplateSourceDialog(const QString& template_name, const TemplateSource& source, QWidget* parent, Mode mode)
: QDialog(parent)
{
	setWindowTitle(tr("Template source"));

	auto* layout = new QFormLayout(this);

	auto intro_text = tr("Where does the data of <b>%1</b> come from, and how may it be used? "
	                     "This information is saved in the map file.").arg(template_name.toHtmlEscaped());
	if (mode == Opening && !source.isEmpty())
		intro_text += QLatin1String("<br><br>") + tr("Some fields were filled in from the file. Please check them.");
	auto* intro = new QLabel(intro_text);
	intro->setWordWrap(true);
	layout->addRow(intro);

	provider_edit = new QLineEdit(source.provider);
	provider_edit->setPlaceholderText(tr("e.g. the national mapping agency"));
	layout->addRow(tr("Provider:"), provider_edit);

	title_edit = new QLineEdit(source.title);
	title_edit->setPlaceholderText(tr("Product or dataset"));
	layout->addRow(tr("Title:"), title_edit);

	captured_edit = new QLineEdit(source.captured);
	captured_edit->setPlaceholderText(tr("YYYY-MM-DD"));
	captured_edit->setToolTip(tr("When the data was captured, e.g. the date of the aerial photo or survey."));
	layout->addRow(tr("Captured:"), captured_edit);

	acquired_edit = new QLineEdit(source.acquired);
	acquired_edit->setPlaceholderText(tr("YYYY-MM-DD"));
	acquired_edit->setToolTip(tr("When the data was obtained from the provider."));
	layout->addRow(tr("Acquired:"), acquired_edit);

	license_edit = new QLineEdit(source.license);
	layout->addRow(tr("License:"), license_edit);

	permitted_use_edit = createTextEdit(source.permitted_use, 2);
	permitted_use_edit->setPlaceholderText(tr("e.g. training maps, competition maps, print, web"));
	layout->addRow(tr("Permitted use:"), permitted_use_edit);

	attribution_edit = createTextEdit(source.attribution, 2);
	attribution_edit->setPlaceholderText(tr("Text to be shown on the map"));
	layout->addRow(tr("Attribution:"), attribution_edit);

	reference_edit = new QLineEdit(source.reference);
	reference_edit->setPlaceholderText(tr("Order number, agreement, contact"));
	layout->addRow(tr("Reference:"), reference_edit);

	url_edit = new QLineEdit(source.url);
	layout->addRow(tr("URL:"), url_edit);

	notes_edit = createTextEdit(source.notes, 3);
	layout->addRow(tr("Notes:"), notes_edit);

	auto* button_box = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
	if (mode == Opening)
	{
		button_box->button(QDialogButtonBox::Cancel)->setText(tr("Skip"));
		dont_ask_check = new QCheckBox(tr("Don't ask when opening templates"));
		dont_ask_check->setToolTip(tr("Information found in template files is still recorded. "
		                              "This can be changed in the settings."));
		layout->addRow(dont_ask_check);
	}
	layout->addRow(button_box);
	connect(button_box, &QDialogButtonBox::accepted, this, &QDialog::accept);
	connect(button_box, &QDialogButtonBox::rejected, this, &QDialog::reject);

	setMinimumWidth(fontMetrics().averageCharWidth() * 60);
}


TemplateSourceDialog::~TemplateSourceDialog() = default;


bool TemplateSourceDialog::dontAskAgain() const
{
	return dont_ask_check && dont_ask_check->isChecked();
}


TemplateSource TemplateSourceDialog::source() const
{
	TemplateSource source;
	source.provider = trimmed(provider_edit);
	source.title = trimmed(title_edit);
	source.captured = trimmed(captured_edit);
	source.acquired = trimmed(acquired_edit);
	source.license = trimmed(license_edit);
	source.permitted_use = trimmed(permitted_use_edit);
	source.attribution = trimmed(attribution_edit);
	source.reference = trimmed(reference_edit);
	source.url = trimmed(url_edit);
	source.notes = trimmed(notes_edit);
	return source;
}


}  // namespace OpenOrienteering
