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
