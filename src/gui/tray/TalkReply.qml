import QtQuick 2.15
import Style 1.0

Rectangle {    
    width: parent.width
    height: activityTextInfo.height
    border.width: 1
    border.color: Style.ncBlue
    radius: 2     
    clip: true
    TextInput {
        id: talkMessage
        font.pixelSize: Style.subLinePixelSize
        height: activityTextInfo.height
        width: parent.width
    }
}
