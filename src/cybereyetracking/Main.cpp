#include <stdafx.hpp>

#include <chrono>
#include <set>
#include <tchar.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>

#include <Workers/BaseInkWidgetController.hpp>
#include <Workers/RadialWheelWorker.hpp>
#include <Workers/CameraPitchWorker.hpp>
#include <Workers/DialogWorker.hpp>
#include <Workers/HUDManagerWorker.hpp>

#include <EyeTracker.hpp>
#include "Utils.hpp"

#define CAMERA_PITCH_LOOK_START 0.5 // 0.338 Screen border where we start pitching camera

#define CAMERA_PITCH_PARABOLA_A 1 // 1
#define CAMERA_PITCH_PARABOLA_B -1 // -1
#define CAMERA_PITCH_PARABOLA_C 0.25 // 0.25

#define CAMERA_PITCH_RESET_START 0.25 // 0.25

#define RED4EXT_EXPORT extern "C" __declspec(dllexport)

CyberEyeTracking::Workers::BaseInkWidgetController _healthBarWorker = CyberEyeTracking::Workers::BaseInkWidgetController("healthbarWidgetGameController");
CyberEyeTracking::Workers::BaseInkWidgetController _minimapWorker = CyberEyeTracking::Workers::BaseInkWidgetController("gameuiMinimapContainerController");
CyberEyeTracking::Workers::BaseInkWidgetController _wantedBarWorker = CyberEyeTracking::Workers::BaseInkWidgetController("WantedBarGameController");
CyberEyeTracking::Workers::BaseInkWidgetController _questTrackerWidgetWorker = CyberEyeTracking::Workers::BaseInkWidgetController("QuestTrackerGameController");
CyberEyeTracking::Workers::BaseInkWidgetController _hotkeysWidgetWorker = CyberEyeTracking::Workers::BaseInkWidgetController("HotkeysWidgetController");
CyberEyeTracking::Workers::BaseInkWidgetController _lootingWorker = CyberEyeTracking::Workers::BaseInkWidgetController("LootingController");
CyberEyeTracking::Workers::BaseInkWidgetController _crosshairWorker = CyberEyeTracking::Workers::BaseInkWidgetController("CrosshairGameController_Simple");
CyberEyeTracking::Workers::BaseInkWidgetController _crosshairWorker2 = CyberEyeTracking::Workers::BaseInkWidgetController("CrosshairGameController_Basic");
CyberEyeTracking::Workers::BaseInkWidgetController _crosshairWorker3 = CyberEyeTracking::Workers::BaseInkWidgetController("CrosshairGameController_Smart_Rifl");
CyberEyeTracking::Workers::BaseInkWidgetController _crosshairWorker4 = CyberEyeTracking::Workers::BaseInkWidgetController("gameuiCrosshairContainerController");

CyberEyeTracking::Workers::RadialWheelWorker _radialWheelWorker =  CyberEyeTracking::Workers::RadialWheelWorker();
CyberEyeTracking::Workers::CameraPitchWorker _cameraPitchWorker = CyberEyeTracking::Workers::CameraPitchWorker();
CyberEyeTracking::Workers::DialogWorker _dialogWorker = CyberEyeTracking::Workers::DialogWorker();
CyberEyeTracking::Workers::HUDManagerWorker _hudManagerWorker = CyberEyeTracking::Workers::HUDManagerWorker();

CyberEyeTracking::EyeTracker _eyeTracker;

RED4ext::WeakHandle<RED4ext::IScriptable> sysHandlers;
RED4ext::CClass* scriptGameInstanceCls;
RED4ext::CClass* inkMenuScenarioCls;

bool _disableWheelSelect = false;
bool _disableCleanUI = false;
bool _disableDialogueSelect = false;
bool _disableCameraPitch = false;

std::string rootPath;
std::chrono::seconds loadCheckSec(15);

void InitializeLogger(std::filesystem::path aRoot)
{
    auto console = std::make_shared<spdlog::sinks::stdout_color_sink_st>();
    auto file = std::make_shared<spdlog::sinks::basic_file_sink_st>(aRoot / L"cybereyetracking.log", true);

    spdlog::sinks_init_list sinks = { console, file };

    auto logger = std::make_shared<spdlog::logger>("", sinks);
    spdlog::set_default_logger(logger);

#ifdef _DEBUG
    logger->flush_on(spdlog::level::trace);
    spdlog::set_level(spdlog::level::trace);
#else
    logger->flush_on(spdlog::level::info);
    spdlog::set_level(spdlog::level::info);
#endif
}


bool RED4EXT_CALL Load()
{
    char buff[MAX_PATH];
    GetModuleFileNameA(NULL, buff, MAX_PATH);

    rootPath = std::string(buff);
    size_t pos = std::string::npos;
    std::string toErase = "Cyberpunk2077.exe";
    // Search for the substring in string in a loop untill nothing is found
    while ((pos = rootPath.find(toErase)) != std::string::npos)
    {
        // If found then erase it from string
        rootPath.erase(pos, toErase.length());
    }
    InitializeLogger(rootPath);
    
    _eyeTracker = CyberEyeTracking::EyeTracker();
    return true;
}

void RED4EXT_CALL PostLoad()
{
    TCHAR iniVal[4];
    std::string iniPathStr = rootPath + "cybereyetracking.ini";
    std::wstring iniPath = std::wstring(iniPathStr.begin(), iniPathStr.end());
    GetPrivateProfileString(L"features", L"DisableWheelSelect", L"0", iniVal, 4, iniPath.c_str());
    _disableWheelSelect = iniVal[0] == '1';

    GetPrivateProfileString(L"features", L"DisableCleanUI", L"0", iniVal, 4, iniPath.c_str());
    _disableCleanUI = iniVal[0] == '1';

    GetPrivateProfileString(L"features", L"DisableDialogueSelect", L"0", iniVal, 4, iniPath.c_str());
    _disableDialogueSelect = iniVal[0] == '1';

    GetPrivateProfileString(L"features", L"DisableCameraPitch", L"0", iniVal, 4, iniPath.c_str());
    _disableCameraPitch = iniVal[0] == '1';

    GetPrivateProfileString(L"features", L"LoadCheckSec", L"15", iniVal, 4, iniPath.c_str());
    int loadCheckSecVal = _ttoi(iniVal);
    loadCheckSec = std::chrono::seconds(loadCheckSecVal);
    spdlog::info("Looking for a connected eye tracking device");
}


void RED4EXT_CALL Unload()
{
    _eyeTracker.Finalize();
}

//float GetCamPitch(float pos, bool negative)
//{
//    return negative ?
//        -CyberEyeTracking::Math::GetParametrizedParabola(CAMERA_PITCH_PARABOLA_A, CAMERA_PITCH_PARABOLA_B, CAMERA_PITCH_PARABOLA_C, 1 - pos) :
//        CyberEyeTracking::Math::GetParametrizedParabola(CAMERA_PITCH_PARABOLA_A, CAMERA_PITCH_PARABOLA_B,CAMERA_PITCH_PARABOLA_C, pos);
//}

float GetCamPitch(float x)
{
    return 2 * pow(x, 3) + x;
}

//static bool resetPitch = false;
static float prevX = 0;
static float prevY = 0;

//void StartResetPitch(float x, float y)
//{
//    if (resetPitch)
//        return;
//
//    resetPitch = true;
//    prevX = x;
//    prevY = y;
//}

bool Update(RED4ext::CGameApplication* aApp)
{
    //static bool game_loaded = false;
    static auto timeStart = std::chrono::high_resolution_clock::now();
    static auto loadCheck = std::chrono::high_resolution_clock::now();
    static bool initialized = false;
    static bool hudManagerInitialized = false;
    static bool trackerFound = false;

    static float previous_camera_X = 0;
    static float previous_camera_Y = 0;

    auto now = std::chrono::high_resolution_clock::now();
    auto static gameInstance = RED4ext::CGameEngine::Get()->framework->gameInstance;
    using namespace std::chrono_literals;
    auto rtti = RED4ext::CRTTISystem::Get();
    spdlog::info("==============");
    try
    {
        if (!initialized && (now - loadCheck) < loadCheckSec)
        {
            spdlog::info("!initialized && (now - loadCheck) < loadCheckSec");
            return false;
        }

        if (!trackerFound)
        {
            bool initRes = _eyeTracker.Init();
            if (initRes)
                spdlog::info("Eye tracker found!");

            trackerFound = true;
            return false;
        }

        if (!initialized)
        {
            spdlog::info("Stuff initialized!");
            if (!_disableCleanUI)
            {
                _healthBarWorker.Init();
                _minimapWorker.Init();
                _wantedBarWorker.Init();
                _questTrackerWidgetWorker.Init();
                _hotkeysWidgetWorker.Init();
                _crosshairWorker.Init();
                _crosshairWorker2.Init();
                _crosshairWorker3.Init();
                _crosshairWorker4.Init();
            }
            if (!_disableWheelSelect)
            {
                _radialWheelWorker.Init();
            }
            if (!_disableCameraPitch)
            {
                _cameraPitchWorker.Init(gameInstance);
                _lootingWorker.Init();
            }

            _dialogWorker.Init();

            inkMenuScenarioCls = rtti->GetClass("inkMenuScenario");
            scriptGameInstanceCls = rtti->GetClass("ScriptGameInstance");

            initialized = true;
        }
        // spdlog::info(_crosshairWorker.ObjectsCount());
        // if (_crosshairWorker.ObjectsCount() > 0)
        //{
        //    game_loaded = true;
        //}

        // if (!game_loaded)
        //{
        //    return false;
        //}

        // RED4ext::ExecuteFunction(gameInstance, inkMenuScenarioCls->GetFunction("GetSystemRequestsHandler"),
        // &sysHandlers, {});

        // auto instance = sysHandlers.Lock();
        ////spdlog::info(instance);
        ////spdlog::info(_dialogWorker.ObjectsCount());
        ////if (!instance || _dialogWorker.ObjectsCount() == 0)
        ////{
        ////    spdlog::info("!instance || _dialogWorker.ObjectsCount() == 0");
        ////    loadCheck = std::chrono::high_resolution_clock::now();
        ////    _dialogWorker.Erase();
        ////    hudManagerInitialized = false;
        ////    return false;
        ////}

        // auto gamePaused = instance->ExecuteFunction<bool>("IsGamePaused", nullptr);
        // if (!gamePaused.has_value() || gamePaused.value())
        //{
        //    spdlog::info("!gamePaused.has_value() || gamePaused.value()");
        //    return false;
        //}

        float* pos = _eyeTracker.GetPos();
        float x = pos[0];
        float y = pos[1];

        float* head_pos = _eyeTracker.GetHeadPos();

        float* head_rotation = _eyeTracker.GetHeadRotation();

        if (x > 1)
            x = 1;
        else if (x < 0)
            x = 0;
        if (y > 1)
            y = 1;
        else if (y < 0)
            y = 0;
        spdlog::info("Gaze:");
        spdlog::info(x);
        spdlog::info(y);

        spdlog::info("Head pos:");
        spdlog::info(head_pos[0]);
        spdlog::info(head_pos[1]);

        spdlog::info("Head rotation:");
        spdlog::info(head_rotation[0]);
        spdlog::info(head_rotation[1]);
        spdlog::info(head_rotation[2]);

        //// Dont work if some camera is controlled
        // RED4ext::Handle<RED4ext::IScriptable> container;
        // std::vector<RED4ext::CStackType> args;
        // spdlog::info("args.emplace_back(nullptr, &gameInstance);");
        // args.emplace_back(nullptr, &gameInstance);

        // auto f = rtti->GetClass("ScriptGameInstance")->GetFunction("GetScriptableSystemsContainer");
        // RED4ext::ExecuteFunction(gameInstance, f, &container, args);
        // spdlog::info("GetScriptableSystemsContainer");
        // if (!container || !container.instance)
        //{
        //    spdlog::info("!container || !container.instance");
        //    return false;
        //}

        // args.clear();
        // RED4ext::Handle<RED4ext::IScriptable> takeOverControlSystem;
        // auto name = RED4ext::CName::CName("TakeOverControlSystem");
        // spdlog::info("TakeOverControlSystem");
        // args.emplace_back(nullptr, &name);
        // f = rtti->GetClass("gameScriptableSystemsContainer")->GetFunction("Get");

        // RED4ext::ExecuteFunction(container, f, &takeOverControlSystem, args);
        // auto isDeviceControlled = takeOverControlSystem->ExecuteFunction<bool>("IsDeviceControlled");
        // spdlog::info(isDeviceControlled.value());
        // if (isDeviceControlled.value())
        //    return false;

        spdlog::info("Got to selection!");
        // ================ WHEEL SELECT ==============
        if (!_disableWheelSelect && _radialWheelWorker.ObjectsCount() > 0)
        {
            float angle = CyberEyeTracking::Math::GetAngle(x, y);
            if (_radialWheelWorker.SetAngle(angle))
                return false;
        }

        // ================ DIALOGUE SELECT ==============
        if (!_disableDialogueSelect && _dialogWorker.SelectAtPos(y))
            return false;

        // ================ CLEAN UI ==============
        if (!_disableCleanUI)
        {
            spdlog::info("Clean UI");
            if (x >= 0 && x <= 0.25 && //(0-480)
                y >= 0 && y <= 0.165)  // (0-110)
            {
                _healthBarWorker.ShowWidget();
                //StartResetPitch(x, y);
                spdlog::info("Health Bar showing");
            }
            else
            {
                _healthBarWorker.HideWidget();
                spdlog::info("Health Bar hidden");
            }

            if (x >= 0.848958333 && x <= 0.971875 // (1630-1866)
                && y >= 0.037037 && y <= 0.3055)  // (41-330)
            {
                _minimapWorker.ShowWidget();
                //StartResetPitch(x, y);
                spdlog::info("Map showing");
            }
            else
            {
                _minimapWorker.HideWidget();
                spdlog::info("Map hidden");
            }

            if (x >= 0.8208333 && x <= 0.848958333 // (1575-1630)
                && y >= 0.055555555 && y <= 0.25)  // (60-270)
            {
                _wantedBarWorker.ShowWidget();
            }
            else
            {
                _wantedBarWorker.HideWidget();
            }

            if (x >= 0.786458333 && x <= 0.9442708333333333 // (1510-1813)
                && y >= 0.35185185 && y <= 0.5)             // (380-540)
            {
                _questTrackerWidgetWorker.ShowWidget();
            }
            else
            {
                _questTrackerWidgetWorker.HideWidget();
            }

            if (x >= 0.03125 && x <= 0.161458333 // (60-310)
                && y >= 0.8703703 && y <= 1)     // (940-1080)
            {
                _hotkeysWidgetWorker.ShowWidget();
                //StartResetPitch(x, y);
            }
            else
            {
                _hotkeysWidgetWorker.HideWidget();
            }

            if (x <= 0.4 || x >= 0.6 || y <= 0.4 || y >= 0.6)
            {
                _crosshairWorker.HideWidget();
                _crosshairWorker2.HideWidget();
                _crosshairWorker3.HideWidget();
                _crosshairWorker4.HideWidget();
            }
            else
            {
                _crosshairWorker.ShowWidget();
                _crosshairWorker2.ShowWidget();
                _crosshairWorker3.ShowWidget();
                _crosshairWorker4.ShowWidget();
            }
        }

        // ================ CAMERA PITCH ==============

        if (!_disableCameraPitch)
        {
            /*if (!hudManagerInitialized)
            {
                _hudManagerWorker.Init();
                hudManagerInitialized = true;
            }*/
            //if (/*_lootingWorker.GetBoolPropertyValue("isShown") || */ _hudManagerWorker.IsScanning() ||
            //    _hudManagerWorker.IsHacking())
            //{
            //    spdlog::info(_lootingWorker.GetBoolPropertyValue("isShown"));
            //    spdlog::info(_hudManagerWorker.IsScanning());
            //    spdlog::info(_hudManagerWorker.IsHacking());
            //    _cameraPitchWorker.SetPitch(0, 0);
            //    spdlog::info("Line 382");
            //    return false;
            //}

            // bool pitchLeft = x <= CAMERA_PITCH_LOOK_START;
            // bool pitchRight = x >= 1 - CAMERA_PITCH_LOOK_START;
            // bool pitchUp = y <= CAMERA_PITCH_LOOK_START;
            // bool pitchDown = y >= 1 - CAMERA_PITCH_LOOK_START;

            /*float pitchX = 0;
            float pitchY = 0;*/

            /*if (resetPitch)
            {
                if (x > CAMERA_PITCH_RESET_START && x < 1 - CAMERA_PITCH_RESET_START && y > CAMERA_PITCH_RESET_START &&
                    y < 1 - CAMERA_PITCH_RESET_START)
                {
                    resetPitch = false;
                }
                else
                {
                    x = prevX;
                    y = prevY;
                }
            }*/

             //pitchX = GetCamPitch(x, pitchRight);
            // pitchY = GetCamPitch(y, pitchDown);
            float head_rotation_X = head_rotation[1];
            float head_rotation_Y = head_rotation[0];

            // smoothing N1, avoid jumping of the camera
            head_rotation_X = round(head_rotation_X * 100.0) / 100.0;
            head_rotation_Y = round(head_rotation_Y * 100.0) / 100.0;

            float pitchX = 10 * pow(head_rotation_X, 3) + 5 * head_rotation_X;
            float pitchY = 10 * pow(head_rotation_Y, 3) + 5 * head_rotation_Y;
            /*pitchX = GetCamPitch(head_rotation[1]);
            pitchY = GetCamPitch(head_rotation[0]);*/
            spdlog::info(pitchX);
            spdlog::info(pitchY);
            

            if (pitchX < -3)
            {
                pitchX = -3;
            }
            if (pitchX > 3)
            {
                pitchX = 3;
            }
            if (pitchY < -3)
            {
                pitchY = -3;
            }
            if (pitchY > 3)
            {
                pitchY = 3;
            }

            // deadzone in the center
            if (pitchY < 0.1 && pitchY > -0.1)
            {
                pitchY = 0;
            }
            if (pitchX < 0.1 && pitchX > -0.1)
            {
                pitchX = 0;
            }
            
            // smoothing N2, avoid jumping of the camera
            if (abs(previous_camera_X - pitchX) < 0.015)
            {
                pitchX = previous_camera_X;
            }
            if (abs(previous_camera_Y - pitchY) < 0.015)
            {
                pitchY = previous_camera_Y;
            }

            //_cameraPitchWorker.SetPitch(pitchX, pitchY);
            _cameraPitchWorker.SetPitch(pitchX, pitchY);
            /*previous_camera_X = pitchX;
            previous_camera_Y = pitchY;*/
        }

        // ================ LOOK AT LOOT ==============
        /* RED4ext::Handle<RED4ext::IScriptable> targetSystem;
        std::vector<RED4ext::CStackType> args;
        args.emplace_back(nullptr, &gameInstance);

        auto f = rtti->GetClass("ScriptGameInstance")->GetFunction("GetTargetingSystem");
        RED4ext::ExecuteFunction(gameInstance, f, &targetSystem, args);

        if (!targetSystem || !targetSystem.instance)
            return;

        RED4ext::Handle<RED4ext::IScriptable> playerH;
        RED4ext::ExecuteGlobalFunction("GetPlayerObject;GameInstance", &playerH, gameInstance);
        RED4ext::WeakHandle<RED4ext::IScriptable> playerWH{};
        playerWH = playerH;

        auto anglesCls = rtti->GetClass("EulerAngles");
        auto fRand = anglesCls->GetFunction("Rand");
        auto angles = anglesCls->AllocInstance();

        anglesCls->GetProperty("Pitch")->SetValue<float>(angles, 0);
        anglesCls->GetProperty("Roll")->SetValue<float>(angles, 0);
        anglesCls->GetProperty("Yaw")->SetValue<float>(angles, 45 * x);

        auto gtsCls = rtti->GetClass("gametargetingTargetingSystem");
        auto fGetObjClosest = gtsCls->GetFunction("GetObjectClosestToCrosshair");

        args.clear();
        args.emplace_back(nullptr, &playerWH);
        args.emplace_back(nullptr, &angles);
        args.emplace_back(nullptr, query);
        RED4ext::Handle<RED4ext::IScriptable> gameObj;
        RED4ext::ExecuteFunction(targetSystem.instance, fGetObjClosest, &gameObj, args);

        if (!gameObj || !gameObj.instance)
            return;

        RED4ext::CName className;
        auto gameObjCls = rtti->GetClass("gameObject");
        auto getNameFunc = gameObjCls->GetFunction("GetClassName");
        RED4ext::ExecuteFunction(gameObj.instance, getNameFunc, &className, {});

        spdlog::debug(className.ToString());*/
    }
    catch (const std::exception& e)
    {
        spdlog::info(e.what());
    }
    return false;
}

RED4EXT_C_EXPORT void RED4EXT_CALL Query(RED4ext::PluginInfo* aInfo)
{
    /*
     * Runtime version is the game's version, it is best to let it set to "RED4EXT_RUNTIME_LATEST" if you want to target
     * the latest game's version that the SDK defined, if the runtime version specified here and the game's version do
     * not match, your plugin will not be loaded. If you want to use RED4ext only as a loader and you do not care about
     * game's version use "RED4EXT_RUNTIME_INDEPENDENT".
     */

    aInfo->name = L"CyberEyeTracking";
    aInfo->author = L"Jay-D & Dergel";
    aInfo->version = RED4EXT_SEMVER(1, 1, 0);
    aInfo->runtime = RED4EXT_RUNTIME_LATEST;
    aInfo->sdk = RED4EXT_SDK_LATEST;
}

RED4EXT_C_EXPORT uint32_t RED4EXT_CALL Supports()
{
    /*
     * This functions returns only what API version is support by your plugins.
     */
    return RED4EXT_API_VERSION_LATEST;
}


bool BaseInit_OnEnter(RED4ext::CGameApplication* aApp)
{
    return true;
}


bool BaseInit_OnExit(RED4ext::CGameApplication* aApp)
{
    return true;
}


RED4EXT_C_EXPORT bool RED4EXT_CALL Main(RED4ext::PluginHandle aHandle, RED4ext::EMainReason aReason,
                                        const RED4ext::Sdk* aSdk)
{
    RED4EXT_UNUSED_PARAMETER(aHandle);
    RED4EXT_UNUSED_PARAMETER(aSdk);

    switch (aReason)
    {
    case RED4ext::EMainReason::Load:
    {
        Load();
        PostLoad();
        RED4ext::GameState initState;
        initState.OnEnter = &BaseInit_OnEnter;
        initState.OnUpdate = &Update;
        initState.OnExit = &BaseInit_OnExit;
        aSdk->gameStates->Add(aHandle, RED4ext::EGameStateType::Running, &initState);
        break;
    }
    case RED4ext::EMainReason::Unload:
    {
        Unload();
        break;
    }
    }

    return true;
}
