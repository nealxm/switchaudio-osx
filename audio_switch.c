#include "audio_switch.h"

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wreserved-identifier"
#pragma clang diagnostic ignored "-Wundef"
#pragma clang diagnostic ignored "-Wfour-char-constants"
#pragma clang diagnostic ignored "-Wnullability-extension"
#pragma clang diagnostic ignored "-Wdocumentation-unknown-command"
#include <CoreAudio/AudioHardware.h>
#include <CoreAudio/AudioHardwareBase.h>
#include <CoreAudioTypes/CoreAudioBaseTypes.h>
#include <CoreFoundation/CFBase.h>
#include <CoreFoundation/CFString.h>
#include <MacTypes.h>
#pragma clang diagnostic pop

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

void showUsage(const char* appName) {
    printf("Usage: %s [-a] [-c] [-t type] [-n] -s device_name | -i device_id | -u device_uid\n"
           "  -a             : shows all devices\n"
           "  -c             : shows current device\n\n"
           "  -f format      : output format (cli/human/json). Defaults to human.\n"
           "  -t type        : device type (input/output/system/all).  Defaults to output.\n"
           "  -m mute        : sets the mute status (mute/unmute/toggle).  For input/output only.\n"
           "  -n             : cycles the audio device to the next one\n"
           "  -i device_id   : sets the audio device to the given device by id\n"
           "  -u device_uid  : sets the audio device to the given device by uid or a substring of the uid\n"
           "  -s device_name : sets the audio device to the given device by name\n\n",
           appName);
}

int runAudioSwitch(int argc, const char* argv[]) {
    char requestedDeviceName[256];
    char printableDeviceName[256];
    int requestedDeviceID;
    char requestedDeviceUID[256];
    AudioObjectID chosenDeviceID = kAudioObjectUnknown;
    ASDeviceType typeRequested = kAudioTypeUnknown;
    ASOutputType outputRequested = kFormatHuman;
    ASMuteType muteRequested = kToggleMute;
    int function = 0;
    int result = 0;

    int c;
    while ((c = getopt(argc, (char**)argv, "hacm:nt:f:i:u:s:")) != -1) {
        switch (c) {
        case 'f':
            //format
            if (strcmp(optarg, "cli") == 0) {
                outputRequested = kFormatCLI;
            } else if (strcmp(optarg, "json") == 0) {
                outputRequested = kFormatJSON;
            } else if (strcmp(optarg, "human") == 0) {
                outputRequested = kFormatHuman;
            } else {
                printf("Unknown format %s\n", optarg);
                showUsage(argv[0]);
                return 1;
            }
            break;
        case 'a':
            //show all
            function = kFunctionShowAll;
            break;
        case 'c':
            //get current device
            function = kFunctionShowCurrent;
            break;

        case 'h':
            //show help
            function = kFunctionShowHelp;
            break;

        case 'm':
            //control the mute status of the interface selected with -t
            function = kFunctionMute;
            //set the mute mode
            if (strcmp(optarg, "mute") == 0) {
                muteRequested = kMute;
            } else if (strcmp(optarg, "unmute") == 0) {
                muteRequested = kUnmute;
            } else if (strcmp(optarg, "toggle") == 0) {
                muteRequested = kToggleMute;
            } else {
                printf("Invalid mute operation type \"%s\" specified.\n", optarg);
                showUsage(argv[0]);
                return 1;
            }
            break;

        case 'n':
            //cycle to the next audio device
            function = kFunctionCycleNext;
            break;

        case 'i':
            //set the requestedDeviceID
            function = kFunctionSetDeviceByID;
            requestedDeviceID = atoi(optarg);
            break;

        case 'u':
            //set the requestedDeviceUID
            function = kFunctionSetDeviceByUID;
            strcpy(requestedDeviceUID, optarg);
            break;

        case 's':
            //set the requestedDeviceName
            function = kFunctionSetDeviceByName;
            strcpy(requestedDeviceName, optarg);
            break;

        case 't':
            //set the requestedDeviceName
            if (strcmp(optarg, "input") == 0) {
                typeRequested = kAudioTypeInput;
            } else if (strcmp(optarg, "output") == 0) {
                typeRequested = kAudioTypeOutput;
            } else if (strcmp(optarg, "system") == 0) {
                typeRequested = kAudioTypeSystemOutput;
            } else if (strcmp(optarg, "all") == 0) {
                typeRequested = kAudioTypeAll;
            } else {
                printf("Invalid device type \"%s\" specified.\n", optarg);
                showUsage(argv[0]);
                return 1;
            }
            break;
        }
    }

    if (function == kFunctionShowAll) {
        switch (typeRequested) {
        case kAudioTypeInput:
        case kAudioTypeOutput:
            showAllDevices(typeRequested, outputRequested);
            break;
        case kAudioTypeSystemOutput:
            showAllDevices(kAudioTypeOutput, outputRequested);
            break;
        default:
            showAllDevices(kAudioTypeInput, outputRequested);
            showAllDevices(kAudioTypeOutput, outputRequested);
        }
        return 0;
    }
    if (function == kFunctionShowHelp) {
        showUsage(argv[0]);
        return 0;
    }
    if (function == kFunctionShowCurrent) {
        if (typeRequested == kAudioTypeUnknown) {
            typeRequested = kAudioTypeOutput;
        }
        showCurrentlySelectedDeviceID(typeRequested, outputRequested);
        return 0;
    }

    if (typeRequested == kAudioTypeUnknown) {
        typeRequested = kAudioTypeOutput;
    }

    if (function == kFunctionCycleNext) {
        result = cycleNext(typeRequested);
        return result;
    }

    if (function == kFunctionSetDeviceByID) {
        chosenDeviceID = (AudioObjectID)requestedDeviceID;
        sprintf(printableDeviceName, "Device with ID: %d", chosenDeviceID);
    }

    if (function == kFunctionSetDeviceByName && typeRequested != kAudioTypeAll) {
        //find the id of the requested device
        chosenDeviceID = getRequestedDeviceID(requestedDeviceName, typeRequested);
        if (chosenDeviceID == kAudioObjectUnknown) {
            printf("Could not find an audio device named \"%s\" of type %s.  Nothing was changed.\n", requestedDeviceName, deviceTypeName(typeRequested));
            return 1;
        }
        strcpy(printableDeviceName, requestedDeviceName);
    }

    if (function == kFunctionSetDeviceByUID) {
        //find the id of the requested device
        chosenDeviceID = getRequestedDeviceIDFromUIDSubstring(requestedDeviceUID, typeRequested);
        if (chosenDeviceID == kAudioObjectUnknown) {
            printf("Could not find an audio device with UID \"%s\" of type %s.  Nothing was changed.\n", requestedDeviceUID, deviceTypeName(typeRequested));
            return 1;
        }
        sprintf(printableDeviceName, "Device with UID: %s", getDeviceUID(chosenDeviceID));
    }

    if (function == kFunctionMute) {
        OSStatus status;
        bool anyStatusError = false;
        if (typeRequested == kAudioTypeUnknown)
            typeRequested = kAudioTypeInput;

        switch (typeRequested) {
        case kAudioTypeInput:
        case kAudioTypeOutput:
            status = setMute(typeRequested, muteRequested);
            if (status != noErr) {
                printf("Failed setting mute state. Error: %d", status);
                return 1;
            }
            break;
        case kAudioTypeAll:
            status = setMute(kAudioTypeInput, muteRequested);
            if (status != noErr) {
                printf("Failed setting mute state for input. Error: %d", status);
                anyStatusError = true;
            }
            status = setMute(kAudioTypeOutput, muteRequested);
            if (status != noErr) {
                printf("Failed setting mute state for output. Error: %d", status);
                anyStatusError = true;
            }
            if (anyStatusError) {
                return 1;
            }
            break;
        case kAudioTypeSystemOutput:
            printf("audio device \"%s\" may not be muted\n", deviceTypeName(typeRequested));
            return 1;
        }
        return 0;
    }

    if (typeRequested == kAudioTypeAll && function == kFunctionSetDeviceByName) {
        //special case for all - process each one separately
        result = setAllDevicesByName(requestedDeviceName);
    } else {
        //require a chose
        if (!chosenDeviceID) {
            printf("Please specify audio device.\n");
            showUsage(argv[0]);
            return 1;
        }

        //choose the requested audio device
        result = setDevice(chosenDeviceID, typeRequested);
        printf("%s audio device set to \"%s\"\n", deviceTypeName(typeRequested), printableDeviceName);
    }
    return result;
}

const char* getDeviceUID(AudioObjectID deviceID) {
    CFStringRef deviceUID = nullptr;
    UInt32 dataSize = sizeof(deviceUID);
    AudioObjectPropertyAddress propertyAddress = {
        kAudioHardwarePropertyDevices,
        kAudioObjectPropertyScopeGlobal,
        kAudioObjectPropertyElementMaster
    };

    propertyAddress.mSelector = kAudioDevicePropertyDeviceUID;

    OSStatus err = AudioObjectGetPropertyData(deviceID, &propertyAddress, 0, nullptr, &dataSize, &deviceUID);
    if (err != 0) {
        //Handle error
        return "";
    }

    const char* deviceUID_string = nullptr;
    CFIndex length = CFStringGetLength(deviceUID);
    CFIndex maxSize = CFStringGetMaximumSizeForEncoding(length, kCFStringEncodingUTF8) + 1;
    deviceUID_string = (char*)malloc(maxSize);

    if (deviceUID_string) {
        if (CFStringGetCString(deviceUID, deviceUID_string, maxSize, kCFStringEncodingUTF8)) {
            CFRelease(deviceUID);
            return deviceUID_string;
        }
        free((void*)deviceUID_string);
    }

    CFRelease(deviceUID);
    return "";
}

AudioObjectID getRequestedDeviceIDFromUIDSubstring(char* requestedDeviceUID, ASDeviceType typeRequested) {
    AudioObjectPropertyAddress propertyAddress = {kAudioHardwarePropertyDevices, kAudioObjectPropertyScopeGlobal, kAudioObjectPropertyElementMaster};
    UInt32 propertySize;
    AudioObjectID dev_array[64];
    int numberOfDevices = 0;

    AudioObjectGetPropertyDataSize(kAudioObjectSystemObject, &propertyAddress, 0, nullptr, &propertySize);
    AudioObjectGetPropertyData(kAudioObjectSystemObject, &propertyAddress, 0, nullptr, &propertySize, dev_array);
    numberOfDevices = (propertySize / sizeof(AudioObjectID));

    for (int i = 0; i < numberOfDevices; ++i) {
        switch (typeRequested) {
        case kAudioTypeInput:
            if (!isAnInputDevice(dev_array[i])) {
                continue;
            }
            break;
        case kAudioTypeOutput:
            if (!isAnOutputDevice(dev_array[i])) {
                continue;
            }
            break;
        case kAudioTypeSystemOutput:
            if (getDeviceType(dev_array[i]) != kAudioTypeOutput) {
                continue;
            }
            break;
        default:
            break;
        }

        char deviceUID[256];
        CFStringRef deviceUIDRef = nullptr;
        propertyAddress.mSelector = kAudioDevicePropertyDeviceUID;
        propertyAddress.mScope = kAudioObjectPropertyScopeGlobal;
        propertyAddress.mElement = dev_array[i];
        propertySize = sizeof(deviceUIDRef);
        AudioObjectGetPropertyData(dev_array[i], &propertyAddress, 0, nullptr, &propertySize, &deviceUIDRef);
        CFStringGetCString(deviceUIDRef, deviceUID, sizeof(deviceUID), CFStringGetSystemEncoding());
        if (strstr(deviceUID, requestedDeviceUID) != nullptr) {
            return dev_array[i];
        }
    }
    return kAudioObjectUnknown;
}

AudioObjectID getCurrentlySelectedDeviceID(ASDeviceType typeRequested) {
    AudioObjectPropertyAddress address;
    address.mSelector = kAudioHardwarePropertyDefaultOutputDevice;
    address.mScope = kAudioObjectPropertyScopeGlobal;
    address.mElement = kAudioObjectPropertyElementMaster;

    switch (typeRequested) {
    case kAudioTypeInput:
        address.mSelector = kAudioHardwarePropertyDefaultInputDevice;
        break;
    case kAudioTypeOutput:
        address.mSelector = kAudioHardwarePropertyDefaultOutputDevice;
        break;
    case kAudioTypeSystemOutput:
        address.mSelector = kAudioHardwarePropertyDefaultSystemOutputDevice;
        break;
    default:
        break;
    }

    AudioObjectID deviceID = kAudioObjectUnknown;
    UInt32 dataSize = sizeof(AudioObjectID);
    OSStatus status = AudioObjectGetPropertyData(kAudioObjectSystemObject, &address, 0, nullptr, &dataSize, &deviceID);
    if (status != noErr) {
        //handle error
    }

    return deviceID;
}

void getDeviceName(AudioObjectID deviceID, char* deviceName) {
    AudioObjectPropertyAddress address = {
        kAudioObjectPropertySelectorWildcard,
        //kAudioDevicePropertyDeviceNameCFString,
        kAudioObjectPropertyScopeGlobal,
        kAudioObjectPropertyElementMaster
    };
    CFStringRef cfDeviceName = nullptr;
    UInt32 dataSize = sizeof(CFStringRef);
    OSStatus result = AudioObjectGetPropertyData(deviceID, &address, 0, nullptr, &dataSize, &cfDeviceName);
    if (result == noErr && cfDeviceName != nullptr) {
        CFStringGetCString(cfDeviceName, deviceName, 256, kCFStringEncodingUTF8);
        CFRelease(cfDeviceName);
    }
}

//returns kAudioTypeInput or kAudioTypeOutput
ASDeviceType getDeviceType(AudioObjectID deviceID) {
    AudioObjectPropertyAddress address = {
        kAudioDevicePropertyStreams,
        kAudioObjectPropertyScopeGlobal,
        kAudioObjectPropertyElementMaster
    };
    UInt32 dataSize = 0;
    OSStatus result = AudioObjectGetPropertyDataSize(deviceID, &address, 0, nullptr, &dataSize);
    if (result == noErr && dataSize > 0) {
        return kAudioTypeOutput;
    }
    address.mElement = kAudioObjectPropertyElementMaster + 1;
    result = AudioObjectGetPropertyDataSize(deviceID, &address, 0, nullptr, &dataSize);
    if (result == noErr && dataSize > 0) {
        return kAudioTypeInput;
    }
    return kAudioTypeUnknown;
}

bool isAnOutputDevice(AudioObjectID deviceID) {
    AudioObjectPropertyAddress propertyAddress = {kAudioDevicePropertyStreams, kAudioDevicePropertyScopeOutput, kAudioObjectPropertyElementMaster};
    UInt32 dataSize = 0;
    OSStatus result = AudioObjectGetPropertyDataSize(deviceID, &propertyAddress, 0, nullptr, &dataSize);
    return (bool)(result == noErr && dataSize > 0);
}

bool isAnInputDevice(AudioObjectID deviceID) {
    AudioObjectPropertyAddress propertyAddress = {kAudioDevicePropertyStreams, kAudioDevicePropertyScopeInput, kAudioObjectPropertyElementMaster};
    UInt32 dataSize = 0;
    OSStatus result = AudioObjectGetPropertyDataSize(deviceID, &propertyAddress, sizeof(AudioClassDescription), nullptr, &dataSize);
    if (result == noErr && dataSize > 0) {
        return kAudioTypeInput;
    }
    return false;
}

char* deviceTypeName(ASDeviceType device_type) {
    switch (device_type) {
    case kAudioTypeInput:
        return "input";
    case kAudioTypeOutput:
        return "output";
    case kAudioTypeSystemOutput:
        return "system";
    case kAudioTypeAll:
        return "all";
    default:
        return "unknown";
    }
}

void showCurrentlySelectedDeviceID(ASDeviceType typeRequested, ASOutputType outputRequested) {
    AudioObjectID currentDeviceID = kAudioObjectUnknown;
    char currentDeviceName[256];

    currentDeviceID = getCurrentlySelectedDeviceID(typeRequested);
    getDeviceName(currentDeviceID, currentDeviceName);

    switch (outputRequested) {
    case kFormatHuman:
        printf("%s\n", currentDeviceName);
        break;
    case kFormatCLI:
        printf("%s,%s,%u,%s\n", currentDeviceName, deviceTypeName(typeRequested), currentDeviceID, getDeviceUID(currentDeviceID));
        break;
    case kFormatJSON:
        printf("{\"name\": \"%s\", \"type\": \"%s\", \"id\": \"%u\", \"uid\": \"%s\"}\n", currentDeviceName, deviceTypeName(typeRequested), currentDeviceID, getDeviceUID(currentDeviceID));
        break;
    default:
        break;
    }
}

AudioObjectID getRequestedDeviceID(char* requestedDeviceName, ASDeviceType typeRequested) {
    AudioObjectPropertyAddress propertyAddress;
    propertyAddress.mSelector = kAudioHardwarePropertyDevices;
    propertyAddress.mScope = kAudioObjectPropertyScopeGlobal;
    propertyAddress.mElement = kAudioObjectPropertyElementMaster;

    UInt32 propertySize;
    AudioObjectID dev_array[64];
    int numberOfDevices = 0;
    char deviceName[256];

    OSStatus status = AudioObjectGetPropertyDataSize(kAudioObjectSystemObject, &propertyAddress, 0, nullptr, &propertySize);
    if (status != noErr) {
        printf("Error getting size of property data: %d\n", status);
        return kAudioObjectUnknown;
    }

    status = AudioObjectGetPropertyData(kAudioObjectSystemObject, &propertyAddress, 0, nullptr, &propertySize, dev_array);
    if (status != noErr) {
        printf("Error getting property data: %d\n", status);
        return kAudioObjectUnknown;
    }

    numberOfDevices = (propertySize / sizeof(AudioObjectID));
    for (int i = 0; i < numberOfDevices; ++i) {
        switch (typeRequested) {
        case kAudioTypeInput:
            if (!isAnInputDevice(dev_array[i])) {
                continue;
            }
            break;

        case kAudioTypeOutput:
            if (!isAnOutputDevice(dev_array[i])) {
                continue;
            }
            break;

        case kAudioTypeSystemOutput:
            if (getDeviceType(dev_array[i]) != kAudioTypeOutput) {
                continue;
            }
            break;
        default:
            break;
        }

        getDeviceName(dev_array[i], deviceName);
        if (strcmp(requestedDeviceName, deviceName) == 0) {
            return dev_array[i];
        }
    }
    return kAudioObjectUnknown;
}

AudioObjectID getNextDeviceID(AudioObjectID currentDeviceID, ASDeviceType typeRequested) {
    AudioObjectPropertyAddress addr;
    AudioObjectID dev_array[64];
    int numberOfDevices = 0;
    AudioObjectID first_dev = kAudioObjectUnknown;
    int found = -1;

    addr.mSelector = kAudioHardwarePropertyDevices;
    addr.mScope = kAudioObjectPropertyScopeGlobal;
    addr.mElement = kAudioObjectPropertyElementMaster;
    UInt32 propertySize = 0;
    OSStatus err = AudioObjectGetPropertyDataSize(kAudioObjectSystemObject, &addr, 0, nullptr, &propertySize);
    if (err != noErr) {
        return kAudioObjectUnknown;
    }

    err = AudioObjectGetPropertyData(kAudioObjectSystemObject, &addr, 0, nullptr, &propertySize, dev_array);
    if (err != noErr) {
        return kAudioObjectUnknown;
    }
    numberOfDevices = (propertySize / sizeof(AudioObjectID));

    for (int i = 0; i < numberOfDevices; ++i) {
        switch (typeRequested) {
        case kAudioTypeInput:
            if (!isAnInputDevice(dev_array[i])) {
                continue;
            }
            break;

        case kAudioTypeOutput:
            if (!isAnOutputDevice(dev_array[i])) {
                continue;
            }
            break;

        case kAudioTypeSystemOutput:
            if (getDeviceType(dev_array[i]) != kAudioTypeOutput) {
                continue;
            }
            break;

        default:
            break;
        }

        if (first_dev == kAudioObjectUnknown) {
            first_dev = dev_array[i];
        }
        if (found >= 0) {
            return dev_array[i];
        }
        if (dev_array[i] == currentDeviceID) {
            found = i;
        }
    }
    return first_dev;
}

int setDevice(AudioObjectID newDeviceID, ASDeviceType typeRequested) {
    return setOneDevice(newDeviceID, typeRequested);
}

int setOneDevice(AudioObjectID newDeviceID, ASDeviceType typeRequested) {
    AudioObjectPropertyAddress addr;
    UInt32 propertySize = sizeof(UInt32);
    OSStatus status;

    addr.mScope = kAudioObjectPropertyScopeGlobal;
    addr.mElement = kAudioObjectPropertyElementMaster;

    switch (typeRequested) {
    case kAudioTypeInput:
        addr.mSelector = kAudioHardwarePropertyDefaultInputDevice;
        break;
    case kAudioTypeOutput:
        addr.mSelector = kAudioHardwarePropertyDefaultOutputDevice;
        break;
    case kAudioTypeSystemOutput:
        addr.mSelector = kAudioHardwarePropertyDefaultSystemOutputDevice;
        break;
    default:
        addr.mSelector = kAudioHardwarePropertyDefaultOutputDevice;
        break;
    }
    status = AudioObjectSetPropertyData(kAudioObjectSystemObject, &addr, 0, nullptr, propertySize, &newDeviceID);
    if (status != noErr) {
        printf("Failed to set %s", deviceTypeName(typeRequested));
    }

    return 0;
}

int setAllDevicesByName(char* requestedDeviceName) {
    int result;
    bool anyStatusError = false;
    AudioObjectID newDeviceID;

    //input
    newDeviceID = getRequestedDeviceID(requestedDeviceName, kAudioTypeInput);
    if (newDeviceID != nil) {
        result = setOneDevice(newDeviceID, kAudioTypeInput);
        if (result != 0) {
            anyStatusError = true;
        } else {
            printf("%s audio device set to \"%s\"\n", deviceTypeName(kAudioTypeInput), requestedDeviceName);
        }
    }

    //output
    newDeviceID = getRequestedDeviceID(requestedDeviceName, kAudioTypeOutput);
    if (newDeviceID != nil) {
        result = setOneDevice(newDeviceID, kAudioTypeOutput);
        if (result != 0) {
            anyStatusError = true;
        } else {
            printf("%s audio device set to \"%s\"\n", deviceTypeName(kAudioTypeOutput), requestedDeviceName);
        }
    }

    //system
    newDeviceID = getRequestedDeviceID(requestedDeviceName, kAudioTypeSystemOutput);
    if (newDeviceID != nil) {
        result = setOneDevice(newDeviceID, kAudioTypeSystemOutput);
        if (result != 0) {
            anyStatusError = true;
        } else {
            printf("%s audio device set to \"%s\"\n", deviceTypeName(kAudioTypeSystemOutput), requestedDeviceName);
        }
    }

    if (anyStatusError) {
        return 1;
    }

    return 0;
}

int cycleNext(ASDeviceType typeRequested) {
    int result;
    bool anyStatusError = false;
    if (typeRequested == kAudioTypeAll) {
        result = cycleNextForOneDevice(kAudioTypeInput);
        if (result != 0) {
            anyStatusError = true;
        }
        result = cycleNextForOneDevice(kAudioTypeOutput);
        if (result != 0) {
            anyStatusError = true;
        }
        result = cycleNextForOneDevice(kAudioTypeSystemOutput);
        if (result != 0) {
            anyStatusError = true;
        }

        if (anyStatusError) {
            return 1;
        }
        return 0;
    }
    return cycleNextForOneDevice(typeRequested);
}

int cycleNextForOneDevice(ASDeviceType typeRequested) {
    char requestedDeviceName[256];

    //get current device of requested type
    AudioObjectID chosenDeviceID = getCurrentlySelectedDeviceID(typeRequested);
    if (chosenDeviceID == kAudioObjectUnknown) {
        printf("Could not find current audio device of type %s.  Nothing was changed.\n", deviceTypeName(typeRequested));
        return 1;
    }

    //find next device to current device
    chosenDeviceID = getNextDeviceID(chosenDeviceID, typeRequested);
    if (chosenDeviceID == kAudioObjectUnknown) {
        printf("Could not find next audio device of type %s.  Nothing was changed.\n", deviceTypeName(typeRequested));
        return 1;
    }

    //choose the requested audio device
    int result = setDevice(chosenDeviceID, typeRequested);
    if (result == 0) {
        getDeviceName(chosenDeviceID, requestedDeviceName);
        printf("%s audio device set to \"%s\"\n", deviceTypeName(typeRequested), requestedDeviceName);
    }
    return result;
}

OSStatus setMute(ASDeviceType typeRequested, ASMuteType muteRequested) {
    AudioObjectID currentDeviceID = kAudioObjectUnknown;
    char currentDeviceName[256];

    currentDeviceID = getCurrentlySelectedDeviceID(typeRequested);
    getDeviceName(currentDeviceID, currentDeviceName);

    UInt32 scope = kAudioObjectPropertyScopeInput;

    switch (typeRequested) {
    case kAudioTypeInput:
        scope = kAudioObjectPropertyScopeInput;
        break;
    case kAudioTypeOutput:
        scope = kAudioObjectPropertyScopeOutput;
        break;
    case kAudioTypeSystemOutput:
        scope = kAudioObjectPropertyScopeGlobal;
        break;
    }

    AudioObjectPropertyAddress propertyAddress = {
        .mSelector = kAudioDevicePropertyMute,
        .mScope = scope,
        .mElement = kAudioObjectPropertyElementMain,
    };

    UInt32 muted = (UInt32)muteRequested;
    UInt32 propertySize = sizeof(muted);

    OSStatus status;
    if (muteRequested == kToggleMute) {
        UInt32 dataSize;
        status = AudioObjectGetPropertyDataSize(currentDeviceID, &propertyAddress, 0, nullptr, &dataSize);
        if (status != noErr) {
            return status;
        }
        status = AudioObjectGetPropertyData(currentDeviceID, &propertyAddress, 0, nullptr, &propertySize, &muted);
        if (status != noErr) {
            return status;
        }
        muted = !muted;
    }

    printf("Setting device %s to %s\n", currentDeviceName, muted ? "muted" : "unmuted");

    return AudioObjectSetPropertyData(currentDeviceID, &propertyAddress, 0, nullptr, propertySize, &muted);
}

void showAllDevices(ASDeviceType typeRequested, ASOutputType outputRequested) {
    UInt32 propertySize;
    AudioObjectPropertyAddress propertyAddress;
    AudioObjectID dev_array[64];
    int numberOfDevices = 0;
    ASDeviceType device_type;
    char deviceName[256];

    propertyAddress.mSelector = kAudioHardwarePropertyDevices;
    propertyAddress.mScope = kAudioObjectPropertyScopeGlobal;
    propertyAddress.mElement = kAudioObjectPropertyElementMaster;

    propertySize = 0;
    AudioObjectGetPropertyDataSize(kAudioObjectSystemObject, &propertyAddress, 0, nullptr, &propertySize);
    numberOfDevices = propertySize / sizeof(AudioObjectID);

    AudioObjectGetPropertyData(kAudioObjectSystemObject, &propertyAddress, 0, nullptr, &propertySize, dev_array);

    for (int i = 0; i < numberOfDevices; ++i) {
        switch (typeRequested) {
        case kAudioTypeInput:
            if (!isAnInputDevice(dev_array[i])) {
                continue;
            }
            device_type = kAudioTypeInput;
            break;
        case kAudioTypeOutput:
            if (!isAnOutputDevice(dev_array[i])) {
                continue;
            }
            device_type = kAudioTypeOutput;
            break;
        case kAudioTypeSystemOutput:
            device_type = getDeviceType(dev_array[i]);
            if (device_type != kAudioTypeOutput) {
                continue;
            }
            break;
        default:
            break;
        }

        getDeviceName(dev_array[i], deviceName);

        switch (outputRequested) {
        case kFormatHuman:
            printf("%s\n", deviceName);
            break;
        case kFormatCLI:
            printf("%s,%s,%u,%s\n", deviceName, deviceTypeName(device_type), dev_array[i], getDeviceUID(dev_array[i]));
            break;
        case kFormatJSON:
            printf("{\"name\": \"%s\", \"type\": \"%s\", \"id\": \"%u\", \"uid\": \"%s\"}\n", deviceName, deviceTypeName(device_type), dev_array[i], getDeviceUID(dev_array[i]));
            break;
        default:
            unreachable();
        }
    }
}
