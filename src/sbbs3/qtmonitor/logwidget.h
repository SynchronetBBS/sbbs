#ifndef LOGWIDGET_H
#define LOGWIDGET_H

#include <QWidget>
#include <QPlainTextEdit>
#include <QComboBox>
#include <QPushButton>
#include <QToolButton>
#include <QMenu>
#include <QTextBlockUserData>

class LogWidget : public QWidget
{
	Q_OBJECT

public:
	explicit LogWidget(const QString &title = "Log", bool dark = true, QWidget *parent = nullptr);
	void appendLog(int level, const QString &timestamp, const QString &text);
	void setDark(bool dark);
	void setMaxLines(int max);

private:
	void appendLine(int level, const QString &timestamp, const QString &text);
	void applyFilters();
	void updateFilterIcons();
	QColor colorForLevel(int level) const;

	QPlainTextEdit *m_text;
	QComboBox *m_levelCombo;
	QPushButton *m_pauseBtn;
	QToolButton *m_showBtn;
	QAction *m_filterActions[8];
	int m_minLevel = 7;
	bool m_paused = false;
	bool m_dark;
	bool m_levelVisible[8] = {true, true, true, true, true, true, true, true};
	QVector<std::tuple<int, QString, QString>> m_buffer;
};

#endif
