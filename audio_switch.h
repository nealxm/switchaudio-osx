#pragma once

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wreserved-identifier"
#pragma clang diagnostic ignored "-Wundef"
#pragma clang diagnostic ignored "-Wfour-char-constants"
#pragma clang diagnostic ignored "-Wnullability-extension"
#pragma clang diagnostic ignored "-Wdocumentation-unknown-command"
#include <CoreAudio/AudioHardwareBase.h>
#include <MacTypes.h>
#pragma clang diagnostic pop

typedef enum {
    kAudioTypeUnknown = 0,
    kAudioTypeInput = 1,
    kAudioTypeOutput = 2,
    kAudioTypeSystemOutput = 3,
    kAudioTypeAll = 4
} ASDeviceType;

typedef enum {
    kFormatHuman = 0,
    kFormatCLI = 1,
    kFormatJSON = 2,
} ASOutputType;

typedef enum {
    kUnmute = 0,
    kMute = 1,
    kToggleMute = 2,
} ASMuteType;

enum {
    kFunctionSetDeviceByName = 1,
    kFunctionShowHelp = 2,
    kFunctionShowAll = 3,
    kFunctionShowCurrent = 4,
    kFunctionCycleNext = 5,
    kFunctionSetDeviceByID = 6,
    kFunctionSetDeviceByUID = 7,
    kFunctionMute = 8,
};

void showUsage(const char* appName);
int runAudioSwitch(int argc, const char* argv[]);
const char* getDeviceUID(AudioObjectID deviceID);
AudioObjectID getRequestedDeviceIDFromUIDSubstring(char* requestedDeviceUID, ASDeviceType typeRequested);
AudioObjectID getCurrentlySelectedDeviceID(ASDeviceType typeRequested);
void getDeviceName(AudioObjectID deviceID, char* deviceName);
ASDeviceType getDeviceType(AudioObjectID deviceID);
bool isAnInputDevice(AudioObjectID deviceID);
bool isAnOutputDevice(AudioObjectID deviceID);
char* deviceTypeName(ASDeviceType device_type);
void showCurrentlySelectedDeviceID(ASDeviceType typeRequested, ASOutputType outputRequested);
AudioObjectID getRequestedDeviceID(char* requestedDeviceName, ASDeviceType typeRequested);
AudioObjectID getNextDeviceID(AudioObjectID currentDeviceID, ASDeviceType typeRequested);
int setDevice(AudioObjectID newDeviceID, ASDeviceType typeRequested);
int setOneDevice(AudioObjectID newDeviceID, ASDeviceType typeRequested);
int setAllDevicesByName(char* requestedDeviceName);
int cycleNext(ASDeviceType typeRequested);
int cycleNextForOneDevice(ASDeviceType typeRequested);
OSStatus setMute(ASDeviceType typeRequested, ASMuteType mute);
void showAllDevices(ASDeviceType typeRequested, ASOutputType outputRequested);
