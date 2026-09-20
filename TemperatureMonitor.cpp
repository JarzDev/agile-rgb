/*---------------------------------------------------------*\
| TemperatureMonitor.cpp                                    |
|                                                             |
|   Reads GPU temperature from vendor APIs (NVAPI for        |
|   NVIDIA, ADL for AMD) for use by temperature-reactive      |
|   lighting modes                                            |
|                                                             |
|   This file is part of the Agile Rgb project                |
|   SPDX-License-Identifier: GPL-2.0-or-later                 |
\*---------------------------------------------------------*/

#include "TemperatureMonitor.h"
#include "LogManager.h"

#include "nvapi.h"

#ifdef _WIN32
    #define _WIN32_LEAN_AND_MEAN
    #include <windows.h>
    #include "adl_sdk.h"
    #include "adl_defines.h"
    #include "adl_structures.h"
#endif

#include <cstring>

/*-----------------------------------------------------------*\
| NVAPI                                                        |
\*-----------------------------------------------------------*/
static const int NVAPI_MAX_PHYSICAL_GPUS = 64;

/*-----------------------------------------------------------*\
| ADL (AMD) - dynamically loaded, same pattern as              |
| i2c_smbus/Windows/i2c_smbus_amdadl.cpp                       |
\*-----------------------------------------------------------*/
#ifdef _WIN32
typedef int ( *ADL2_MAIN_CONTROL_CREATE )(ADL_MAIN_MALLOC_CALLBACK, int, ADL_CONTEXT_HANDLE*);
typedef int ( *ADL2_MAIN_CONTROL_DESTROY )(ADL_CONTEXT_HANDLE);
typedef int ( *ADL2_ADAPTER_NUMBEROFADAPTERS_GET ) ( ADL_CONTEXT_HANDLE, int* );
typedef int ( *ADL2_ADAPTER_ADAPTERINFOX2_GET ) ( ADL_CONTEXT_HANDLE, AdapterInfo** );
typedef int ( *ADL2_ADAPTER_ACTIVE_GET ) ( ADL_CONTEXT_HANDLE, int, int* );
typedef int ( *ADL2_OVERDRIVE5_TEMPERATURE_GET ) ( ADL_CONTEXT_HANDLE, int, int, ADLTemperature* );

static ADL2_MAIN_CONTROL_CREATE            ADL2_Main_Control_Create           = nullptr;
static ADL2_MAIN_CONTROL_DESTROY           ADL2_Main_Control_Destroy          = nullptr;
static ADL2_ADAPTER_NUMBEROFADAPTERS_GET   ADL2_Adapter_NumberOfAdapters_Get  = nullptr;
static ADL2_ADAPTER_ADAPTERINFOX2_GET      ADL2_Adapter_AdapterInfoX2_Get     = nullptr;
static ADL2_ADAPTER_ACTIVE_GET             ADL2_Adapter_Active_Get            = nullptr;
static ADL2_OVERDRIVE5_TEMPERATURE_GET     ADL2_Overdrive5_Temperature_Get    = nullptr;

static void* __stdcall ADL_Main_Memory_Alloc(int size)
{
    return malloc(size);
}

static void __stdcall ADL_Main_Memory_Free(void* buffer)
{
    if(buffer != nullptr)
    {
        free(buffer);
    }
}

static bool LoadADLLibrary()
{
    HINSTANCE hDLL = LoadLibraryA("atiadlxx.dll");

    if(hDLL == NULL)
    {
        hDLL = LoadLibraryA("atiadlxy.dll");
    }

    if(hDLL == NULL)
    {
        return false;
    }

    ADL2_Main_Control_Create           = (ADL2_MAIN_CONTROL_CREATE)          GetProcAddress(hDLL, "ADL2_Main_Control_Create");
    ADL2_Main_Control_Destroy          = (ADL2_MAIN_CONTROL_DESTROY)         GetProcAddress(hDLL, "ADL2_Main_Control_Destroy");
    ADL2_Adapter_NumberOfAdapters_Get  = (ADL2_ADAPTER_NUMBEROFADAPTERS_GET) GetProcAddress(hDLL, "ADL2_Adapter_NumberOfAdapters_Get");
    ADL2_Adapter_AdapterInfoX2_Get     = (ADL2_ADAPTER_ADAPTERINFOX2_GET)    GetProcAddress(hDLL, "ADL2_Adapter_AdapterInfoX2_Get");
    ADL2_Adapter_Active_Get            = (ADL2_ADAPTER_ACTIVE_GET)           GetProcAddress(hDLL, "ADL2_Adapter_Active_Get");
    ADL2_Overdrive5_Temperature_Get    = (ADL2_OVERDRIVE5_TEMPERATURE_GET)   GetProcAddress(hDLL, "ADL2_Overdrive5_Temperature_Get");

    return(ADL2_Main_Control_Create
        && ADL2_Main_Control_Destroy
        && ADL2_Adapter_NumberOfAdapters_Get
        && ADL2_Adapter_AdapterInfoX2_Get
        && ADL2_Adapter_Active_Get
        && ADL2_Overdrive5_Temperature_Get);
}
#endif

TemperatureMonitor* TemperatureMonitor::instance = nullptr;

TemperatureMonitor::TemperatureMonitor()
{
    nvapi_available = false;
    adl_available    = false;

    DetectGPUs();
}

TemperatureMonitor::~TemperatureMonitor()
{
}

TemperatureMonitor* TemperatureMonitor::get()
{
    if(!instance)
    {
        instance = new TemperatureMonitor();
    }

    return instance;
}

void TemperatureMonitor::InitNVAPI()
{
    nvapi_available = (NvAPI_Initialize() == 0);

    if(nvapi_available)
    {
        LOG_INFO("[TemperatureMonitor] NVAPI initialized successfully");
    }
}

void TemperatureMonitor::InitADL()
{
#ifdef _WIN32
    if(!LoadADLLibrary())
    {
        adl_available = false;
        return;
    }

    ADL_CONTEXT_HANDLE context = nullptr;

    if(ADL2_Main_Control_Create(ADL_Main_Memory_Alloc, 1, &context) != ADL_OK)
    {
        adl_available = false;
        return;
    }

    ADL2_Main_Control_Destroy(context);

    adl_available = true;
    LOG_INFO("[TemperatureMonitor] ADL initialized successfully");
#else
    adl_available = false;
#endif
}

void TemperatureMonitor::DetectGPUs()
{
    InitNVAPI();
    InitADL();
}

std::vector<GPUTemperatureReading> TemperatureMonitor::ReadNVIDIATemperatures()
{
    std::vector<GPUTemperatureReading> readings;

    if(!nvapi_available)
    {
        return readings;
    }

    NV_PHYSICAL_GPU_HANDLE handles[NVAPI_MAX_PHYSICAL_GPUS];
    NV_S32 gpu_count = 0;

    if(NvAPI_EnumPhysicalGPUs(handles, &gpu_count) != 0)
    {
        return readings;
    }

    for(NV_S32 i = 0; i < gpu_count; i++)
    {
        GPUTemperatureReading reading;
        reading.vendor              = GPUVendor::NVIDIA;
        reading.temperature_celsius = -1;
        reading.valid                = false;

        NV_SHORT_STRING name = { 0 };
        if(NvAPI_GPU_GetFullName(handles[i], name) == 0)
        {
            reading.name = name;
        }
        else
        {
            reading.name = "NVIDIA GPU";
        }

        NV_GPU_THERMAL_SETTINGS_V2 thermal;

        if(NvAPI_GPU_GetThermalSettings(handles[i], NV_THERMAL_TARGET::ALL, &thermal) == 0 && thermal.count > 0)
        {
            reading.temperature_celsius = thermal.sensor[0].current_temperature;
            reading.valid               = true;
        }

        readings.push_back(reading);
    }

    return readings;
}

std::vector<GPUTemperatureReading> TemperatureMonitor::ReadAMDTemperatures()
{
    std::vector<GPUTemperatureReading> readings;

#ifdef _WIN32
    if(!adl_available)
    {
        return readings;
    }

    ADL_CONTEXT_HANDLE context = nullptr;

    if(ADL2_Main_Control_Create(ADL_Main_Memory_Alloc, 1, &context) != ADL_OK)
    {
        return readings;
    }

    int num_adapters = 0;

    if(ADL2_Adapter_NumberOfAdapters_Get(context, &num_adapters) != ADL_OK || num_adapters <= 0)
    {
        ADL2_Main_Control_Destroy(context);
        return readings;
    }

    AdapterInfo* adapter_info = nullptr;

    if(ADL2_Adapter_AdapterInfoX2_Get(context, &adapter_info) != ADL_OK || adapter_info == nullptr)
    {
        ADL2_Main_Control_Destroy(context);
        return readings;
    }

    for(int i = 0; i < num_adapters; i++)
    {
        int adapter_active = 0;
        ADL2_Adapter_Active_Get(context, adapter_info[i].iAdapterIndex, &adapter_active);

        if(!adapter_active)
        {
            continue;
        }

        ADLTemperature temp;
        temp.iSize = sizeof(ADLTemperature);

        if(ADL2_Overdrive5_Temperature_Get(context, adapter_info[i].iAdapterIndex, 0, &temp) == ADL_OK)
        {
            GPUTemperatureReading reading;
            reading.vendor              = GPUVendor::AMD;
            reading.name                 = adapter_info[i].strAdapterName;
            reading.temperature_celsius = temp.iTemperature / 1000;
            reading.valid                = true;

            readings.push_back(reading);
        }
    }

    ADL_Main_Memory_Free(adapter_info);
    ADL2_Main_Control_Destroy(context);
#endif

    return readings;
}

std::vector<GPUTemperatureReading> TemperatureMonitor::GetGPUTemperatures()
{
    std::vector<GPUTemperatureReading> readings = ReadNVIDIATemperatures();

    std::vector<GPUTemperatureReading> amd_readings = ReadAMDTemperatures();
    readings.insert(readings.end(), amd_readings.begin(), amd_readings.end());

    return readings;
}

int TemperatureMonitor::GetHighestGPUTemperature()
{
    std::vector<GPUTemperatureReading> readings = GetGPUTemperatures();

    int highest = -1;

    for(const GPUTemperatureReading& reading : readings)
    {
        if(reading.valid && reading.temperature_celsius > highest)
        {
            highest = reading.temperature_celsius;
        }
    }

    return highest;
}
