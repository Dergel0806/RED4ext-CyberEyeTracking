#include <stdafx.hpp>
#include <set>
#include <map>

#include "BaseInkWidgetController.hpp"

#define OPACITY_HIDE 0.07

using ScriptableHandle = RED4ext::Handle<RED4ext::IScriptable>;

std::set<RED4ext::CClass*> g_inkWidgetControllersCls = std::set<RED4ext::CClass*>();
std::set<RED4ext::IScriptable*> g_scriptObjects = std::set<RED4ext::IScriptable*>();
std::map<RED4ext::CClass*, bool> g_singletonClasses;
std::map<RED4ext::CClass*, RED4ext::IScriptable*> g_signletoneScriptObjects;

std::mutex _syncMutex;

std::unordered_map<uint64_t, ScriptableHandle> s_objhs;

bool g_FindScriptObject(RED4ext::IScriptable* aScriptable)
{
    for (auto& x : g_scriptObjects)
    {
        if (x == aScriptable)
        {
            return true;
        }
    }
    return false;
}

bool g_FindInkWidgetControllersCls(RED4ext::CClass* aThis)
{
    for (auto& x : g_inkWidgetControllersCls)
    {
        if (aThis == x)
        {
            return true;
        }
    }
    return false;
}

typedef uint64_t (*HookFunction)(void*, RED4ext::IScriptable*);
typedef std::map<RED4ext::CClass*, HookFunction> HooksMap;
std::map<RED4ext::CClass*, uint64_t> WidgetControllersOrigInitRelAdr;
HookFunction g_defaultHook;

HooksMap WidgetControllersOrigInitHooks;

uint64_t WidgetControllerInitHook(void* aThis, RED4ext::IScriptable* aScriptable)
{
    auto cls = ((RED4ext::CClass*)aThis);
    
    HookFunction initOrig = nullptr;
    
    if (WidgetControllersOrigInitHooks.count(cls))
    {
        initOrig = WidgetControllersOrigInitHooks[cls];
        //spdlog::debug("init {}", cls->name.ToString());
    }

    if (initOrig == nullptr)
        initOrig = g_defaultHook;

    auto ret = initOrig(aThis, aScriptable);
    
   // _syncMutex.lock();
    if (!g_FindInkWidgetControllersCls(cls) || g_FindScriptObject(aScriptable))
    {
        //_syncMutex.unlock();
        return ret;
    }

    if (g_singletonClasses[cls])
    {
        g_signletoneScriptObjects[cls] = aScriptable;
    }
    else if (aScriptable)
    {
        g_scriptObjects.emplace(aScriptable);
    }
    else
    {
        spdlog::debug("hooked InitCls but aScriptable is null");
    }

   // _syncMutex.unlock();
    return ret;
}

HooksMap WidgetControllersOrigDestroyHooks{};

uint64_t WidgetControllerDestroyHook(void* aThis, RED4ext::IScriptable* aMemory)
{
    auto cls = ((RED4ext::CClass*)aThis);
    const char* name = cls->name.ToString();

   // _syncMutex.lock();

    if (g_singletonClasses[cls])
    {
        g_signletoneScriptObjects[cls] = nullptr;
        spdlog::debug("destroy {}", cls->name.ToString());
    }
    else if (g_FindScriptObject(aMemory))
    {
        g_scriptObjects.erase(aMemory);
    }
    
    //_syncMutex.unlock();

    HookFunction destroyOrig = nullptr;

    if (WidgetControllersOrigDestroyHooks.count(cls))
        destroyOrig = WidgetControllersOrigDestroyHooks[cls];
    
    if (destroyOrig == nullptr)
        destroyOrig = WidgetControllersOrigDestroyHooks.begin()->second;
    return destroyOrig(aThis, aMemory);
}

bool CyberEyeTracking::Workers::BaseInkWidgetController::vtblHooked = false;

CyberEyeTracking::Workers::BaseInkWidgetController::BaseInkWidgetController(const char* ctrlrRTTIname, bool singleton)
{
    _ctrlrRTTIname = RED4ext::CName(ctrlrRTTIname);
    _singleton = singleton;
}

void CyberEyeTracking::Workers::BaseInkWidgetController::Init()
{
    _rtti = RED4ext::CRTTISystem::Get();
    _inkWidgetControllerCls = _rtti->GetClass(_ctrlrRTTIname);
    _inkWidgetCls = _rtti->GetClass("inkWidget");
    _getMarginF = _inkWidgetCls->GetFunction("GetMargin");
    bool exists = false;
    for (auto& x : g_inkWidgetControllersCls)
    {
        if (_inkWidgetControllerCls == x)
        {
            exists = true;
            break;
        }
    }
    if (!exists)
        g_inkWidgetControllersCls.emplace(_inkWidgetControllerCls);

    auto clsName = _inkWidgetControllerCls->name.ToString();
    spdlog::debug("===============");
    spdlog::debug("name: {}", clsName);
    spdlog::debug("address: {:016X}", (uint64_t)_inkWidgetControllerCls);

    uint64_t* pvtbl = *(uint64_t**)_inkWidgetControllerCls;

    const uint64_t baseaddr = (uint64_t)GetModuleHandle(nullptr);
    auto vtblAdr = (uint64_t)pvtbl - baseaddr;
    spdlog::debug("baseaddr      : {:016X}", baseaddr);
    spdlog::debug("vtbladdr (rel): {:016X}", vtblAdr);

    g_singletonClasses[_inkWidgetControllerCls] = _singleton;
    
    for (auto it = WidgetControllersOrigInitRelAdr.begin(); it != WidgetControllersOrigInitRelAdr.end(); it++)
    {
        if (it->second == vtblAdr)
        {
            WidgetControllersOrigInitHooks[_inkWidgetControllerCls] = WidgetControllersOrigInitHooks[it->first];
            WidgetControllersOrigDestroyHooks[_inkWidgetControllerCls] = WidgetControllersOrigDestroyHooks[it->first];
            //spdlog::debug("already hooked for {}", it->first);
            return;
        }
    }
    WidgetControllersOrigInitHooks[_inkWidgetControllerCls] = (HookFunction)pvtbl[27];
    WidgetControllersOrigDestroyHooks[_inkWidgetControllerCls] = (HookFunction)pvtbl[28];
    WidgetControllersOrigInitRelAdr[_inkWidgetControllerCls] = vtblAdr;
    DWORD oldRw;
    if (g_defaultHook == nullptr)
        g_defaultHook = (HookFunction)pvtbl[27];

    if (VirtualProtect((void*)pvtbl, 0x100, PAGE_EXECUTE_READWRITE, &oldRw))
    {
        pvtbl[27] = (uint64_t)WidgetControllerInitHook;
        pvtbl[28] = (uint64_t)WidgetControllerDestroyHook;

        spdlog::debug("vtbl hooked !");

        VirtualProtect((void*)pvtbl, 0x100, oldRw, NULL);
    }
}

bool CyberEyeTracking::Workers::BaseInkWidgetController::CheckScriptObject(RED4ext::IScriptable* scriptObject)
{
    try
    {
        return scriptObject->unk18 >= 0 && scriptObject->unk18 < 4000000000000000000 && scriptObject->unk20 >= 0 &&
               scriptObject->unk28 >= 0 && scriptObject->valueHolder != nullptr &&
               scriptObject->ref.instance != nullptr && scriptObject->ref.GetUseCount() > 0 &&
               (uint64_t)scriptObject->ref.instance != 0x00000000ffffffff && scriptObject->GetNativeType() != nullptr &&
               _inkWidgetControllerCls->name == scriptObject->GetNativeType()->name;
    }
    catch (...)
    {
        spdlog::error("CheckScriptObject catch error");
        return false;
    }
}

bool CyberEyeTracking::Workers::BaseInkWidgetController::GetBoolPropertyValue(const char* propName)
{
    RED4ext::CProperty* prop = _inkWidgetControllerCls->GetProperty(propName);
    for (auto& so : GetScriptObjects())
    {
        return prop->GetValue<bool>(so);
    }
}

std::set<RED4ext::IScriptable*> CyberEyeTracking::Workers::BaseInkWidgetController::GetScriptObjects()
{
    std::set<RED4ext::IScriptable*> res;    
    if (g_singletonClasses[_inkWidgetControllerCls])
    {
        if (g_signletoneScriptObjects.count(_inkWidgetControllerCls) && CheckScriptObject(g_signletoneScriptObjects[_inkWidgetControllerCls]))
            res.emplace(g_signletoneScriptObjects[_inkWidgetControllerCls]);

        return res;
    }
    for (auto& so : g_scriptObjects)
    {
        if (CheckScriptObject(so))
        {
            res.emplace(so);
        }
    }
    return res;
}


void CyberEyeTracking::Workers::BaseInkWidgetController::SetRootOpacity(float value)
{
    for (auto& scriptable : GetScriptObjects())
    {
        if (*(uint64_t*)scriptable == 0)
        {
            spdlog::debug("{} null vtbl", scriptable->GetType()->name.ToString());
            continue;
        }

        if (scriptable->ref.instance && scriptable->ref.GetUseCount())
        {
            ScriptableHandle sh(scriptable); // auto-share_from_this
        }
        if (!scriptable)
            return;

        auto pWH = scriptable->ExecuteFunction<RED4ext::WeakHandle<RED4ext::IScriptable>>("GetRootWidget", nullptr);

        if (pWH)
        {
            auto inkWidget = pWH->Lock();
            if (inkWidget)
            {
                inkWidget->ExecuteFunction("SetOpacity", value);
            }
            else
            {
                spdlog::debug("couldn't lock root inkWidget whandle");
            }
        }
    }
}


void CyberEyeTracking::Workers::BaseInkWidgetController::HideWidget()
{
    SetRootOpacity(OPACITY_HIDE);
}

void CyberEyeTracking::Workers::BaseInkWidgetController::ShowWidget()
{
    SetRootOpacity(1);
}

bool CyberEyeTracking::Workers::BaseInkWidgetController::Exist()
{
    return GetScriptObjects().size() > 0;
}

void CyberEyeTracking::Workers::BaseInkWidgetController::Erase()
{
    if (g_signletoneScriptObjects.count(_inkWidgetControllerCls))
    {
        g_signletoneScriptObjects.erase(_inkWidgetControllerCls);
    }
}


int CyberEyeTracking::Workers::BaseInkWidgetController::ObjectsCount()
{
    return GetScriptObjects().size();
}
