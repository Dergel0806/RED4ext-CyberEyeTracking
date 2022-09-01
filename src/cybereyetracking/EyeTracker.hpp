#pragma once
#include "..\..\deps\tobii\tobii.h"
#include "..\..\deps\tobii\tobii_streams.h"

namespace CyberEyeTracking
{
    class EyeTracker
    {
    public:
        EyeTracker() = default;
        ~EyeTracker();

        bool Init();
        float* GetPos();
        float* GetHeadPos();
        float* GetHeadRotation();
        void Finalize();
    };
}
