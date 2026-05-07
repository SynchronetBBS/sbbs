#ifndef LOGWIDGET_H
#define LOGWIDGET_H

#include <QWidget>
#include <QPlainTextEdit>
#include <QComboBox>
#include <QPushButton>

class LogWidget : public QWidget
{
	Q_OBJECT

public:
	explicit LogWidget(const QString &title = "Log", bool dark = true, QWidget *parent = nullptr);
	void appendLog(int level, const QString &timestamp, const QString &text);
	void setDark(bool dark);

private:
	void appendLine(int level, const QString &timestamp, const QString &text);
	QColor colorForLevel(int level) const;

	QPlainTextEdit *m_text;
	QComboBox *m_levelCombo;
	QPushButton *m_pauseBtn;
	int m_minLevel = 7;
	bool m_paused = false;
	bool m_dark;
	QVector<std::tuple<int, QString, QString>> m_buffer;
};

#endif
