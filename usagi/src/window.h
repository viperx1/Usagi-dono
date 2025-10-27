#ifndef WINDOW_H
#define WINDOW_H
#include <QtGui>
#include <QtAlgorithms>
#include <QRegularExpression>
#include <QProcess>
#include <QDateTime>
#include <QDir>
#include <QtNetwork/QNetworkAccessManager>
#include <QtNetwork/QNetworkRequest>
#include <QtNetwork/QNetworkReply>
#include <QtWidgets/QTableWidget>
#include <QtWidgets/QBoxLayout>
#include <QtWidgets/QTextEdit>
#include <QtWidgets/QProgressBar>
#include <QtWidgets/QCheckBox>
#include <QtWidgets/QComboBox>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QLabel>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QHeaderView>
#include <QtWidgets/QFileDialog>
#include <QtWidgets/QListView>
#include <QtWidgets/QTreeWidget>
#include <QXmlStreamReader>
#include "hash/ed2k.h"
#include "anidbapi.h"
#include "epno.h"
#include "aired.h"
//#include "hasherthread.h"

class hashes_ : public QTableWidget
{
public:
	bool event(QEvent *e);
};

// Custom tree widget item that can sort episodes using epno type
class EpisodeTreeWidgetItem : public QTreeWidgetItem
{
public:
    EpisodeTreeWidgetItem(QTreeWidgetItem *parent) : QTreeWidgetItem(parent) {}
    
    void setEpno(const epno& ep) { m_epno = ep; }
    epno getEpno() const { return m_epno; }
    
    bool operator<(const QTreeWidgetItem &other) const override
    {
        int column = treeWidget()->sortColumn();
        
        // If sorting by episode number column (column 1) and both items have epno data
        if(column == 1)
        {
            const EpisodeTreeWidgetItem *otherEpisode = dynamic_cast<const EpisodeTreeWidgetItem*>(&other);
            if(otherEpisode && m_epno.isValid() && otherEpisode->m_epno.isValid())
            {
                return m_epno < otherEpisode->m_epno;
            }
        }
        
        // Default comparison for other columns
        return QTreeWidgetItem::operator<(other);
    }
    
private:
    epno m_epno;
};

class Window : public QWidget
{
    Q_OBJECT
private:
	bool eventFilter(QObject *, QEvent *);
	void closeEvent(QCloseEvent *);
    QTimer *safeclose;
    QTimer *startupTimer;
    QElapsedTimer waitforlogout;
	// main layout
    QBoxLayout *layout;
    QTabWidget *tabwidget;
    QPushButton *loginbutton;

    // pages
    QBoxLayout *pageHasher;
    QWidget *pageHasherParent;
    QBoxLayout *pageMylist;
    QWidget *pageMylistParent;
    QBoxLayout *pageNotify;
    QWidget *pageNotifyParent;
    QGridLayout *pageSettings;
    QWidget *pageSettingsParent;
	QBoxLayout *pageLog;
	QWidget *pageLogParent;
	QBoxLayout *pageApiTester;
	QWidget *pageApiTesterParent;

	// page hasher
    QBoxLayout *pageHasherAddButtons;
    QGridLayout *pageHasherSettings;
    QWidget *pageHasherAddButtonsParent;
    QWidget *pageHasherSettingsParent;
    QPushButton *button1;
    QPushButton *button2;
	QPushButton *button3;
    QTextEdit *hasherOutput;
    QPushButton *buttonstart;
    QPushButton *buttonstop;
	QPushButton *buttonclear;
	QProgressBar *progressFile;
	QProgressBar *progressTotal;
	QCheckBox *addtomylist;
	QCheckBox *markwatched;
	QLineEdit *storage;
	QComboBox *hasherFileState;
	QCheckBox *moveto;
	QCheckBox *renameto;
	QLineEdit *movetodir;
	QLineEdit *renametopattern;


	QString lastDir;

    // page mylist
    QTreeWidget *mylistTreeWidget;
    QLabel *mylistStatusLabel;
	QSet<int> episodesNeedingData;  // Track EIDs that need EPISODE API call
	// page settings

    QLabel *labelLogin;
    QLineEdit *editLogin;
    QLabel *labelPassword;
    QLineEdit *editPassword;
    QPushButton *buttonSaveSettings;
    QPushButton *buttonRequestMylistExport;

	// page notify
	QTextEdit *notifyOutput;

	// Notification tracking for mylist export
	int expectedNotificationsToCheck;
	int notificationsCheckedWithoutExport;
	bool isCheckingNotifications;

	// page log
	QTextEdit *logOutput;

	// page apitester
	QLineEdit *apitesterInput;
	QTextEdit *apitesterOutput;


public slots:
    void Button1Click();
    void Button2Click();
	void Button3Click();
    void ButtonHasherStartClick();
    void ButtonHasherStopClick();
    void ButtonHasherClearClick();
    void getNotifyPartsDone(int, int);
    void getNotifyFileHashed(ed2k::ed2kfilestruct);
    void getNotifyLogAppend(QString);
    void getNotifyLoginChagned(QString);
    void getNotifyPasswordChagned(QString);
    void hasherFinished();
    void shot();
    void saveSettings();
	void apitesterProcess();
	void markwatchedStateChanged(int);

    void ButtonLoginClick();
	void getNotifyMylistAdd(QString, int);
    void getNotifyLoggedIn(QString, int);
    void getNotifyLoggedOut(QString, int);
	void getNotifyMessageReceived(int nid, QString message);
	void getNotifyCheckStarting(int count);
	void getNotifyExportQueued(QString tag);
	void getNotifyExportAlreadyInQueue(QString tag);
	void getNotifyExportNoSuchTemplate(QString tag);
	void getNotifyEpisodeUpdated(int eid, int aid);
	void onMylistItemExpanded(QTreeWidgetItem *item);
    void safeClose();
    void loadMylistFromDatabase();
    void updateEpisodeInTree(int eid, int aid);
    void startupInitialization();
    bool isMylistFirstRunComplete();
    void setMylistFirstRunComplete();
    void requestMylistExportManually();
signals:
	void hashFiles(QStringList);
	void notifyStopHasher();
public:
	// page hasher
    hashes_ *hashes;
	void hashesinsertrow(QFileInfo, Qt::CheckState);
	int parseMylistExport(const QString &tarGzPath);
    Window();
	~Window();
};

#endif // WINDOW_H
