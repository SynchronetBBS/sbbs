#include "logwidget.h"
#include "textutil.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QFont>
#include <QTextCharFormat>
#include <QTextCursor>
#include <QPainter>

static const QStringList LogLevelNames = {
	"Emergency", "Alert", "Critical", "Error",
	"Warning", "Notice", "Normal", "Debug",
};

static const char *LogLevelShort[] = {
	"Emrg", "Alrt", "Crit", "Err", "Warn", "Note", "Norm", "Dbg",
};

class LogBlockData : public QTextBlockUserData
{
public:
	LogBlockData(int level, int tsLen = 0) : m_level(level), m_tsLen(tsLen) {}
	int level() const { return m_level; }
	int tsLen() const { return m_tsLen; }
private:
	int m_level;
	int m_tsLen;
};

LogWidget::LogWidget(const QString &title, const QString &serverId,
                     const QString &controlLabel, bool dark, QWidget *parent)
	: QWidget(parent), m_dark(dark)
{
	auto *layout = new QVBoxLayout(this);
	layout->setContentsMargins(2, 2, 2, 2);

	auto *toolbar = new QHBoxLayout;

	if (!serverId.isEmpty()) {
		auto *serverBtn = new QToolButton;
		serverBtn->setText(controlLabel);
		serverBtn->setPopupMode(QToolButton::InstantPopup);
		auto *serverMenu = new QMenu(this);
		connect(serverMenu->addAction("&Recycle"), &QAction::triggered, this, [this, serverId] { emit recycleServer(serverId); });
		connect(serverMenu->addAction("&Pause"), &QAction::triggered, this, [this, serverId] { emit pauseServer(serverId); });
		connect(serverMenu->addAction("Resu&me"), &QAction::triggered, this, [this, serverId] { emit resumeServer(serverId); });
		serverBtn->setMenu(serverMenu);
		toolbar->addWidget(serverBtn);
	}

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
	m_text = new QPlainTextEdit;
	connect(clearBtn, &QPushButton::clicked, m_text, &QPlainTextEdit::clear);
	toolbar->addWidget(clearBtn);
	toolbar->addStretch();

	m_filterEdit = new QLineEdit;
	m_filterEdit->setPlaceholderText("Filter");
	m_filterEdit->setClearButtonEnabled(true);
	m_filterEdit->setMaximumWidth(200);
	m_filterDebounce = new QTimer(this);
	m_filterDebounce->setSingleShot(true);
	m_filterDebounce->setInterval(150);
	connect(m_filterEdit, &QLineEdit::textChanged, this, [this](const QString &text) {
		m_filterText = text;
		m_filterGeneration++;
		m_filterDebounce->start();
	});
	connect(m_filterDebounce, &QTimer::timeout, this, &LogWidget::startFilterApply);
	toolbar->addWidget(m_filterEdit);

	m_showBtn = new QToolButton;
	m_showBtn->setText("Show");
	m_showBtn->setPopupMode(QToolButton::InstantPopup);
	auto *showMenu = new QMenu(this);
	for (int i = 0; i < 8; ++i) {
		m_filterActions[i] = showMenu->addAction(LogLevelNames[i]);
		m_filterActions[i]->setCheckable(true);
		m_filterActions[i]->setChecked(true);
		connect(m_filterActions[i], &QAction::toggled, this, [this, i](bool on) {
			m_levelVisible[i] = on;
			startFilterApply();
		});
	}
	m_showBtn->setMenu(showMenu);
	toolbar->addWidget(m_showBtn);

	layout->addLayout(toolbar);

	m_text->setReadOnly(true);
	m_text->setMaximumBlockCount(DefaultMaxLogLines);
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

static void insertWithControls(QTextCursor &cursor, const QString &text, const QTextCharFormat &fmt)
{
	QString run;
	for (QChar ch : text) {
		ushort c = ch.unicode();
		if (c <= 0x1F || c == 0x7F) {
			if (!run.isEmpty()) {
				cursor.insertText(run, fmt);
				run.clear();
			}
			QTextCharFormat ctrlFmt(fmt);
			QChar display = (c == 0x7F) ? QChar(0x2421) : QChar(0x2400 + c);
			QChar caret = (c == 0x7F) ? '?' : QChar('@' + c);
			const char *name = (c == 0x7F) ? "DEL" : CtrlNames[c];
			const char *esc = ctrlCEscape(c);
			QString tip;
			if (esc)
				tip = QString("%1, ^%2 (0x%3 \u2014 %4)")
					.arg(esc).arg(caret).arg(c, 2, 16, QChar('0')).arg(name);
			else
				tip = QString("^%1 (0x%2 \u2014 %3)")
					.arg(caret).arg(c, 2, 16, QChar('0')).arg(name);
			ctrlFmt.setToolTip(tip);
			ctrlFmt.setFontWeight(QFont::Bold);
			cursor.insertText(QString(display), ctrlFmt);
		} else {
			run += ch;
		}
	}
	if (!run.isEmpty())
		cursor.insertText(run, fmt);
}

void LogWidget::appendLine(int level, const QString &timestamp, const QString &text)
{
	QTextCharFormat fmt;
	fmt.setForeground(colorForLevel(level));
	if (level <= 3)
		fmt.setFontWeight(QFont::Bold);

	QTextCursor cursor = m_text->textCursor();
	cursor.movePosition(QTextCursor::End);
	int tsLen = 0;
	if (!timestamp.isEmpty()) {
		QTextCharFormat tsFmt;
		tsFmt.setForeground(m_dark ? QColor("#888888") : QColor("gray"));
		tsLen = timestamp.size() + 1;
		cursor.setCharFormat(tsFmt);
		cursor.insertText(timestamp + " ");
	}
	if (hasControlChars(text))
		insertWithControls(cursor, text, fmt);
	else
		cursor.insertText(text, fmt);
	cursor.insertText("\n", fmt);

	QTextBlock block = cursor.block().previous();
	if (block.isValid()) {
		block.setUserData(new LogBlockData(level, tsLen));
		block.setVisible(blockMatchesFilter(block));
	}

	m_text->setTextCursor(cursor);
	m_text->ensureCursorVisible();
}

bool LogWidget::blockMatchesFilter(const QTextBlock &block) const
{
	auto *data = static_cast<LogBlockData *>(block.userData());
	if (!data)
		return true;
	if (!m_levelVisible[qBound(0, data->level(), 7)])
		return false;
	if (!m_filterText.isEmpty() && !block.text().contains(m_filterText, Qt::CaseInsensitive))
		return false;
	return true;
}

void LogWidget::startFilterApply()
{
	m_activeGeneration = m_filterGeneration;
	m_filterBlock = m_text->document()->end().previous();
	applyFilterChunk();
}

void LogWidget::applyFilterChunk()
{
	if (m_activeGeneration != m_filterGeneration)
		return;

	static const int ChunkSize = 50000;
	int count = 0;
	while (m_filterBlock.isValid() && count < ChunkSize) {
		m_filterBlock.setVisible(blockMatchesFilter(m_filterBlock));
		m_filterBlock = m_filterBlock.previous();
		++count;
	}

	if (m_filterBlock.isValid() && m_activeGeneration == m_filterGeneration) {
		QTextDocument *doc = m_text->document();
		doc->markContentsDirty(0, doc->characterCount());
		m_text->viewport()->update();
		QTimer::singleShot(0, this, &LogWidget::applyFilterChunk);
	} else if (m_activeGeneration == m_filterGeneration) {
		QTextDocument *doc = m_text->document();
		doc->markContentsDirty(0, doc->characterCount());
		m_text->viewport()->update();
	}
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

void LogWidget::updateFilterIcons()
{
	for (int i = 0; i < 8; ++i) {
		QPixmap pm(12, 12);
		pm.fill(Qt::transparent);
		QPainter p(&pm);
		p.setRenderHint(QPainter::Antialiasing);
		p.setBrush(colorForLevel(i));
		p.setPen(Qt::NoPen);
		p.drawEllipse(1, 1, 10, 10);
		p.end();
		m_filterActions[i]->setIcon(QIcon(pm));
	}
}

void LogWidget::recolorBlocks()
{
	QColor tsColor = m_dark ? QColor("#888888") : QColor("gray");
	QTextCursor cursor(m_text->document());
	cursor.beginEditBlock();
	for (QTextBlock block = m_text->document()->begin(); block.isValid(); block = block.next()) {
		auto *data = static_cast<LogBlockData *>(block.userData());
		if (!data)
			continue;
		QColor color = colorForLevel(data->level());
		QTextCharFormat fmt;
		fmt.setForeground(color);
		if (data->level() <= 3)
			fmt.setFontWeight(QFont::Bold);

		int tsLen = data->tsLen();
		cursor.setPosition(block.position());
		if (tsLen > 0) {
			cursor.setPosition(block.position() + tsLen, QTextCursor::KeepAnchor);
			QTextCharFormat tsFmt;
			tsFmt.setForeground(tsColor);
			cursor.setCharFormat(tsFmt);
			cursor.setPosition(block.position() + tsLen);
		}
		cursor.movePosition(QTextCursor::EndOfBlock, QTextCursor::KeepAnchor);
		cursor.setCharFormat(fmt);
	}
	cursor.endEditBlock();
}

void LogWidget::setDark(bool dark)
{
	m_dark = dark;
	m_text->setStyleSheet(dark
		? "QPlainTextEdit { background-color: #1e1e1e; color: #cccccc; }"
		: QString());
	recolorBlocks();
	updateFilterIcons();
}

void LogWidget::setMaxLines(int max)
{
	m_text->setMaximumBlockCount(max);
}
