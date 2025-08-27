#include "switchaudio.h"

#include <assert.h>

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

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

OSStatus run_cli(int argc, char** argv) {
    cli_opts opts = {};
    int8_t c;

    while ((c = (int8_t)getopt(argc, argv, "acnm:t:f:i:u:s:h")) != -1) {
        switch (c) {
        case 'a': {
            opts.show_all = true;
            break;
        }
        case 'c': {
            opts.show_curr = true;
            break;
        }
        case 'n': {
            opts.next_dev = true;
            break;
        }
        case 'm': {
            opts.mute_dev = true;

            if (strcmp(optarg, "mute") == 0) {
                opts.mute_kind = mute;
            } else if (strcmp(optarg, "unmute") == 0) {
                opts.mute_kind = unmute;
            } else if (strcmp(optarg, "toggle") == 0) {
                opts.mute_kind = toggle;
            } else {
                printf("invalid mute operation type '%s'\n", optarg);
                return kAudio_ParamError;
            }
            break;
        }
        case 'i': {
            opts.set_dev = true;
            opts.has_id = true;
            opts.dev_id = (AudioObjectID)atoi(optarg);
            break;
        }
        case 'u': {
            opts.set_dev = true;
            opts.has_uid = true;
            strncpy(opts.dev_uid, optarg, NAME_BUF);
            break;
        }
        case 's': {
            opts.set_dev = true;
            opts.has_name = true;
            strncpy(opts.dev_name, optarg, NAME_BUF);
            break;
        }
        case 't': {
            if (strcmp(optarg, "input") == 0) {
                opts.dev_kind = input;
            } else if (strcmp(optarg, "output") == 0) {
                opts.dev_kind = output;
            } else if (strcmp(optarg, "all") == 0) {
                opts.dev_kind = all;
            } else {
                printf("invalid device type '%s'\n", optarg);
                return kAudio_ParamError;
            }
            break;
        }
        case 'f': {
            if (strcmp(optarg, "cli") == 0) {
                opts.out_kind = cli_format;
            } else if (strcmp(optarg, "json") == 0) {
                opts.out_kind = json_format;
            } else if (strcmp(optarg, "human") == 0) {
                opts.out_kind = human_format;
            } else {
                printf("invalid outout format '%s'\n", optarg);
                return kAudio_ParamError;
            }
            break;
        }
        default:
        case 'h':
            show_usage(argv[0]);
            return noErr;
        }
    }
    return process_args(opts);
}

OSStatus process_args(cli_opts opts) {
    //if () {
    //    fprintf(stderr, "must specify exactly one operation\n");
    //    return kAudio_ParamError;
    //}

    assert(opts.show_all + opts.show_curr + opts.next_dev + opts.set_dev + opts.mute_dev == 1);
    assert(opts.has_id + opts.has_uid + opts.has_name <= 1);
    OSStatus status = noErr;

    if (opts.show_all) {
        return show_devs(opts.dev_kind, opts.out_kind);
    }
    if (opts.show_curr) {
        return show_dev_curr(opts.dev_kind, opts.out_kind);
    }
    if (opts.next_dev) {
        return cycleNext(opts.dev_kind);
    }
    if (opts.set_dev) { //need to handle error reporting for edge cases better
        if (opts.has_name && opts.dev_kind == all) {
            return setAllDevicesByName(opts.dev_name);
        } else if (opts.has_name && opts.dev_kind != all) {
            opts.dev_id = getRequestedDeviceID(opts.dev_name, opts.dev_kind);
            if (opts.dev_id == kAudioObjectUnknown) {
                printf("Could not find an audio device named \"%s\" of type %s.  Nothing was changed.\n", opts.dev_name, deviceTypeName(opts.dev_kind));
                return 1;
            }
        } else if (opts.has_uid) {
            opts.dev_id = getRequestedDeviceIDFromUIDSubstring(opts.dev_uid, opts.dev_kind);
            if (opts.dev_id == kAudioObjectUnknown) {
                printf("Could not find an audio device with UID \"%s\" of type %s.  Nothing was changed.\n", opts.dev_uid, deviceTypeName(opts.dev_kind));
                return 1;
            }
        }
        return setOneDevice(opts.dev_id, opts.dev_kind);
    }
    if (opts.mute_dev) {
        switch (opts.dev_kind) {
        case input:
        case output:
            status = setMute(opts.dev_kind, opts.mute_kind);
            if (status != noErr) {
                printf("Failed setting mute state. Error: %d", status);
                return status;
            }
            break;
        case unspecified:
        case all:
            status = setMute(input, opts.mute_kind);
            if (status != noErr) {
                printf("failed setting mute state for input");
                return status;
            }
            status = setMute(output, opts.mute_kind);
            if (status != noErr) {
                printf("failed setting mute state for output");
                return status;
            }
            break;
        default:
            unreachable();
        }
        return status;
    }
    return status;
}

OSStatus show_devs(device_type dev_kind, output_type out_kind) {
    AudioObjectPropertyAddress addr = {
        .mSelector = kAudioHardwarePropertyDevices,
        .mElement = kAudioObjectPropertyElementMain
    };
    switch (dev_kind) {
    case input:
        addr.mScope = kAudioObjectPropertyScopeInput;
        break;

    case output:
        addr.mScope = kAudioObjectPropertyScopeOutput;
        break;

    case unspecified:
    case all:
        addr.mScope = kAudioObjectPropertyScopeGlobal;
        break;
    default:
        unreachable();
    }
    AudioObjectID devs[DEV_ARR];
    char dev_name[NAME_BUF];

    uint32_t propertySize = 0;
    OSStatus status = AudioObjectGetPropertyDataSize(
        kAudioObjectSystemObject, &addr, 0,
        nullptr, &propertySize
    );
    if (status != noErr) {
        fprintf(stderr, "failed to query property data size");
        return status;
    }
    status = AudioObjectGetPropertyData(
        kAudioObjectSystemObject, &addr, 0,
        nullptr, &propertySize, devs
    );
    if (status != noErr) {
        fprintf(stderr, "failed to query property data");
        return status;
    }

    for (uint8_t i = 0; i < propertySize / sizeof(AudioObjectID); ++i) {
        getDeviceName(devs[i], dev_name);
        device_type device_type = getDeviceType(devs[i]);

        switch (out_kind) {
        case human_format:
            printf("%s\n", dev_name);
            break;
        case cli_format:
            printf("%s,%s,%u,%s\n", dev_name, deviceTypeName(device_type), devs[i], getDeviceUID(devs[i]));
            break;
        case json_format:
            printf("{\"name\": \"%s\", \"type\": \"%s\", \"id\": \"%u\", \"uid\": \"%s\"}\n", dev_name, deviceTypeName(device_type), devs[i], getDeviceUID(devs[i]));
            break;
        default:
            unreachable();
        }
    }
    return noErr;
}

const char* getDeviceUID(AudioObjectID deviceID) {
    CFStringRef deviceUID = nullptr;
    uint32_t dataSize = sizeof(CFStringRef);
    AudioObjectPropertyAddress addr = {
        kAudioHardwarePropertyDevices,
        kAudioDevicePropertyDeviceUID,
        kAudioObjectPropertyElementMain
    };

    OSStatus status = AudioObjectGetPropertyData(
        deviceID, &addr, 0,
        nullptr, &dataSize, (void*)&deviceUID
    );
    if (status != noErr) {
        fprintf(stderr, "failed to query property data");
    }

    char* deviceUID_string = nullptr;
    CFIndex length = CFStringGetLength(deviceUID);
    CFIndex maxSize = CFStringGetMaximumSizeForEncoding(length, kCFStringEncodingUTF8) + 1;
    deviceUID_string = (char*)malloc((size_t)maxSize);

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

AudioObjectID getRequestedDeviceIDFromUIDSubstring(char* requestedDeviceUID, device_type typeRequested) {
    AudioObjectPropertyAddress propertyAddress = {
        kAudioHardwarePropertyDevices,
        kAudioObjectPropertyScopeGlobal,
        kAudioObjectPropertyElementMain
    };
    uint32_t propertySize;
    AudioObjectID devices[DEV_ARR];
    uint8_t numberOfDevices = 0;

    AudioObjectGetPropertyDataSize(kAudioObjectSystemObject, &propertyAddress, 0, nullptr, &propertySize);
    AudioObjectGetPropertyData(kAudioObjectSystemObject, &propertyAddress, 0, nullptr, &propertySize, devices);
    numberOfDevices = (uint8_t)(propertySize / sizeof(AudioObjectID));

    for (int i = 0; i < numberOfDevices; ++i) {
        switch (typeRequested) {
        case input:
            if (!isAnInputDevice(devices[i])) {
                continue;
            }
            break;
        case output:
            if (!isAnOutputDevice(devices[i])) {
                continue;
            }
            break;
        case all:
        case unspecified:
        default:
        }

        char deviceUID[UID_BUF];
        CFStringRef deviceUIDRef = nullptr;
        propertyAddress.mSelector = kAudioDevicePropertyDeviceUID;
        propertyAddress.mScope = kAudioObjectPropertyScopeGlobal;
        propertyAddress.mElement = devices[i];
        propertySize = sizeof(CFStringRef);
        AudioObjectGetPropertyData(devices[i], &propertyAddress, 0, nullptr, &propertySize, &deviceUIDRef);
        CFStringGetCString(deviceUIDRef, deviceUID, sizeof(deviceUID), CFStringGetSystemEncoding());
        if (strstr(deviceUID, requestedDeviceUID) != nullptr) {
            return devices[i];
        }
    }
    return kAudioObjectUnknown;
}

AudioObjectID getCurrentlySelectedDeviceID(device_type typeRequested) {
    AudioObjectPropertyAddress addr = {
        .mSelector = kAudioHardwarePropertyDefaultOutputDevice,
        .mScope = kAudioObjectPropertyScopeGlobal,
        .mElement = kAudioObjectPropertyElementMain
    };

    switch (typeRequested) {
    case input:
        addr.mSelector = kAudioHardwarePropertyDefaultInputDevice;
        break;
    case output:
        addr.mSelector = kAudioHardwarePropertyDefaultOutputDevice;
        break;
    default:
    case unspecified:
    case all:
        break;
    }

    AudioObjectID deviceID = kAudioObjectUnknown;
    uint32_t dataSize = sizeof(AudioObjectID);
    OSStatus status = AudioObjectGetPropertyData(kAudioObjectSystemObject, &addr, 0, nullptr, &dataSize, &deviceID);
    if (status != noErr) {
        fprintf(stderr, "failed to query property data");
    }
    return deviceID;
}

void getDeviceName(AudioObjectID deviceID, char* deviceName) {
    AudioObjectPropertyAddress addr = {
        kAudioObjectPropertyName,
        kAudioObjectPropertyScopeGlobal,
        kAudioObjectPropertyElementMain
    };
    CFStringRef cfDeviceName = nullptr;
    uint32_t dataSize = sizeof(CFStringRef);
    OSStatus result = AudioObjectGetPropertyData(
        deviceID, &addr,
        0, nullptr,
        &dataSize, &cfDeviceName
    );
    if (result == noErr && cfDeviceName != nullptr) {
        CFStringGetCString(cfDeviceName, deviceName, NAME_BUF, kCFStringEncodingUTF8);
        CFRelease(cfDeviceName);
    }
}

//returns kAudioTypeInput or kAudioTypeOutput
device_type getDeviceType(AudioObjectID deviceID) {
    AudioObjectPropertyAddress address = {
        kAudioDevicePropertyStreams,
        kAudioObjectPropertyScopeGlobal,
        kAudioObjectPropertyElementMain
    };
    uint32_t dataSize = 0;
    OSStatus result = AudioObjectGetPropertyDataSize(deviceID, &address, 0, nullptr, &dataSize);
    if (result == noErr && dataSize > 0) {
        return output;
    }
    address.mElement = kAudioObjectPropertyElementMain + 1;
    result = AudioObjectGetPropertyDataSize(deviceID, &address, 0, nullptr, &dataSize);
    if (result == noErr && dataSize > 0) {
        return input;
    }
    return all;
}

bool isAnOutputDevice(AudioObjectID deviceID) {
    AudioObjectPropertyAddress propertyAddress = {kAudioDevicePropertyStreams, kAudioObjectPropertyScopeOutput, kAudioObjectPropertyElementMain};
    uint32_t dataSize = 0;
    OSStatus result = AudioObjectGetPropertyDataSize(deviceID, &propertyAddress, 0, nullptr, &dataSize);
    return (bool)(result == noErr && dataSize > 0);
}

bool isAnInputDevice(AudioObjectID deviceID) {
    AudioObjectPropertyAddress propertyAddress = {kAudioDevicePropertyStreams, kAudioObjectPropertyScopeInput, kAudioObjectPropertyElementMain};
    uint32_t dataSize = 0;
    OSStatus result = AudioObjectGetPropertyDataSize(deviceID, &propertyAddress, sizeof(AudioClassDescription), nullptr, &dataSize);
    return (bool)(result == noErr && dataSize > 0);
}

char* deviceTypeName(device_type device_type) {
    switch (device_type) {
    case input:
        return "input";
    case output:
        return "output";
    case all:
        return "all";
    case unspecified:
    default:
        return "unknown";
    }
}

OSStatus show_dev_curr(device_type typeRequested, output_type outputRequested) {
    AudioObjectID currentDeviceID = kAudioObjectUnknown;
    char currentDeviceName[NAME_BUF];

    currentDeviceID = getCurrentlySelectedDeviceID(typeRequested);
    getDeviceName(currentDeviceID, currentDeviceName);

    switch (outputRequested) {
    case human_format:
        printf("%s\n", currentDeviceName);
        break;
    case cli_format:
        printf("%s,%s,%u,%s\n", currentDeviceName, deviceTypeName(typeRequested), currentDeviceID, getDeviceUID(currentDeviceID));
        break;
    case json_format:
        printf("{\"name\": \"%s\", \"type\": \"%s\", \"id\": \"%u\", \"uid\": \"%s\"}\n", currentDeviceName, deviceTypeName(typeRequested), currentDeviceID, getDeviceUID(currentDeviceID));
        break;
    default:
        break;
    }
    return kAudio_NoError;
}

AudioObjectID getRequestedDeviceID(char* requestedDeviceName, device_type typeRequested) {
    AudioObjectPropertyAddress addr = {
        .mSelector = kAudioHardwarePropertyDevices,
        .mScope = kAudioObjectPropertyScopeGlobal,
        .mElement = kAudioObjectPropertyElementMain
    };

    uint32_t propertySize;
    AudioObjectID devices[DEV_ARR];
    uint8_t numberOfDevices = 0;
    char deviceName[NAME_BUF];

    OSStatus status = AudioObjectGetPropertyDataSize(kAudioObjectSystemObject, &addr, 0, nullptr, &propertySize);
    if (status != noErr) {
        printf("Error getting size of property data: %d\n", status);
        return kAudioObjectUnknown;
    }

    status = AudioObjectGetPropertyData(kAudioObjectSystemObject, &addr, 0, nullptr, &propertySize, devices);
    if (status != noErr) {
        printf("Error getting property data: %d\n", status);
        return kAudioObjectUnknown;
    }

    numberOfDevices = (uint8_t)(propertySize / sizeof(AudioObjectID));
    for (int i = 0; i < numberOfDevices; ++i) {
        switch (typeRequested) {
        case input:
            if (!isAnInputDevice(devices[i])) {
                continue;
            }
            break;
        case output:
            if (!isAnOutputDevice(devices[i])) {
                continue;
            }
            break;
        case all:
        case unspecified:
        default:
            break;
        }

        getDeviceName(devices[i], deviceName);
        if (strcmp(requestedDeviceName, deviceName) == 0) {
            return devices[i];
        }
    }
    return kAudioObjectUnknown;
}

AudioObjectID getNextDeviceID(AudioObjectID currentDeviceID, device_type typeRequested) {
    AudioObjectID devices[DEV_ARR];
    AudioObjectID first_dev = kAudioObjectUnknown;
    int found = -1;

    AudioObjectPropertyAddress addr = {
        .mSelector = kAudioHardwarePropertyDevices,
        .mScope = kAudioObjectPropertyScopeGlobal,
        .mElement = kAudioObjectPropertyElementMain
    };
    uint32_t propertySize = 0;
    OSStatus err = AudioObjectGetPropertyDataSize(
        kAudioObjectSystemObject, &addr, 0,
        nullptr, &propertySize
    );
    if (err != noErr) {
        return kAudioObjectUnknown;
    }

    err = AudioObjectGetPropertyData(
        kAudioObjectSystemObject, &addr, 0,
        nullptr, &propertySize, devices
    );
    if (err != noErr) {
        return kAudioObjectUnknown;
    }
    uint8_t numberOfDevices = (uint8_t)(propertySize / sizeof(AudioObjectID));

    for (int i = 0; i < numberOfDevices; ++i) {
        switch (typeRequested) {
        case input:
            if (!isAnInputDevice(devices[i])) {
                continue;
            }
            break;
        case output:
            if (!isAnOutputDevice(devices[i])) {
                continue;
            }
            break;
        case all:
        case unspecified:
        default:
            break;
        }

        if (first_dev == kAudioObjectUnknown) {
            first_dev = devices[i];
        }
        if (found >= 0) {
            return devices[i];
        }
        if (devices[i] == currentDeviceID) {
            found = i;
        }
    }
    return first_dev;
}

int setOneDevice(AudioObjectID new_id, device_type dev_kind) {
    AudioObjectPropertyAddress addr = {
        .mScope = kAudioObjectPropertyScopeGlobal,
        .mElement = kAudioObjectPropertyElementMain
    };

    switch (dev_kind) {
    case input:
        addr.mSelector = kAudioHardwarePropertyDefaultInputDevice;
        break;
    case output:
        addr.mSelector = kAudioHardwarePropertyDefaultOutputDevice;
        break;
    case all:
    case unspecified:
    default:
    }
    OSStatus status = AudioObjectSetPropertyData(
        kAudioObjectSystemObject, &addr,
        0, nullptr,
        sizeof(uint32_t), &new_id
    );
    if (status != noErr) {
        printf("Failed to set %s", deviceTypeName(dev_kind));
        return 1;
    }
    return 0;
}

int setAllDevicesByName(char* requestedDeviceName) {
    int result;
    bool anyStatusError = false;
    AudioObjectID newDeviceID;

    //input
    newDeviceID = getRequestedDeviceID(requestedDeviceName, input);
    if (newDeviceID != nil) {
        result = setOneDevice(newDeviceID, input);
        if (result != 0) {
            anyStatusError = true;
        } else {
            printf("%s audio device set to \"%s\"\n", deviceTypeName(input), requestedDeviceName);
        }
    }

    //output
    newDeviceID = getRequestedDeviceID(requestedDeviceName, output);
    if (newDeviceID != nil) {
        result = setOneDevice(newDeviceID, output);
        if (result != 0) {
            anyStatusError = true;
        } else {
            printf("%s audio device set to \"%s\"\n", deviceTypeName(output), requestedDeviceName);
        }
    }
    return anyStatusError;
}

int cycleNext(device_type typeRequested) {
    int result;
    bool anyStatusError = false;
    if (typeRequested == all) {
        result = cycleNextForOneDevice(input);
        if (result != 0) {
            anyStatusError = true;
        }
        result = cycleNextForOneDevice(output);
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

int cycleNextForOneDevice(device_type typeRequested) {
    char requestedDeviceName[NAME_BUF];

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
    int result = setOneDevice(chosenDeviceID, typeRequested);
    if (result == 0) {
        getDeviceName(chosenDeviceID, requestedDeviceName);
        printf("%s audio device set to \"%s\"\n", deviceTypeName(typeRequested), requestedDeviceName);
    }
    return result;
}

OSStatus setMute(device_type typeRequested, mute_type muteRequested) {
    AudioObjectID currentDeviceID = kAudioObjectUnknown;
    char currentDeviceName[NAME_BUF];

    currentDeviceID = getCurrentlySelectedDeviceID(typeRequested);
    getDeviceName(currentDeviceID, currentDeviceName);

    uint32_t scope = kAudioObjectPropertyScopeInput;

    switch (typeRequested) {
    case input:
        scope = kAudioObjectPropertyScopeInput;
        break;
    case output:
        scope = kAudioObjectPropertyScopeOutput;
        break;
    case all:
    case unspecified:
    default:
    }

    AudioObjectPropertyAddress propertyAddress = {
        .mSelector = kAudioDevicePropertyMute,
        .mScope = scope,
        .mElement = kAudioObjectPropertyElementMain,
    };

    uint32_t muted = (uint32_t)muteRequested;
    uint32_t propertySize = sizeof(muted);

    OSStatus status;
    if (muteRequested == toggle) {
        uint32_t dataSize;
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

void show_usage(char* exec_name) {
    printf("usage: %s [-a] [-c] [-t type] [-n] -s device_name | -i device_id | -u device_uid\n"
           "  -a             : shows all devices\n"
           "  -c             : shows current device\n\n"
           "  -f format      : output format (cli/human/json). defaults to human.\n"
           "  -t type        : device type (input/output/system/all). defaults to output.\n"
           "  -m mute        : sets the mute status (mute/unmute/toggle). for input/output only.\n"
           "  -n             : cycles the audio device to the next one\n"
           "  -i device_id   : sets the audio device to the given device by id\n"
           "  -u device_uid  : sets the audio device to the given device by uid or a substring of the uid\n"
           "  -s device_name : sets the audio device to the given device by name\n\n",
           exec_name //
    );
}
