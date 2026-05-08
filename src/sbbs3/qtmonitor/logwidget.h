#ifndef LOGWIDGET_H
#define LOGWIDGET_H

#include <QWidget>
#include <QPlainTextEdit>
#include <QComboBox>
#include <QLineEdit>
#include <QPushButton>
#include <QToolButton>
#include <QMenu>
#include <QTimer>
#include <QTextBlockUserData>

static constexpr int DefaultMaxLogLines = sizeof(void *) >= 8 ? 2000000 : 1000000;

class LogWidget : public QWidget
{
	Q_OBJECT

public:
	explicit LogWidget(const QString &title = "Log", const QString &serverId = {},
	                   const QString &controlLabel = "Server",
	                   bool dark = true, QWidget *parent = nullptr);
	void appendLog(int level, const QString &timestamp, const QString &text);
	void setDark(bool dark);
	void setMaxLines(int max);

signals:
	void recycleServer(const QString &server);
	void pauseServer(const QString &server);
	void resumeServer(const QString &server);

private:
	void appendLine(int level, const QString &timestamp, const QString &text);
	void startFilterApply();
	void applyFilterChunk();
	void updateFilterIcons();
	QColor colorForLevel(int level) const;
	bool blockMatchesFilter(const QTextBlock &block) const;

	QPlainTextEdit *m_text;
	QComboBox *m_levelCombo;
	QLineEdit *m_filterEdit;
	QPushButton *m_pauseBtn;
	QToolButton *m_showBtn;
	QAction *m_filterActions[8];
	QTimer *m_filterDebounce;
	QString m_filterText;
	QTextBlock m_filterBlock;
	int m_filterGeneration = 0;
	int m_activeGeneration = 0;
	int m_minLevel = 7;
	bool m_paused = false;
	bool m_dark;
	bool m_levelVisible[8] = {true, true, true, true, true, true, true, true};
	QVector<std::tuple<int, QString, QString>> m_buffer;
};

#endif
