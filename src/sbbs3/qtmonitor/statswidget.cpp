#include "statswidget.h"
#include <QVBoxLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QFont>

StatsWidget::StatsWidget(bool dark, QWidget *parent)
	: QWidget(parent), m_dark(dark)
{
	auto *layout = new QVBoxLayout(this);
	layout->setContentsMargins(4, 4, 4, 4);

	QFont valueFont("Monospace", 10);
	valueFont.setStyleHint(QFont::Monospace);

	struct Group { const char *name; QVector<QPair<QString, QString>> fields; };
	QVector<Group> groups = {
		{"Today", {{"logons","Logons"}, {"timeon","Time On"}, {"email","Email"},
		           {"feedback","Feedback"}, {"newusers","New Users"}, {"posts","Posts"}}},
		{"Total", {{"tlogons","Logons"}, {"ttimeon","Time On"}, {"temail","Email"},
		           {"tfeedback","Feedback"}, {"tusers","Users"}}},
		{"Uploads Today", {{"ulfiles","Files"}, {"ulbytes","Bytes"}}},
		{"Downloads Today", {{"dlfiles","Files"}, {"dlbytes","Bytes"}}},
	};

	for (auto &g : groups) {
		auto *group = new QGroupBox(g.name);
		auto *grid = new QGridLayout(group);
		for (int row = 0; row < g.fields.size(); ++row) {
			grid->addWidget(new QLabel(g.fields[row].second + ":"), row, 0);
			auto *val = new QLabel("0");
			val->setFont(valueFont);
			val->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
			val->setMinimumWidth(80);
			grid->addWidget(val, row, 1);
			m_labels[g.fields[row].first] = val;
		}
		layout->addWidget(group);
	}
	layout->addStretch();
	setDark(m_dark);
}

void StatsWidget::setStat(const QString &key, const QString &value)
{
	auto it = m_labels.find(key);
	if (it != m_labels.end())
		(*it)->setText(value);
}

void StatsWidget::setDark(bool dark)
{
	m_dark = dark;
	setStyleSheet(dark
		? "QGroupBox { color: #aaaaaa; border: 1px solid #555; border-radius: 4px; "
		  "margin-top: 8px; padding-top: 12px; } "
		  "QGroupBox::title { subcontrol-origin: margin; left: 8px; } "
		  "QLabel { color: #cccccc; }"
		: QString());
}
