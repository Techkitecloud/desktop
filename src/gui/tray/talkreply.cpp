#include "talkreply.h"
#include "accountstate.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>

namespace OCC {

Q_LOGGING_CATEGORY(lcTalkReply, "nextcloud.gui.talkreply", QtInfoMsg)

TalkReply::TalkReply(AccountState *accountState, QObject *parent)
    : QObject(parent)
    , _accountState(accountState)
{
    
}

int TalkReply::lastMessageId(const QString &chatToken) const
{
    Q_UNUSED(chatToken);
    
    if (_userMessagesList.size() == 0) {
        qCWarning(lcTalkReply) << "There are no messages for this user.";
        return 0;
    }
    
    for (const QJsonValue &value : _userMessagesList) {
        auto room = value.toObject();
        if (room.value("token").toString() != chatToken) {
            continue;
        }
        
        const auto lastMessage = room.value("lastMessage").toObject();
        if (lastMessage.value("actorType").toString() == QStringLiteral("bots")) {
            return 0;
        }
        
        return lastMessage.value("id").toInt();
    }
    
    return _userMessageId;
}

int TalkReply::lastMessageSentId() const
{
    return _lastMessageSentId;
}

QJsonArray TalkReply::userMessagesList() const
{
    return _userMessagesList;
}

void TalkReply::sendChatMessage(const QString &token, const QString &message, const QString &replyTo)
{
    Q_ASSERT(_accountState && _accountState->account());
    
    if (!_accountState || !_accountState->account()) {
        return;
    }

    const auto apiJob =  new JsonApiJob(_accountState->account(),
        QLatin1String("ocs/v2.php/apps/spreed/api/v1/chat/%1").arg(token),
        this);
    
    QObject::connect(apiJob, &JsonApiJob::jsonReceived, this, [&](const QJsonDocument &response,
                                                                              int statusCode) {
        if(statusCode != 200) {
            qCWarning(lcTalkReply) << "Status code" << statusCode;
        }
        
        sender()->deleteLater();
        
        const auto responseObj = response.object().value("ocs").toObject().value("data").toObject();
        _lastMessageSentId = responseObj.value("id").toInt();
        
        emit messageSent(responseObj.value("message").toString());
    });

    QUrlQuery params;
    params.addQueryItem(QStringLiteral("message"), message);
    params.addQueryItem(QStringLiteral("replyTo"), QString(replyTo));

    apiJob->addQueryParams(params);
    apiJob->setVerb(JsonApiJob::Verb::Post);
    apiJob->start();
}
}
