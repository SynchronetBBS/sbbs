#include "logwidget.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QFont>
#include <QTextCharFormat>
#include <QTextCursor>

static const QStringList LogLevelNames = {
	"Emergency", "Alert", "Critical", "Error",
	"Warning", "Notice", "Normal", "Debug",
};

LogWidget::LogWidget(const QString &title, bool dark, QWidget *parent)
	: QWidget(parent), m_dark(dark)
{
	auto *layout = new QVBoxLayout(this);
	layout->setContentsMargins(2, 2, 2, 2);

	auto *toolbar = new QHBoxLayout;
	toolbar->addWidget(new QLabel("Level:"));
	m_levelCombo = new QComboBox;
	m_levelCombo->addItems(LogLevelNames);
	m_levelCombo->setCurrentIndex(m_minLevel);
	connect(m_levelCombo, &QComboBox::currentIndexChanged, this, [this](int i) { m_minLevel = i; });
	toolbar->addWidget(m_levelCombo);

	m_pauseBtn = new QPushButton("Pause");
	m_pauseBtn->setCheckable(true);
	connect(m_pauseBtn, &QPushButton::toggled, this, [this](bool paused) {
		m_paused = paused;
		m_pauseBtn->setText(paused ? "Resume" : "Pause");
		if (!paused) {
			for (auto &[level, ts, txt] : m_buffer)
				appendLine(level, ts, txt);
			m_buffer.clear();
		}
	});
	toolbar->addWidget(m_pauseBtn);

	auto *clearBtn = new QPushButton("Clear");
	connect(clearBtn, &QPushButton::clicked, m_text = new QPlainTextEdit, &QPlainTextEdit::clear);
	toolbar->addWidget(clearBtn);
	toolbar->addStretch();
	layout->addLayout(toolbar);

	m_text->setReadOnly(true);
	m_text->setMaximumBlockCount(5000);
	QFont font("Monospace", 9);
	font.setStyleHint(QFont::Monospace);
	m_text->setFont(font);
	setDark(m_dark);
	layout->addWidget(m_text);
}

void LogWidget::appendLog(int level, const QString &timestamp, const QString &text)
{
	if (level > m_minLevel) return;
	if (m_paused) {
		m_buffer.append({level, timestamp, text});
		return;
	}
	appendLine(level, timestamp, text);
}

void LogWidget::appendLine(int level, const QString &timestamp, const QString &text)
{
	QTextCharFormat fmt;
	fmt.setForeground(colorForLevel(level));
	if (level <= 3)
		fmt.setFontWeight(QFont::Bold);

	QTextCursor cursor = m_text->textCursor();
	cursor.movePosition(QTextCursor::End);
	if (!timestamp.isEmpty()) {
		QTextCharFormat tsFmt;
		tsFmt.setForeground(m_dark ? QColor("#888888") : QColor("gray"));
		cursor.insertText(timestamp + " ", tsFmt);
	}
	cursor.insertText(text + "\n", fmt);
	m_text->setTextCursor(cursor);
	m_text->ensureCursorVisible();
}

QColor LogWidget::colorForLevel(int level) const
{
	if (m_dark) {
		switch (level) {
		case 0: case 1: case 2: return QColor("#FF0000");
		case 3: return QColor("#FF4444");
		case 4: return QColor("#FF00FF");
		case 5: return QColor("#55AAFF");
		case 6: return QColor("#CCCCCC");
		case 7: return QColor("#44FF44");
		default: return QColor("#CCCCCC");
		}
	} else {
		switch (level) {
		case 0: case 1: case 2: return QColor("red");
		case 3: return QColor("darkRed");
		case 4: return QColor("magenta");
		case 5: return QColor("blue");
		case 6: return QColor("black");
		case 7: return QColor("darkGreen");
		default: return QColor("black");
		}
	}
}

void LogWidget::setDark(bool dark)
{
	m_dark = dark;
	m_text->setStyleSheet(dark
		? "QPlainTextEdit { background-color: #1e1e1e; color: #cccccc; }"
		: QString());
}
