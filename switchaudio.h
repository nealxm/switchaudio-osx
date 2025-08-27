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

#include <stdint.h>

#define NAME_BUF 128
#define UID_BUF 256
#define DEV_ARR 16

typedef enum {
    unspecified,
    input,
    output,
    all,
} device_type;

typedef enum {
    human_format,
    cli_format,
    json_format
} output_type;

typedef enum {
    unmute,
    mute,
    toggle
} mute_type;

typedef struct {
    bool show_all, show_curr, next_dev, set_dev, mute_dev;

    bool has_name, has_uid, has_id;
    char dev_name[NAME_BUF], dev_uid[UID_BUF];
    AudioObjectID dev_id;

    device_type dev_kind;
    output_type out_kind;
    mute_type mute_kind;
} cli_opts;

OSStatus run_cli(int, char**);
OSStatus process_args(cli_opts);

OSStatus show_devs(device_type, output_type);
OSStatus show_dev_curr(device_type, output_type);
OSStatus cycleNext(device_type);
OSStatus cycleNextForOneDevice(device_type);
OSStatus setOneDevice(AudioObjectID, device_type);
OSStatus setAllDevicesByName(char*);
OSStatus setMute(device_type, mute_type);
void show_usage(char*);

const char* getDeviceUID(AudioObjectID);
AudioObjectID getRequestedDeviceIDFromUIDSubstring(char*, device_type);
AudioObjectID getCurrentlySelectedDeviceID(device_type);
void getDeviceName(AudioObjectID, char*);
device_type getDeviceType(AudioObjectID);
bool isAnInputDevice(AudioObjectID);
bool isAnOutputDevice(AudioObjectID);
char* deviceTypeName(device_type);
AudioObjectID getRequestedDeviceID(char*, device_type);
AudioObjectID getNextDeviceID(AudioObjectID, device_type);
