#pragma once

#include <QtCore>
#include <QPointer>

class QJsonArray;

namespace OCC {
class Activity;
class JsonApiJob;
class AccountState;

class TalkReply : public QObject
{
    Q_OBJECT
        
public:
    explicit TalkReply(AccountState *accountState, QObject *parent = nullptr);
    
    void sendChatMessage(const QString &token, const QString &message, const QString &replyTo = "");
  
signals:
    void messageSent(const QString &message);
    
private:
    AccountState *_accountState;
};
}
