#pragma once

#include <QtCore>
#include <QPointer>

class QJsonArray;

namespace OCC {

class Activity;
class JsonApiJob;
class AccountState;
class Systray;

class TalkReply : public QObject
{
    Q_OBJECT
        
public:
    explicit TalkReply(AccountState *accountState, QObject *parent = nullptr);

    int lastMessageId(const QString &chatToken) const;
    int lastMessageSentId() const;
    QJsonArray userMessagesList() const;
    
public slots:
        void sendChatMessage(const QString &token, const QString &message, const QString &replyTo = "");
  
signals:
    void messageSent(const QString &message);
    void userMessagesFetched();
    
private:
    AccountState *_accountState;
    int _userMessageId{};
    int _lastMessageSentId{};
    QJsonArray _userMessagesList{};
};
}
