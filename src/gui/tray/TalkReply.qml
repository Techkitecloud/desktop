import QtQuick 2.15
import Style 1.0
import QtQuick.Controls 2.2
import QtQuick.Layouts 1.2
import com.nextcloud.desktopclient 1.0

ColumnLayout {
    id: talkLayout
    spacing: 0
    
    Layout.fillWidth: true        
    Layout.fillHeight: true        
    Layout.minimumWidth: 170
    Layout.minimumHeight: parent.height
    
    function sendMessage() {        
        if (talkMessage.text === "") {
            rectangleInput.border.color = Style.errorBoxBorderColor;
            return;
        }
        rectangleInput.border.color = Style.menuBorder;
        UserModel.currentUser.sendChatMessage(model.conversationToken, talkMessage.text, model.messageId);
        talkMessageSent.text = talkMessage.text;
        talkMessage.clear();
        rectangleInput.visible = false
    }
    
    Text {
        id: talkMessageSent
        color: Style.menuBorder
        visible: talkMessageSent.text !== ""
    }
    
    Rectangle {  
        id: rectangleInput
        width: 180
        height: 24
        border.width: 1
        border.color: Style.menuBorder
        radius: 3
        clip: true
        TextInput {
            id: talkMessage
            font.pixelSize: Style.topLinePixelSize
            height: activityTextInfo.height
            width: parent.width
            onAccepted: sendMessage()
            leftPadding: 4
            anchors.verticalCenter: parent.verticalCenter 
            Text {
                id: placeholderText
                text: qsTr("I will join the chat in 5")
                color: Style.menuBorder
                visible: talkMessage.text === "" && talkMessage.activeFocus
                width: parent.width
                height: parent.height
            }
        }    
        Button {
            id: sendReply
            icon.source: "qrc:///client/theme/confirm.svg"
            icon.width: 24
            icon.height: 24
            icon.color: "transparent"
            width: 24
            height: parent.height
            opacity: 0.8
            anchors.right: rectangleInput.right
            anchors.top: rectangleInput.top
            onClicked: sendMessage()
            flat: true
            background: Rectangle {
                color: parent.hovered ? Style.lightHover : "transparent"
            }
            
            ToolTip.visible: hovered
            ToolTip.delay: Qt.styleHints.mousePressAndHoldInterval
            ToolTip.text: qsTr("Send reply to chat message")        
        }
    }
}
