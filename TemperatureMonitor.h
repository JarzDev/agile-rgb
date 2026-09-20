/*---------------------------------------------------------*\
| TemperatureMonitor.h                                      |
|                                                             |
|   Reads GPU temperature from vendor APIs (NVAPI for        |
|   NVIDIA, ADL for AMD) for use by temperature-reactive      |
|   lighting modes                                            |
|                                                             |
|   This file is part of the Agile Rgb project                |
|   SPDX-License-Identifier: GPL-2.0-or-later                 |
\*---------------------------------------------------------*/

#pragma once

#include <vector>
#include <string>

enum class GPUVendor
{
    NONE,
    NVIDIA,
    AMD
};

typedef struct
{
    GPUVendor   vendor;
    std::string name;
    int         temperature_celsius;
    bool        valid;
} GPUTemperatureReading;

class TemperatureMonitor
{
public:
    TemperatureMonitor();
    ~TemperatureMonitor();

    /*-----------------------------------------------------*\
    | TemperatureMonitor Global Instance Accessor            |
    \*-----------------------------------------------------*/
    static TemperatureMonitor*             get();

    /*-----------------------------------------------------*\
    | Detect available GPUs (NVIDIA via NVAPI, AMD via ADL)  |
    | Safe to call multiple times; re-scans each time        |
    \*-----------------------------------------------------*/
    void                                    DetectGPUs();

    /*-----------------------------------------------------*\
    | Returns the temperature readings for all GPUs found    |
    | by the last DetectGPUs() call                          |
    \*-----------------------------------------------------*/
    std::vector<GPUTemperatureReading>      GetGPUTemperatures();

    /*-----------------------------------------------------*\
    | Convenience accessor: highest temperature across all   |
    | detected GPUs, in degrees Celsius. Returns -1 if no     |
    | GPU temperature could be read                          |
    \*-----------------------------------------------------*/
    int                                     GetHighestGPUTemperature();

private:
    static TemperatureMonitor*             instance;

    bool                                    nvapi_available;
    bool                                    adl_available;

    void                                    InitNVAPI();
    void                                    InitADL();

    std::vector<GPUTemperatureReading>      ReadNVIDIATemperatures();
    std::vector<GPUTemperatureReading>      ReadAMDTemperatures();
};
