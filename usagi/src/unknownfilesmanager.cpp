#include "unknownfilesmanager.h"
#include "anidbapi.h"
#include "hashercoordinator.h"
#include "logger.h"
#include "window.h"  // For unknown_files_ class definition
#include <QFile>
#include <QFileInfo>
#include <QMessageBox>
#include <QScrollBar>
#include <QHeaderView>

UnknownFilesManager::UnknownFilesManager(AniDBApi *api, HasherCoordinator *hasherCoord, QObject *parent)
    : QObject(parent)
    , m_api(api)
    , m_hasherCoordinator(hasherCoord)
    , m_containerWidget(nullptr)
    , m_tableWidget(nullptr)
    , m_titleLabel(nullptr)
{
    createUI();
}

UnknownFilesManager::~UnknownFilesManager()
{
    // Container widget will be deleted by Qt's parent-child system
}

void UnknownFilesManager::createUI()
{
    // Create the table widget using unknown_files_ custom class
    m_tableWidget = new unknown_files_();
    m_tableWidget->setColumnCount(4);
    m_tableWidget->setHorizontalHeaderLabels({"Filename", "Anime", "Episode", "Actions"});
    m_tableWidget->horizontalHeader()->setStretchLastSection(false);
    m_tableWidget->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    m_tableWidget->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Interactive);
    m_tableWidget->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Interactive);
    m_tableWidget->horizontalHeader()->setSectionResizeMode(3, QHeaderView::Fixed);
    m_tableWidget->setColumnWidth(1, 250);
    m_tableWidget->setColumnWidth(2, 120);
    m_tableWidget->setColumnWidth(3, 360);
    m_tableWidget->setMinimumHeight(60);
    m_tableWidget->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    m_tableWidget->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_tableWidget->setSelectionMode(QAbstractItemView::SingleSelection);
    m_tableWidget->setEditTriggers(QAbstractItemView::NoEditTriggers);
    
    // Create the title label
    m_titleLabel = new QLabel("Unknown Files (not in AniDB database):");
    m_titleLabel->setObjectName("unknownFilesLabel");
    m_titleLabel->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    
    // Create the container widget with layout
    m_containerWidget = new QWidget();
    m_containerWidget->setObjectName("unknownFilesContainer");
    m_containerWidget->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    
    QVBoxLayout *layout = new QVBoxLayout(m_containerWidget);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    layout->addWidget(m_titleLabel, 0);
    layout->addWidget(m_tableWidget, 1);
    
    // Initially hide the container
    m_containerWidget->hide();
}

void UnknownFilesManager::setAnimeTitlesCache(const QStringList& titles, const QMap<QString, int>& titleToAid)
{
    m_cachedAnimeTitles = titles;
    m_cachedTitleToAid = titleToAid;
}

void UnknownFilesManager::setUpdatesEnabled(bool enable)
{
    m_tableWidget->setUpdatesEnabled(enable);
}

int UnknownFilesManager::findRowByFilepath(const QString& filepath) const
{
    for(int i = 0; i < m_tableWidget->rowCount(); ++i) {
        QTableWidgetItem *item = m_tableWidget->item(i, 0);
        if(item && item->toolTip() == filepath) {
            return i;
        }
    }
    return -1;
}

void UnknownFilesManager::reindexFilesData(int removedRow)
{
    QMap<int, LocalFileInfo> newMap;
    for(auto it = m_filesData.begin(); it != m_filesData.end(); ++it)
    {
        int oldRow = it.key();
        if(oldRow > removedRow) {
            newMap[oldRow - 1] = it.value();
        } else {
            newMap[oldRow] = it.value();
        }
    }
    m_filesData = newMap;
}

void UnknownFilesManager::insertFile(const QString& filename, const QString& filepath, const QString& hash, qint64 size)
{
    // Show the container if hidden
    if(m_containerWidget->isHidden()) {
        m_containerWidget->show();
    }
    
    int row = m_tableWidget->rowCount();
    m_tableWidget->insertRow(row);
    
    // Column 0: Filename
    QTableWidgetItem *filenameItem = new QTableWidgetItem(filename);
    filenameItem->setToolTip(filepath);
    m_tableWidget->setItem(row, 0, filenameItem);
    
    // Column 1: Anime search field (QLineEdit with autocomplete)
    QLineEdit *animeSearch = new QLineEdit();
    animeSearch->setPlaceholderText("Search anime title...");
    
    // Set up autocomplete with cached titles
    if(!m_cachedAnimeTitles.isEmpty()) {
        QCompleter *completer = new QCompleter(m_cachedAnimeTitles, animeSearch);
        completer->setCaseSensitivity(Qt::CaseInsensitive);
        completer->setFilterMode(Qt::MatchContains);
        animeSearch->setCompleter(completer);
    }
    
    m_tableWidget->setCellWidget(row, 1, animeSearch);
    
    // Column 2: Episode input (QLineEdit with manual entry)
    QLineEdit *episodeInput = new QLineEdit();
    episodeInput->setPlaceholderText("Enter episode number...");
    episodeInput->setEnabled(false);
    m_tableWidget->setCellWidget(row, 2, episodeInput);
    
    // Column 3: Action buttons
    QWidget *actionContainer = new QWidget();
    QHBoxLayout *actionLayout = new QHBoxLayout(actionContainer);
    actionLayout->setContentsMargins(2, 2, 2, 2);
    actionLayout->setSpacing(4);
    
    QPushButton *bindButton = new QPushButton("Bind");
    bindButton->setEnabled(false);
    bindButton->setProperty("filepath", filepath);
    
    QPushButton *notAnimeButton = new QPushButton("Not Anime");
    notAnimeButton->setProperty("filepath", filepath);
    
    QPushButton *recheckButton = new QPushButton("Re-check");
    recheckButton->setProperty("filepath", filepath);
    recheckButton->setToolTip("Re-validate this file against AniDB (in case it was added since last check)");
    
    QPushButton *deleteButton = new QPushButton("Delete");
    deleteButton->setProperty("filepath", filepath);
    deleteButton->setToolTip("Delete this file from the filesystem");
    deleteButton->setStyleSheet("QPushButton { color: red; }");
    
    actionLayout->addWidget(bindButton);
    actionLayout->addWidget(notAnimeButton);
    actionLayout->addWidget(recheckButton);
    actionLayout->addWidget(deleteButton);
    actionContainer->setLayout(actionLayout);
    
    m_tableWidget->setCellWidget(row, 3, actionContainer);
    
    // Store file data
    LocalFileInfo fileInfo(filename, filepath, hash, size);
    fileInfo.setSelectedAid(-1);
    fileInfo.setSelectedEid(-1);
    m_filesData[row] = fileInfo;
    
    // Connect anime search changes
    connect(animeSearch, &QLineEdit::textChanged, this, [this, filepath, animeSearch, episodeInput, bindButton]() {
        onAnimeSearchChanged(filepath, animeSearch->text(), episodeInput, bindButton);
    });
    
    // Connect episode input changes
    connect(episodeInput, &QLineEdit::textChanged, this, [this, filepath, episodeInput, bindButton]() {
        onEpisodeInputChanged(filepath, episodeInput->text().trimmed(), bindButton);
    });
    
    // Connect bind button
    connect(bindButton, &QPushButton::clicked, this, [this, filepath, episodeInput]() {
        int currentRow = findRowByFilepath(filepath);
        if(currentRow >= 0) {
            onBindClicked(currentRow, episodeInput->text().trimmed());
        }
    });
    
    // Connect "Not Anime" button
    connect(notAnimeButton, &QPushButton::clicked, this, [this, filepath]() {
        emit logMessage("Not Anime button clicked");
        emit logMessage(QString("Not Anime button filepath: %1").arg(filepath));
        
        int currentRow = findRowByFilepath(filepath);
        emit logMessage(QString("Not Anime button found row: %1").arg(currentRow));
        
        if(currentRow >= 0) {
            onNotAnimeClicked(currentRow);
        } else {
            emit logMessage("ERROR: Could not find row for filepath");
        }
    });
    
    // Connect "Re-check" button
    connect(recheckButton, &QPushButton::clicked, this, [this, filepath]() {
        emit logMessage("Re-check button clicked");
        emit logMessage(QString("Re-check button filepath: %1").arg(filepath));
        
        int currentRow = findRowByFilepath(filepath);
        emit logMessage(QString("Re-check button found row: %1").arg(currentRow));
        
        if(currentRow >= 0) {
            onRecheckClicked(currentRow);
        } else {
            emit logMessage("ERROR: Could not find row for filepath");
        }
    });
    
    // Connect "Delete" button
    connect(deleteButton, &QPushButton::clicked, this, [this, filepath]() {
        emit logMessage("Delete button clicked");
        emit logMessage(QString("Delete button filepath: %1").arg(filepath));
        
        int currentRow = findRowByFilepath(filepath);
        emit logMessage(QString("Delete button found row: %1").arg(currentRow));
        
        if(currentRow >= 0) {
            onDeleteClicked(currentRow);
        } else {
            emit logMessage("ERROR: Could not find row for filepath");
        }
    });
}

void UnknownFilesManager::onAnimeSearchChanged(const QString& filepath, const QString& searchText, 
                                               QLineEdit* episodeInput, QPushButton* bindButton)
{
    int currentRow = findRowByFilepath(filepath);
    if(currentRow < 0 || !m_filesData.contains(currentRow)) {
        return;
    }
    
    // Check if the text matches a valid anime from autocomplete
    if(m_cachedTitleToAid.contains(searchText))
    {
        int aid = m_cachedTitleToAid[searchText];
        m_filesData[currentRow].setSelectedAid(aid);
        
        // Enable episode input
        episodeInput->setEnabled(true);
        episodeInput->setPlaceholderText("Enter episode number (e.g., 1, S1, etc.)...");
    }
    else
    {
        // Clear episode selection if anime is not valid
        episodeInput->clear();
        episodeInput->setEnabled(false);
        episodeInput->setPlaceholderText("Select anime first...");
        bindButton->setEnabled(false);
        m_filesData[currentRow].setSelectedAid(-1);
    }
}

void UnknownFilesManager::onEpisodeInputChanged(const QString& filepath, const QString& episodeText, 
                                                QPushButton* bindButton)
{
    int currentRow = findRowByFilepath(filepath);
    if(currentRow < 0 || !m_filesData.contains(currentRow)) {
        return;
    }
    
    if(!episodeText.isEmpty() && m_filesData[currentRow].selectedAid() > 0)
    {
        bindButton->setEnabled(true);
    }
    else
    {
        bindButton->setEnabled(false);
    }
}

void UnknownFilesManager::onBindClicked(int row, const QString& epno)
{
    if(!m_filesData.contains(row))
    {
        emit logMessage(QString("Error: Unknown file data not found for row %1").arg(row));
        return;
    }
    
    LocalFileInfo &fileInfo = m_filesData[row];
    
    if(fileInfo.selectedAid() <= 0)
    {
        emit logMessage(QString("Error: Invalid anime selection for row %1").arg(row));
        QMessageBox::warning(nullptr, "Invalid Selection", "Please select an anime before binding.");
        return;
    }
    
    if(epno.isEmpty())
    {
        emit logMessage(QString("Error: Empty episode number for row %1").arg(row));
        QMessageBox::warning(nullptr, "Invalid Episode", "Please enter an episode number.");
        return;
    }
    
    emit logMessage(QString("Binding unknown file: %1 to anime %2, episode %3")
        .arg(fileInfo.filename())
        .arg(fileInfo.selectedAid())
        .arg(epno));
    
    // Prepare the "other" field with file details
    QString otherField = QString("File: %1\nHash: %2\nSize: %3")
        .arg(fileInfo.filename(), fileInfo.hash())
        .arg(fileInfo.size());
    
    // Truncate if too long (limit to 100 chars for API compatibility)
    if(otherField.length() > 100)
    {
        otherField = otherField.left(97) + "...";
        emit logMessage(QString("Truncated 'other' field to 100 chars for API compatibility"));
    }
    
    // Add to mylist via API using generic parameter
    if(m_api->LoggedIn())
    {
        emit logMessage(QString("Adding unknown file to mylist using generic: aid=%1, epno=%2")
            .arg(fileInfo.selectedAid())
            .arg(epno));
        
        // Use reasonable defaults for unknown files
        int viewed = -1; // no change
        int state = 1; // Internal (HDD)
        QString storageStr = "";
        
        m_api->MylistAddGeneric(fileInfo.selectedAid(), epno, viewed, state, storageStr, otherField);
        
        // Mark the file in local_files as bound to anime (binding_status 1)
        m_api->UpdateLocalFileBindingStatus(fileInfo.filepath(), 1);
        
        // Remove from unknown files widget after successful binding
        m_tableWidget->removeRow(row);
        m_filesData.remove(row);
        reindexFilesData(row);
        
        // Hide the widget if no more unknown files
        if(m_tableWidget->rowCount() == 0)
        {
            m_containerWidget->hide();
        }
        
        emit logMessage(QString("Successfully bound unknown file to anime %1, episode %2")
            .arg(fileInfo.selectedAid())
            .arg(epno));
    }
    else
    {
        QMessageBox::warning(nullptr, "Cannot Add", 
            "Please enable 'Add file(s) to MyList' and ensure you are logged in.");
    }
}

void UnknownFilesManager::onNotAnimeClicked(int row)
{
    if(!m_filesData.contains(row))
    {
        emit logMessage(QString("Error: Unknown file data not found for row %1").arg(row));
        return;
    }
    
    LocalFileInfo &fileInfo = m_filesData[row];
    
    emit logMessage(QString("Marking file as not anime: %1").arg(fileInfo.filename()));
    
    // Mark the file in local_files as not anime (binding_status 2)
    m_api->UpdateLocalFileBindingStatus(fileInfo.filepath(), 2);
    
    // Remove from unknown files widget
    m_tableWidget->removeRow(row);
    m_filesData.remove(row);
    reindexFilesData(row);
    
    // Hide the widget if no more unknown files
    if(m_tableWidget->rowCount() == 0)
    {
        m_containerWidget->hide();
    }
}

void UnknownFilesManager::onRecheckClicked(int row)
{
    if(!m_filesData.contains(row))
    {
        emit logMessage(QString("Error: Unknown file data not found for row %1").arg(row));
        return;
    }
    
    LocalFileInfo &fileInfo = m_filesData[row];
    
    emit logMessage(QString("Re-checking file against AniDB: %1").arg(fileInfo.filename()));
    emit logMessage(QString("Hash: %1, Size: %2").arg(fileInfo.hash()).arg(fileInfo.size()));
    
    // Emit signal to add file to hashes table for checking
    QFileInfo qFileInfo(fileInfo.filepath());
    emit fileNeedsHashing(qFileInfo, Qt::Unchecked, fileInfo.hash());
    
    // Call MylistAdd API with the hash and size
    QString tag = m_api->MylistAdd(fileInfo.size(), fileInfo.hash(), 
                                    m_hasherCoordinator->getMarkWatched()->checkState(), 
                                    m_hasherCoordinator->getHasherFileState()->currentIndex(), 
                                    m_hasherCoordinator->getStorage()->text());
    
    emit logMessage(QString("Sent re-check request with tag: %1").arg(tag));
    emit logMessage(QString("Re-check initiated for file: %1").arg(fileInfo.filename()));
    
    // Request hasher to start if not already running
    emit requestStartHasher();
}

void UnknownFilesManager::onDeleteClicked(int row)
{
    if(!m_filesData.contains(row))
    {
        emit logMessage(QString("Error: Unknown file data not found for row %1").arg(row));
        return;
    }
    
    LocalFileInfo fileInfo = m_filesData[row];  // Copy to avoid invalidation
    
    emit logMessage(QString("Delete button clicked for file: %1").arg(fileInfo.filename()));
    
    // Show confirmation dialog
    QMessageBox::StandardButton reply = QMessageBox::question(
        nullptr, 
        "Confirm File Deletion",
        QString("Are you sure you want to permanently delete this file?\n\n"
                "File: %1\n\n"
                "This action cannot be undone!")
            .arg(fileInfo.filename()),
        QMessageBox::Yes | QMessageBox::No,
        QMessageBox::No
    );
    
    if(reply != QMessageBox::Yes)
    {
        emit logMessage(QString("File deletion cancelled by user: %1").arg(fileInfo.filename()));
        return;
    }
    
    // Attempt to delete the physical file
    QFile file(fileInfo.filepath());
    if(!file.exists())
    {
        emit logMessage(QString("Error: File does not exist: %1").arg(fileInfo.filepath()));
        QMessageBox::warning(nullptr, "File Not Found", 
            QString("The file no longer exists:\n%1").arg(fileInfo.filepath()));
        
        // Save scroll position before removing row
        int scrollPos = m_tableWidget->verticalScrollBar()->value();
        
        // Disconnect all button signals for this row
        QWidget *cellWidget = m_tableWidget->cellWidget(row, 3);
        if (cellWidget) {
            QList<QPushButton*> buttons = cellWidget->findChildren<QPushButton*>();
            for (QPushButton* btn : buttons) {
                disconnect(btn, nullptr, this, nullptr);
            }
        }
        
        // Remove from UI anyway since file doesn't exist
        m_tableWidget->removeRow(row);
        m_filesData.remove(row);
        reindexFilesData(row);
        
        // Restore scroll position
        m_tableWidget->verticalScrollBar()->setValue(scrollPos);
        
        // Hide container if empty
        if(m_tableWidget->rowCount() == 0)
        {
            m_containerWidget->hide();
        }
        
        return;
    }
    
    // Delete the file
    if(file.remove())
    {
        emit logMessage(QString("Successfully deleted file: %1").arg(fileInfo.filepath()));
        
        // Update database to mark file as deleted
        m_api->UpdateLocalFileStatus(fileInfo.filepath(), 4); // 4 = deleted
        
        // Save scroll position before removing row
        int scrollPos = m_tableWidget->verticalScrollBar()->value();
        
        // Disconnect all button signals for this row
        QWidget *cellWidget = m_tableWidget->cellWidget(row, 3);
        if (cellWidget) {
            QList<QPushButton*> buttons = cellWidget->findChildren<QPushButton*>();
            for (QPushButton* btn : buttons) {
                disconnect(btn, nullptr, this, nullptr);
            }
        }
        
        // Remove from UI
        m_tableWidget->removeRow(row);
        m_filesData.remove(row);
        reindexFilesData(row);
        
        // Restore scroll position
        m_tableWidget->verticalScrollBar()->setValue(scrollPos);
        
        // Hide container if empty
        if(m_tableWidget->rowCount() == 0)
        {
            m_containerWidget->hide();
        }
        
        QMessageBox::information(nullptr, "File Deleted", 
            QString("File successfully deleted:\n%1").arg(fileInfo.filename()));
    }
    else
    {
        emit logMessage(QString("Error: Failed to delete file: %1").arg(fileInfo.filepath()));
        QMessageBox::critical(nullptr, "Delete Failed", 
            QString("Failed to delete file:\n%1\n\nError: %2")
                .arg(fileInfo.filepath(), file.errorString()));
    }
}

void UnknownFilesManager::removeFileByPath(const QString& filepath, int fromRow)
{
    int row = (fromRow >= 0) ? fromRow : findRowByFilepath(filepath);
    
    if(row < 0) {
        return;  // File not found
    }
    
    // Save scroll position
    int scrollPos = m_tableWidget->verticalScrollBar()->value();
    
    // Disconnect all button signals for this row
    QWidget *cellWidget = m_tableWidget->cellWidget(row, 3);
    if (cellWidget) {
        QList<QPushButton*> buttons = cellWidget->findChildren<QPushButton*>();
        for (QPushButton* btn : buttons) {
            disconnect(btn, nullptr, this, nullptr);
        }
    }
    
    // Remove from UI
    m_tableWidget->removeRow(row);
    m_filesData.remove(row);
    reindexFilesData(row);
    
    // Restore scroll position
    m_tableWidget->verticalScrollBar()->setValue(scrollPos);
    
    // Hide container if empty
    if(m_tableWidget->rowCount() == 0)
    {
        m_containerWidget->hide();
    }
}

int UnknownFilesManager::rescanAndFilterFiles()
{
    if (!m_hasherCoordinator) {
        emit logMessage("Error: HasherCoordinator not available for filter check");
        return 0;
    }
    
    emit logMessage("Re-scanning unknown files with current filter settings...");
    
    int removedCount = 0;
    QList<QString> filesToRemove;
    
    // First pass: collect files to remove (avoid modifying while iterating)
    for (int row = 0; row < m_tableWidget->rowCount(); ++row) {
        if (!m_filesData.contains(row)) {
            // Data synchronization issue - log for debugging
            emit logMessage(QString("Warning: Row %1 exists in table but not in filesData map").arg(row));
            continue;
        }
        
        const LocalFileInfo &fileInfo = m_filesData[row];
        QString filepath = fileInfo.filepath();
        
        // Check if file should be filtered based on current settings
        if (m_hasherCoordinator->shouldFilterFile(filepath)) {
            QString filename = fileInfo.filename();
            emit logMessage(QString("Marking filtered file for removal: %1").arg(filename));
            filesToRemove.append(filepath);
        }
    }
    
    // Second pass: remove the marked files
    if (!filesToRemove.isEmpty()) {
        // Disable table updates during batch removal for performance
        m_tableWidget->setUpdatesEnabled(false);
        
        for (const QString &filepath : filesToRemove) {
            removeFileByPath(filepath);
        }
        
        // Re-enable table updates
        m_tableWidget->setUpdatesEnabled(true);
        
        removedCount = filesToRemove.size();
    }
    
    if (removedCount > 0) {
        emit logMessage(QString("Re-scan complete: removed %1 file(s) matching filter patterns").arg(removedCount));
    } else {
        emit logMessage("Re-scan complete: no files matched filter patterns");
    }
    
    return removedCount;
}
