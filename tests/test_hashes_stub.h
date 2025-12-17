#ifndef TEST_HASHES_STUB_H
#define TEST_HASHES_STUB_H

#include <QTableWidget>
#include <QEvent>

/**
 * Minimal declaration of hashes_ class for test purposes
 * This avoids including window.h which would pull in the entire Window class
 */
class hashes_ : public QTableWidget
{
    Q_OBJECT
public:
    bool event(QEvent *e);
};

#endif // TEST_HASHES_STUB_H
