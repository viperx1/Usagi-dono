#ifndef PLAYBACKMANAGER_H
#define PLAYBACKMANAGER_H

#include <QObject>
#include <QString>
#include <QProcess>
#include <QTimer>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QRegularExpression>

/**
 * @brief Manages media playback and tracking
 * 
 * This class handles launching the media player (MPC-HC), tracking playback
 * progress via the web interface, and updating the database with playback state.
 */
class PlaybackManager : public QObject
{
    Q_OBJECT
    
public:
    explicit PlaybackManager(QObject *parent = nullptr);
    ~PlaybackManager();
    
    /**
     * @brief Start playback of a file
     * @param filePath Full path to the media file
     * @param lid MyList ID for tracking
     * @param resumePosition Optional position to resume from (in seconds), 0 for start
     * @return true if playback was started successfully
     */
    bool startPlayback(const QString &filePath, int lid, int resumePosition = 0);
    
    /**
     * @brief Stop tracking current playback
     */
    void stopTracking();
    
    /**
     * @brief Check if playback is currently being tracked
     */
    bool isTracking() const { return m_tracking; }
    
    /**
     * @brief Get the media player path from settings
     */
    static QString getMediaPlayerPath();
    
    /**
     * @brief Set the media player path in settings
     */
    static void setMediaPlayerPath(const QString &path);
    
signals:
    /**
     * @brief Emitted when playback position is updated
     * @param lid MyList ID
     * @param position Current position in seconds
     * @param duration Total duration in seconds
     */
    void playbackPositionUpdated(int lid, int position, int duration);
    
    /**
     * @brief Emitted when playback is completed
     * @param lid MyList ID
     */
    void playbackCompleted(int lid);
    
    /**
     * @brief Emitted when playback stops (file closed or player closed)
     * @param lid MyList ID
     * @param position Last known position in seconds
     */
    void playbackStopped(int lid, int position);
    
private slots:
    void checkPlaybackStatus();
    void handleStatusReply();
    
private:
    void savePlaybackPosition(int position, int duration);
    
    // Playback state
    bool m_tracking;
    int m_currentLid;
    QString m_currentFilePath;
    int m_lastPosition;
    int m_lastDuration;
    
    QTimer *m_statusTimer;
    QNetworkAccessManager *m_networkManager;
    
    // Configuration constants
    static const int MPC_WEB_PORT = 13579;
    static const int STATUS_POLL_INTERVAL_MS = 1000;  // Poll every second
    static const int PLAYER_STARTUP_DELAY_MS = 2000;  // Wait 2 seconds for player to start
    static const int SAVE_INTERVAL_SECONDS = 5;       // Save position every 5 seconds
    static const int COMPLETION_THRESHOLD_SECONDS = 5; // Mark complete within 5s of end
    static const int MAX_CONSECUTIVE_FAILURES = 5;     // Stop tracking after 5 failures
    
    QString m_mpcStatusUrl;
};

#endif // PLAYBACKMANAGER_H
