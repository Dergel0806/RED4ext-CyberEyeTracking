#include <stdafx.hpp>
#include "EyeTracker.hpp"

#include "..\..\deps\tobii\tobii.h"
#include "..\..\deps\tobii\tobii_streams.h"
#include <string.h>

tobii_device_t* device = NULL;
tobii_api_t* api;

float pos[2];
float head_pos[3];
float head_rotation[3];


CyberEyeTracking::EyeTracker::~EyeTracker()
{
    Finalize();
}

void url_receiver(char const* url, void* user_data)
{
    char* buffer = (char*)user_data;
    if (*buffer != '\0')
        return; // only keep first value

    if (strlen(url) < 256)
        strcpy(buffer, url);
}

void gaze_point_callback(tobii_gaze_point_t const* gaze_point, void* /* user_data */)
{
    // Check that the data is valid before using it
    if (gaze_point->validity == TOBII_VALIDITY_VALID)
    {
        pos[0] = gaze_point->position_xy[0];
        pos[1] = gaze_point->position_xy[1];
    }
}

void head_point_callback(tobii_head_pose_t const* head_pose, void* /* user_data */)
{
    // Check that the data is valid before using it
    if (head_pose->position_validity == TOBII_VALIDITY_VALID)
    {
        head_pos[0] = head_pose->position_xyz[0];
        head_pos[1] = head_pose->position_xyz[1];
        head_pos[2] = head_pose->position_xyz[2];
    }

    for (int i = 0; i < 3; ++i)
        if (head_pose->rotation_validity_xyz[i] == TOBII_VALIDITY_VALID)
            head_rotation[i]= head_pose->rotation_xyz[i];
}

bool CyberEyeTracking::EyeTracker::Init()
{
    // Create API
    api = NULL;
    auto result = tobii_api_create(&api, NULL, NULL);
    if (result != TOBII_ERROR_NO_ERROR)
        return false;

     // Enumerate devices to find connected eye trackers, keep the first
    char url[256] = { 0 };
    result = tobii_enumerate_local_device_urls(api, url_receiver, url);
    if (result != TOBII_ERROR_NO_ERROR)
        return false;

    if (*url == '\0')
    {
        return false;
    }

    // Connect to the first tracker found
    result = tobii_device_create(api, url, TOBII_FIELD_OF_USE_INTERACTIVE, &device);
    if (result != TOBII_ERROR_NO_ERROR)
        return false;

    // Subscribe to gaze data
    result = tobii_gaze_point_subscribe(device, gaze_point_callback, 0);
    if (result != TOBII_ERROR_NO_ERROR)
        return false;   

    result = tobii_head_pose_subscribe(device, head_point_callback, 0);
    if (result != TOBII_ERROR_NO_ERROR)
        return false;

    return true;
}

float* CyberEyeTracking::EyeTracker::GetPos()
{
    tobii_device_process_callbacks(device);
    return pos;
}

float* CyberEyeTracking::EyeTracker::GetHeadPos()
{
    tobii_device_process_callbacks(device);
    return head_pos;
}

float* CyberEyeTracking::EyeTracker::GetHeadRotation()
{
    tobii_device_process_callbacks(device);
    return head_rotation;
}

void CyberEyeTracking::EyeTracker::Finalize()
{
    try
    {
        if (api)
        {
            tobii_api_destroy(api);
            api = nullptr;
        }
        if (device)
        {
            tobii_gaze_point_unsubscribe(device);
            tobii_device_destroy(device);
            device = nullptr;
        }
    }
    catch (const std::exception&)
    {
    }
}
