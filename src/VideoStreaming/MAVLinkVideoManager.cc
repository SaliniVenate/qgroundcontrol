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
    , _cameraSysid(0)
    , _cameraLink(NULL)
    , _selectedStream(-1)
    , _currentResolution(0)
{
    _mavlink = qgcApp()->toolbox()->mavlinkProtocol();
    connect(_mavlink, &MAVLinkProtocol::videoHeartbeatInfo, this, &MAVLinkVideoManager::_videoHeartbeatInfo);
    connect(_mavlink, &MAVLinkProtocol::messageReceived, this, &MAVLinkVideoManager::_mavlinkMessageReceived);

    _resolutionList.append(new Resolution("Default", 0, 0));
    _resolutionList.append(new Resolution("4K (3840x2160)", 3840, 2160));
    _resolutionList.append(new Resolution("1080p (1920x1080)", 1920, 1080));
    _resolutionList.append(new Resolution("720p (1280x720)", 1280, 720));
    _resolutionList.append(new Resolution("VGA (640x480)", 640, 480));
}

//-----------------------------------------------------------------------------
MAVLinkVideoManager::~MAVLinkVideoManager()
{
}

//-----------------------------------------------------------------------------

/**
 * This methos detects the first heartbeat from csd and requests camera information with command MAV_CMD_REQUEST_CAMERA_INFORMATION
 * @param link : The interface to read from and send to
 * @param message : The recieved mavlink message
 */

void MAVLinkVideoManager::_videoHeartbeatInfo(LinkInterface *link, int systemId)
{
    if (systemId == _cameraSysid)
        return;

    qDebug() << "MAVLinkVideoManager: First camera heartbeat info received";
    _cameraSysid = systemId;
    _cameraLink = link;
    link->setActive(true);

    mavlink_message_t msg;
    mavlink_msg_command_long_pack(_mavlink->getSystemId(), _mavlink->getComponentId(), &msg, _cameraSysid, MAV_COMP_ID_CAMERA, MAV_CMD_REQUEST_CAMERA_INFORMATION, 0, 0, 1, 0, 0, 0, 0, 0);

    uint8_t buffer[MAVLINK_MAX_PACKET_LEN];
    int len = mavlink_msg_to_send_buffer(buffer, &msg);

    _cameraLink->writeBytesSafe((const char*)buffer, len);

    qDebug() << "Request camera information sent:"<<msg.msgid;
}

Stream *MAVLinkVideoManager::_find_camera_by_id(int cameraId)
{
    QObject *s;
    foreach(s, _streamList) {
        if (((Stream *)s)->cameraId == cameraId)
            return (Stream *)s;
    }

    return NULL;
}

/**
 * @brief This method prints the camera information
 * @param link : The interface to read from and send to
 * @param message : The recieved mavlink message
 */
void MAVLinkVideoManager::_mavlinkMessageReceived(LinkInterface* link, mavlink_message_t message)
{
    Q_UNUSED(link);

    if(message.msgid == MAVLINK_MSG_ID_HEARTBEAT || message.sysid != _cameraSysid)
        return;

    qDebug() << "Camera message received" << message.msgid;
    switch(message.msgid) {
    case MAVLINK_MSG_ID_CAMERA_INFORMATION: {
        mavlink_camera_information_t info;
        mavlink_msg_camera_information_decode(&message, &info);

        Stream *s = _find_camera_by_id(info.camera_id);
        if (s) {
            //Camera already added to stream list. Droping duplicate.
            break;
        }
        _streamList.append(new Stream(info.camera_id, (const char *)info.model_name));
        if (_selectedStream < 0)
            setSelectedStream(0);

        emit streamListChanged();

        qDebug() << "Camera found:@ id:" << info.camera_id;
        qDebug() << "model:" << (const char *)info.model_name;
        break;
    }
    case MAVLINK_MSG_ID_VIDEO_STREAM_INFORMATION: {
        mavlink_video_stream_information_t info;
        mavlink_msg_video_stream_information_decode(&message, &info);

        Stream *s = _find_camera_by_id(info.camera_id);
        if (!s) {
            qDebug() << "Camera " << info.camera_id << " removed. Ignoring message.";
            break;
        }
        s->uri = info.uri;

        emit currentUriChanged();
        break;
    }
    }
}

QString MAVLinkVideoManager::getVideoURI()
{
    if (_selectedStream < 0 || _selectedStream >= _streamList.size())
        return QString("");

    return ((Stream *)_streamList[_selectedStream])->uri;
}

void MAVLinkVideoManager::_updateStream()
{
    Stream *s = (Stream *)_streamList[_selectedStream];
    uint8_t buffer[MAVLINK_MAX_PACKET_LEN];
    mavlink_message_t msg;

    mavlink_msg_command_long_pack(_mavlink->getSystemId(), _mavlink->getComponentId(), &msg, _cameraSysid, MAV_COMP_ID_CAMERA, MAV_CMD_REQUEST_VIDEO_STREAM_INFORMATION, 0, s->cameraId, 1, 0, 0, 0, 0, 0);

    int len = mavlink_msg_to_send_buffer(buffer, &msg);
    _cameraLink->writeBytesSafe((const char*)buffer, len);

    emit currentUriChanged();
}

void MAVLinkVideoManager::setSelectedStream(int index)
{
    if (index >= _streamList.size())
        index = -1;
    _selectedStream = index;

    if (index >= 0){
        _updateStream();
    setCurrentFrameSize(0);
    emit currentFrameSizeChanged();
    }
}

int MAVLinkVideoManager::currentFrameSize()
{
    return _currentFrameSize;
}

void MAVLinkVideoManager::setCurrentFrameSize(int frameSize)
{
    if (_selectedStream < 0 || _selectedStream >= _streamList.size())
        return;

    if (frameSize < 0 || frameSize >= _frameSizeList.size())
        return;
     
    _currentFrameSize=frameSize;
    Stream *s = (Stream *)_streamList[_selectedStream];
    FrameSize *fs = (FrameSize *)_frameSizeList[_currentFrameSize];
    uint8_t buffer[MAVLINK_MAX_PACKET_LEN];
    mavlink_message_t msg;

    mavlink_msg_set_video_stream_settings_pack(_mavlink->getSystemId(), _mavlink->getComponentId(), &msg, _cameraSysid, MAV_COMP_ID_CAMERA, s->cameraId, 0, fs->h, fs->v, 0, 0, "");
    int len = mavlink_msg_to_send_buffer(buffer, &msg);
    _cameraLink->writeBytesSafe((const char*)buffer, len);
    _updateStream();
}

void MAVLinkVideoManager::refreshVideoProvider()
{
    _selectedStream = -1;
    _currentResolution = 0;
    _cameraSysid = 0;

    _streamList.clear();

    emit currentUriChanged();
    emit currentFrameSizeChanged();
    emit currentUriChanged();
   
}

