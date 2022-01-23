import QtQuick 2.15
import Style 1.0
import QtQuick.Controls 2.2
import com.nextcloud.desktopclient 1.0

Rectangle {  
    visible: (model.objectType === "chat" || model.objectType === "call")
    width: parent.width
    height: 20
    border.width: 1
    border.color: Style.menuBorder
    radius: 1     
    clip: true
    TextInput {
        id: talkMessage
        font.pixelSize: Style.subLinePixelSize
        height: activityTextInfo.height
        width: parent.width
    }    
    Button {
        id: sendReply
        icon.source: "qrc:///client/theme/confirm.svg"
        icon.width: 16
        icon.height: 16
        width: 18
        height: parent.height
        opacity: 1.0
        anchors.right: talkMessage.right
        anchors.top: talkMessage.top
        onClicked: Systray.sendChatMessage(model.conversationToken, talkMessage.text, model.messageId)
    }
}
