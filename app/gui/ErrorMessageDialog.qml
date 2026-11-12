import QtQuick 2.0
import QtQuick.Controls 2.2

import SystemProperties 1.0

NavigableMessageDialog {
    standardButtons: Dialog.Ok | (SystemProperties.hasBrowser && helpUrl !== "" ? Dialog.Help : 0)
}
