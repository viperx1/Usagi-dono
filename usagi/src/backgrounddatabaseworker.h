#ifndef BACKGROUNDDATABASEWORKER_H
#define BACKGROUNDDATABASEWORKER_H

#include <QObject>
#include <QString>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include "logger.h"

/**
 * BackgroundDatabaseWorker - Base class for background database operations
 * 
 * This template class provides a common pattern for executing database queries
 * in background threads. It handles:
 * - Thread-specific database connection setup
 * - Connection lifecycle management
 * - Error handling
 * - Result emission via signals
 * 
 * Design principles:
 * - Template Method Pattern: Subclasses implement executeQuery()
 * - RAII-style resource management for database connections
 * - Type-safe results via template parameter
 * 
 * Usage:
 *   class MyWorker : public BackgroundDatabaseWorker<QList<int>> {
 *       QList<int> executeQuery(QSqlDatabase &db) override {
 *           // Execute query and return results
 *       }
 *   };
 */
template<typename ResultType>
class BackgroundDatabaseWorker : public QObject
{
public:
    explicit BackgroundDatabaseWorker(const QString &dbName, const QString &connectionName)
        : m_dbName(dbName)
        , m_connectionName(connectionName)
    {
    }
    
    virtual ~BackgroundDatabaseWorker() = default;
    
    /**
     * Main work method - called from background thread
     * Manages database connection lifecycle and calls executeQuery()
     */
    void doWork()
    {
        LOG(QString("Background thread: Starting database operation (%1)...").arg(m_connectionName));
        
        ResultType result = getDefaultResult();
        
        {
            // Create separate database connection for this thread
            QSqlDatabase db = QSqlDatabase::addDatabase("QSQLITE", m_connectionName);
            db.setDatabaseName(m_dbName);
            
            if (!db.open()) {
                LOG(QString("Background thread: Failed to open database for %1").arg(m_connectionName));
                QSqlDatabase::removeDatabase(m_connectionName);
                emitFinished(result);
                return;
            }
            
            // Execute the query (implemented by subclass)
            try {
                result = executeQuery(db);
            } catch (...) {
                LOG(QString("Background thread: Exception during query execution for %1").arg(m_connectionName));
                db.close();
                QSqlDatabase::removeDatabase(m_connectionName);
                emitFinished(getDefaultResult());
                return;
            }
            
            db.close();
            // Query objects destroyed here when leaving scope
        }
        
        // Now safe to remove database connection
        QSqlDatabase::removeDatabase(m_connectionName);
        
        LOG(QString("Background thread: Completed database operation (%1)").arg(m_connectionName));
        
        emitFinished(result);
    }
    
protected:
    /**
     * Subclasses implement this to execute their specific query
     * @param db Open database connection (guaranteed to be valid)
     * @return Query results of type ResultType
     */
    virtual ResultType executeQuery(QSqlDatabase &db) = 0;
    
    /**
     * Subclasses implement this to provide a default/empty result
     * Used when database connection fails or query throws exception
     */
    virtual ResultType getDefaultResult() const = 0;
    
    /**
     * Subclasses implement this to emit their specific finished signal
     * This allows type-safe signal emission with the correct signature
     */
    virtual void emitFinished(const ResultType &result) = 0;
    
    QString m_dbName;
    QString m_connectionName;
};

#endif // BACKGROUNDDATABASEWORKER_H
