/****************************************************************************
 *
 * Copyright (c) 2017, Intel Corporation
 *
 * QGroundControl is licensed according to the terms in the file
 * COPYING.md in the root of the source code directory.
 *
 ****************************************************************************/

#include <QDebug>

#include "MAVLinkVideoManager.h"
#include "LinkInterface.h"
#include "QGCApplication.h"

/**
 * @file
 *   @brief QGC MAVLink video streaming Manager
 */


//-----------------------------------------------------------------------------
MAVLinkVideoManager::MAVLinkVideoManager()
    : QObject()
    , _cameraId(0)
    , _cameraLink(NULL)

{
    _mavlink = qgcApp()->toolbox()->mavlinkProtocol();
    connect(_mavlink, &MAVLinkProtocol::videoHeartbeatInfo, this, &MAVLinkVideoManager::_videoHeartbeatInfo);
    connect(_mavlink, &MAVLinkProtocol::messageReceived, this, &MAVLinkVideoManager::_mavlinkMessageReceived);
}

//-----------------------------------------------------------------------------
MAVLinkVideoManager::~MAVLinkVideoManager()
{
}

//-----------------------------------------------------------------------------

/**
 * @brief MAVLinkVideoManager::_videoHeartbeatInfo
 * @param link
 * @param message
 */
void MAVLinkVideoManager::_videoHeartbeatInfo(LinkInterface *link, mavlink_message_t message)
{

    if (message.compid == _cameraId)
        return;

    qDebug() << "MAVLinkVideoManager: First camera heartbeat info received";
    qDebug() << "Heartbeat Recieved:"<<message.msgid;
    _cameraId = message.compid;
    _cameraLink = link;

    mavlink_message_t msg;
    mavlink_msg_command_long_pack(255, MAV_COMP_ID_ALL, &msg, 1, MAV_COMP_ID_CAMERA, MAV_CMD_REQUEST_CAMERA_INFORMATION, 0, 1, 0, 0, 0, 0, 0, 0);

    uint8_t buffer[MAVLINK_MAX_PACKET_LEN];
    int len = mavlink_msg_to_send_buffer(buffer, &msg);

    _cameraLink->writeBytesSafe((const char*)buffer, len);
    qDebug() << "Request camera information sent:"<<msg.msgid;
    return;
}

/**
 * @brief MAVLinkVideoManager::_mavlinkMessageReceived
 * @param link
 * @param message
 */
void MAVLinkVideoManager::_mavlinkMessageReceived(LinkInterface* link, mavlink_message_t message)
{

    qDebug() << "Message received"<<message.msgid;
    if (message.compid != _cameraId)
        return;
    mavlink_camera_information_t info;
    mavlink_msg_camera_information_decode(&message, &info);
    qDebug()<<"Camera found:@ id:"<<info.camera_id;
    qDebug()<<"model:"<<(const char *)info.model_name;
}
