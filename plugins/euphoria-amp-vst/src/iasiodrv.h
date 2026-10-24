#pragma once
#ifndef IASIODRV_H
#define IASIODRV_H

#include <windows.h>
#include <objbase.h>

#pragma pack(push, 4)

typedef struct ASIOSamples {
    unsigned long hi;
    unsigned long lo;
} ASIOSamples;

typedef struct ASIOTimeStamp {
    unsigned long hi;
    unsigned long lo;
} ASIOTimeStamp;

typedef double ASIOSampleRate;

typedef long ASIOBool;
enum {
    ASIOFalse = 0,
    ASIOTrue = 1
};

typedef long ASIOSampleType;
enum {
    ASIOSTInt16MSB   = 0,
    ASIOSTInt24MSB   = 1,
    ASIOSTInt32MSB   = 2,
    ASIOSTFloat32MSB = 3,
    ASIOSTFloat64MSB = 4,
    ASIOSTInt32MSB16 = 8,
    ASIOSTInt32MSB18 = 9,
    ASIOSTInt32MSB20 = 10,
    ASIOSTInt32MSB24 = 11,
    ASIOSTInt16LSB   = 16,
    ASIOSTInt24LSB   = 17,
    ASIOSTInt32LSB   = 18,
    ASIOSTFloat32LSB = 19,
    ASIOSTFloat64LSB = 20,
    ASIOSTInt32LSB16 = 24,
    ASIOSTInt32LSB18 = 25,
    ASIOSTInt32LSB20 = 26,
    ASIOSTInt32LSB24 = 27,
    ASIOSTDSDInt8LSB1 = 32,
    ASIOSTDSDInt8MSB1 = 33,
    ASIOSTDSDInt8NER8 = 40
};

typedef long ASIOError;
enum {
    ASE_OK = 0,
    ASE_SUCCESS = 0x3f4847a0,
    ASE_NotPresent = -1000,
    ASE_HWMalfunction,
    ASE_InvalidParameter,
    ASE_InvalidMode,
    ASE_SPNotAdvancing,
    ASE_NoClock,
    ASE_NoMemory
};

typedef struct ASIOTimeCode {
    double        speed;
    ASIOSamples   timeCodeSamples;
    unsigned long flags;
    char          future[64];
} ASIOTimeCode;

typedef enum ASIOTimeCodeFlags {
    kTcValid      = 1,
    kTcRunning    = 1 << 1,
    kTcReverse    = 1 << 2,
    kTcOnspeed    = 1 << 3,
    kTcStill      = 1 << 4,
    kTcSpeedValid = 1 << 8
} ASIOTimeCodeFlags;

typedef struct AsioTimeInfo {
    double          speed;
    ASIOTimeStamp   systemTime;
    ASIOSamples     samplePosition;
    ASIOSampleRate  sampleRate;
    unsigned long   flags;
    char            reserved[12];
} AsioTimeInfo;

typedef enum AsioTimeInfoFlags {
    kSystemTimeValid     = 1,
    kSamplePositionValid = 1 << 1,
    kSampleRateValid     = 1 << 2,
    kSpeedValid          = 1 << 3,
    kSampleRateChanged   = 1 << 4,
    kClockSourceChanged  = 1 << 5
} AsioTimeInfoFlags;

typedef struct ASIOTime {
    long                reserved[4];
    struct AsioTimeInfo timeInfo;
    struct ASIOTimeCode timeCode;
} ASIOTime;

typedef struct ASIOCallbacks {
    void (*bufferSwitch)(long doubleBufferIndex, ASIOBool directProcess);
    void (*sampleRateDidChange)(ASIOSampleRate sRate);
    long (*asioMessage)(long selector, long value, void* message, double* opt);
    ASIOTime* (*bufferSwitchTimeInfo)(ASIOTime* params, long doubleBufferIndex, ASIOBool directProcess);
} ASIOCallbacks;

enum {
    kAsioSelectorSupported    = 1,
    kAsioEngineVersion        = 2,
    kAsioResetRequest         = 3,
    kAsioBufferSizeChange     = 4,
    kAsioResyncRequest        = 5,
    kAsioLatenciesChanged     = 6,
    kAsioSupportsTimeInfo     = 7,
    kAsioSupportsTimeCode     = 8,
    kAsioMMCCommand           = 9,
    kAsioSupportsInputMonitor = 10,
    kAsioSupportsInputGain    = 11,
    kAsioSupportsInputMeter   = 12,
    kAsioSupportsOutputGain   = 13,
    kAsioSupportsOutputMeter  = 14,
    kAsioOverload             = 15,
    kNumAsioSelectors
};

typedef struct ASIOClockSource {
    long     index;
    long     associatedChannel;
    long     associatedGroup;
    ASIOBool isCurrentSource;
    char     name[32];
} ASIOClockSource;

typedef struct ASIOChannelInfo {
    long           channel;
    ASIOBool       isInput;
    ASIOBool       isActive;
    long           channelGroup;
    ASIOSampleType type;
    char           name[32];
} ASIOChannelInfo;

typedef struct ASIOBufferInfo {
    ASIOBool isInput;
    long     channelNum;
    void*    buffers[2];
} ASIOBufferInfo;

typedef struct ASIOInputMonitor {
    long     input;
    long     output;
    long     gain;
    ASIOBool state;
    long     pan;
} ASIOInputMonitor;

typedef struct ASIOChannelControls {
    long     channel;
    ASIOBool isInput;
    long     gain;
    long     meter;
    char     future[32];
} ASIOChannelControls;

typedef struct ASIOIoFormat {
    long formatType;
    char future[512 - sizeof(long)];
} ASIOIoFormat;

enum {
    kAsioEnableTimeCodeRead = 1,
    kAsioDisableTimeCodeRead,
    kAsioSetInputMonitor,
    kAsioTransport,
    kAsioSetInputGain,
    kAsioGetInputMeter,
    kAsioSetOutputGain,
    kAsioGetOutputMeter,
    kAsioCanInputMonitor,
    kAsioCanTimeInfo,
    kAsioCanTimeCode,
    kAsioCanTransport,
    kAsioCanInputGain,
    kAsioCanInputMeter,
    kAsioCanOutputGain,
    kAsioCanOutputMeter,
    kAsioOptionalOne,
    kAsioSetIoFormat          = 0x23111961,
    kAsioGetIoFormat          = 0x23111983,
    kAsioCanDoIoFormat        = 0x23112004,
    kAsioCanReportOverload    = 0x24042012,
    kAsioGetInternalBufferSamples = 0x25042012
};

#pragma pack(pop)

interface IASIO : public IUnknown {
    virtual ASIOBool init(void* sysHandle) = 0;
    virtual void getDriverName(char* name) = 0;
    virtual long getDriverVersion() = 0;
    virtual void getErrorMessage(char* string) = 0;
    virtual ASIOError start() = 0;
    virtual ASIOError stop() = 0;
    virtual ASIOError getChannels(long* numInputChannels, long* numOutputChannels) = 0;
    virtual ASIOError getLatencies(long* inputLatency, long* outputLatency) = 0;
    virtual ASIOError getBufferSize(long* minSize, long* maxSize, long* preferredSize, long* granularity) = 0;
    virtual ASIOError canSampleRate(ASIOSampleRate sRate) = 0;
    virtual ASIOError getSampleRate(ASIOSampleRate* currentRate) = 0;
    virtual ASIOError setSampleRate(ASIOSampleRate sampleRate) = 0;
    virtual ASIOError getClockSources(ASIOClockSource* clocks, long* numSources) = 0;
    virtual ASIOError setClockSource(long reference) = 0;
    virtual ASIOError getSamplePosition(ASIOSamples* sPos, ASIOTimeStamp* tStamp) = 0;
    virtual ASIOError getChannelInfo(ASIOChannelInfo* info) = 0;
    virtual ASIOError createBuffers(ASIOBufferInfo* bufferInfos, long numChannels, long bufferSize, ASIOCallbacks* callbacks) = 0;
    virtual ASIOError disposeBuffers() = 0;
    virtual ASIOError controlPanel() = 0;
    virtual ASIOError future(long selector, void* opt) = 0;
    virtual ASIOError outputReady() = 0;
};

#endif // IASIODRV_H
