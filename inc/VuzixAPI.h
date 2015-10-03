/*-----------------------------------------------------------------------------
 * VuzixAPI.h
 *
 * Copyright (c) 2015 Vuzix Corporation.  All rights reserved.
 *
 */
#ifndef _VUZIX_PUBLIC_API
#define _VUZIX_PUBLIC_API

//
// BE VERY,VERY CAREFUL CHANGING THIS FILE. IT IS USED BY MULTIPLE PRODUCTS/PROJECTS 
// AND ANY CHANGES MIGHT CAUSE VERY BAD, UNINTENTED CONSEQUENCES !!!!!!
//

/***************************************************************************************
 *
 * This file defines the base messages that are sent VIA HID to/from a Vuzix USB device.
 *
 * The base message IDS must NOT change. This file is shared between many USB devices and
 * PC applications.
 *
 * A new device can create NEW messages with command values between 0x80 and 0xEF. They are
 * application-specific.
 *
 * Those new message can have their own structures also but cannot exceed the maximum message
 * length defined in this file (MAX_PKT_PAYLOAD). To create your own new message structures, start
 * with COMMANDPKT and/or RESPONSEPKT but make a unique one for your own APP. Do NOT change this
 * file. The proper way to do it is to use the 'payload' buffer since its unformatted bytes. 
 * Map you structure over it and reference the elements as necessary. BE CAREFUL of little
 * endian vs. big endian differences ....
 * 
 ******************************************************************************************/

// This is our USB Vendor ID that is used on all the products to identify our devices
#define VUZIX_USB_VENDOR_ID             0x1BAE

// Report ID used when recieving the following messages from control pipe (GET_REPORT)
#define VUZIXAPI_GET_REPORT_ID          0x01
// Report ID used when sending the following messages down the interrupt pipe (OUT)
#define VUZIXAPI_SEND_CMD_REPORT_ID     0x02

#define VUZIXAPI_VERSION                0x02

typedef enum
{
  WRITE_I2C               =  0x01,        // DEPRECATED - Used on Wrap Products, Poke a value into an I2C device
  READ_I2C                =  0x02,        // DEPRECATED - Used on Wrap Products, Peek a value from an I2C device
  GET_VERSION             =  0x03,        // Read the current major/minor version from device
  GET_BOOT_VERSION        =  0x04,        // DEPRECATED - Used on Wrap Products, Get the Boot loader version
  GET_PRODUCT_ID          =  0x05,        // DEPRECATED - Used on Wrap Products, Return a product specfic ID. Return 0 if unknown or not supported.
  GET_HW_VERSION          =  0x06,        // Return current version of the HW. Return 0 if unknown or not supported.
  GET_UPPER_VERSION       =  0x07,        // DEPRECATED - Used on Wrap Products, Return current version of the Upper HW. Return 0 if unknown or not supported.
  GET_SYSTEM_INIT_STATE   =  0x08,        // DEPRECATED, Return current state of the initialization state.  Return 0 if unknown or not supported.
  WRITEMULTI_I2C          =  0x09,        // Poke values into an I2C device
  READMULTI_I2C           =  0x0A,        // Peek values from an I2C device
  WRITEMULTI_LONGADDR_I2C =  0x0B,        // Poke values into an I2C device with multi-byte address
  READMULTI_LONGADDR_I2C  =  0x0C,        // Peek values from an I2C device with multi-byte address

  RESTORE_DEFAULTS        =  0x10,        // Put NV RAM values back to factor defaults
  SAVE_TO_NVRAM           =  0x11,        // Save current values to NV RAM
  RESTORE_FROM_NVRAM      =  0x12,        // Read NV RAM values back to current values

  GET_BAT_LEVEL           =  0x25,        // Return current battery level. Payload is an 8-bit representation of the current battery level

  VUZIXAPI_VERSION_GET    =  0x40,        // Get the Vuzix API Version to see what commands are supported

  DEBUG_MSG               =  0x66,        // Used to send async debug data back to an app in RESPONSEPKT

  TRACKER_RAW_DATA        =  0x80,        // Sending tracker raw data packets back to host device
  SET_2D                  =  0x81,        // DEPRECATED - Replaced with SET_STEREO_MODE, used on 331
  SET_SXS                 =  0x82,        // DEPRECATED - Replaced with SET_STEREO_MODE, used on 331
  TRACKER_SET_REPORT_MODE =  0x86,        // Set tracker reporting mode
  SET_MENU_OVERRIDE       =  0x87,        // DEPRECATED - Used on Wrap Products, Allows control of button interface via USB
  SET_STEREO_MODE         =  0x88,        // Force switching between 2D/3D modes
  UNKNOWN_CMD             =  0x89,        // Device does not recognize command request.
  INVALID_ARG             =  0x8A,        // Device responds to requested command, but doesn't understand the argument.
  COMMAND_FAILED          =  0x8B,        // The command specified in payload[0] failed to execute properly

  TRACKER_QUATS_DATA      =  0x90,        // RESERVED FOR FUTURE USE, Sending tracker quaternion data packets back to host device
  TRACKER_EULER_DATA      =  0x91,        // RESERVED FOR FUTURE USE, Sending tracker Euler data packets back to host device
  SET_GYRO_ZERO           =  0x92,        // RESERVED FOR FUTURE USE, Calibrate gyros
  GET_GYRO_ZERO           =  0x93,        // RESERVED FOR FUTURE USE, Get gyros cal data
  CLEAR_GYRO_ZERO         =  0x94,        // RESERVED FOR FUTURE USE, Clear gyros cal data
  TRACKER_CALIBRATE       =  0x95,        // RESERVED FOR FUTURE USE, Starting Calibration of tracker
  TRACKER_GET_STATE       =  0x96,        // RESERVED FOR FUTURE USE, returns first byte: TRACKER_CALIBRATE state, second byte TRACKER_SET_REPORT_MODE

  CPU_RESET               =  0xF1         // Reset Device
} E_VUZIX_COMMANDS;

// USB Endpoint size for HID interface
// By convention Vuzix Products have Interrupt IN/OUT eps with max packet of 64
#define MAX_USB_BUFFER      64

// Max payload is 2 less than max size. Accounts for report id and command
#define MAX_PKT_PAYLOAD (MAX_USB_BUFFER - sizeof(unsigned char) - sizeof(unsigned char))

#if defined(_MSC_VER) || defined(__MICROLIB)
#pragma pack(1)
#endif

typedef struct _i2cwritepkt {
  unsigned char devaddr;          // I2C device address
  unsigned char regaddr;          // Register address within I2C device to write to
  unsigned char value;            // Data to write to register
} I2CWRITEPKT, *PI2CWRITEPKT;

typedef struct _i2creadpkt {
  unsigned char devaddr;          // I2C device address
  unsigned char regaddr;          // Register within I2C device to read
} I2CREADPKT, *PI2CREADPKT;

typedef struct _i2cwritemultipkt {
  unsigned char devaddr;          // I2C device address
  unsigned char regaddr;          // Register address within I2C device to write to
  unsigned char bytecount;
  unsigned char payload[MAX_PKT_PAYLOAD - 3];            // Data to write to register
} I2CWRITEMULTIPKT, *PI2CWRITEMULTIPKT;

typedef struct _i2creadmultipkt {
  unsigned char devaddr;          // I2C device address
  unsigned char regaddr;          // Register within I2C device to read
  unsigned char bytecount;
} I2CREADMULTIPKT, *PI2CREADMULTIPKT;

typedef struct _i2cwritemultipkt_longaddr {
  unsigned char devaddr;          // I2C device address
  unsigned long regaddr;          // 8-bit - 32-bit Register address within I2C device to write to
  unsigned char addrbytecount;    // how many address bytes are present
  unsigned char databytecount;    // How many data bytes are present
  unsigned char payload[MAX_PKT_PAYLOAD - 7];            // Data to write to register
} I2CWRITEMULTIPKT_LONGADDR, *PI2CWRITEMULTIPKT_LONGADDR;

typedef struct _i2creadmultipkt_longaddr {
  unsigned char devaddr;          // I2C device address
  unsigned long regaddr;          // 8-bit - 32-bit Register address within I2C device to write to
  unsigned char addrbytecount;    // how many address bytes are present
  unsigned char databytecount;    // How many data bytes are expected
} I2CREADMULTIPKT_LONGADDR, *PI2CREADMULTIPKT_LONGADDR;

typedef struct _EulerFormated {
  float         Roll;
  float         Pitch;
  float         Yaw;
  unsigned char payload[MAX_PKT_PAYLOAD - 12]; 
} EULERFORMATEDPKT, *PEULERFORMATEDPKT;

typedef struct _QuatsFormated {
  float         Q0;
  float         Q1;
  float         Q2;
  float         Q3;
  unsigned char payload[MAX_PKT_PAYLOAD - 16]; 
} QUATSFORMATEDPKT, *PQUATSFORMATEDPKT;

typedef struct _commandpkt {
  unsigned char pktcmd;           // the ID of the command this is a response to.
  union {
    unsigned char payload[MAX_PKT_PAYLOAD]; // Unformatted data. Use for any app specific messages
    I2CWRITEPKT i2cwrite;       // Write to an I2C Address
    I2CREADPKT  i2cread;        // Read from an I2C Address
    I2CWRITEMULTIPKT i2cwritemulti;       // Write to an I2C Address
    I2CREADMULTIPKT  i2creadmulti;        // Read from an I2C Address
    I2CWRITEMULTIPKT_LONGADDR i2cwritemulti_longaddr;       // Write to an I2C Device with a multi byte Address
    I2CREADMULTIPKT_LONGADDR  i2creadmulti_longaddr;        // Read from an I2C Device with a multi byte Address
    EULERFORMATEDPKT pkteuler;  // Packet format includes Euler on front and raw at back end.
    QUATSFORMATEDPKT pktquats;  // Packet format includes Quaternions on front and raw at back end.
    unsigned char   value;      // Use for commands that need to send 1 byte of data
  } pktdata;
} COMMANDPKT, *PCOMMANDPKT;

typedef struct _commandpktID {
  unsigned char repid; // The report id for hid to and from pipes.
  COMMANDPKT pkt;
} IDCOMMANDPKT, *PIDCOMMANDPKT;


// If a device does not support/require any of these fields they should fill them with 0xFF.
// 0xFF should be used as an error/unsused identifier and not used as a valid version number
// This structure superceeds all previous versions.
typedef struct _versionpkt {
  unsigned char USB_vmajor;           // Devices major version number (usually hardware version)
  unsigned char USB_vminor;           // Devices minor version number (usually software version)
  unsigned char ProductID;            // Used to identify Wraps from one another
  unsigned char  Subsys_HWversion;    // Subsystem hardware identifier
  unsigned char Subsys_vmajor;        // Subsystem major version number
  unsigned char  Subsys_vminor;       // Subsystem minor version number
  unsigned char  Tracker_HWversion;   // Tracker hardware version number
  unsigned char Tracker_SWversion;    // Tracker software version number
} VERSIONPKT, *PVERSIONPKT;

typedef struct _responsepkt {
  unsigned char pktcmd;           // the ID of the command this is a response to.
  union {
    unsigned char payload[MAX_PKT_PAYLOAD]; // Unformatted data. Use for any app specific messages
    unsigned char status;       // use this when the device returns a 'status'
    unsigned char value;        // Use this when device just returns a value
    VERSIONPKT version;         // Use this when device returns the standard version data
  } pktdata;
} RESPONSEPKT, *PRESPONSEPKT;

typedef enum
{
  eVFORMAT_MONO  =  0,
  eVFORMAT_SXS_HALF = 1,
  eVFORMAT_TOP_BOTTOM = 2,
  eVFORMAT_FRAME_PACKED = 3
} E_VIDEO_FORMATS;

// These are originally defined in the 311ES0001 Vuzix 20-Pin Interface document, but have been placed here for convenience
typedef enum
{
  eWRAP_310 = 1,
  eWRAP_920 = 2,
  eWRAP_280 = 3,
  eWRAP_230 = 4,
  eWRAP_1200 = 5,
  eWRAP_STAR1200 = 6,
  eWRAP_VR1200 = 7,
  eWRAP_STAR1200XL = 8,
  eWRAP_1200D = 9,
  eWRAP_STAR1200XLD = 10,
  eWRAP_VR1200D = 11
} E_WRAP_PRODUCT_ID;

typedef enum
{
  eDID_NOT = 0,
  eDID_TESTID = 1,
  eDID_WRAP_USBVGA = 331,
  eDID_VR920 = 227,
  eDID_WRAP_HDMI = 355,
  eDID_WRAP_LIION = 359,
  eDID_V720 = 413,
  eDID_WRAP_1200D = 418,
  eDID_M2000 = 424,
  eDID_STAR_1200D  = 427,
} E_DEVICE_ID;

// These states are mutually exclusive and end automatically.    
typedef enum
{
  eTRACKER_CALIBRATE_NOT_CALIBRATING  =  0x00,
  eTRACKER_CALIBRATE_MAGS              =  0x01,
  eTRACKER_CALIBRATE_DRIFT            =  0x02
} E_TRACKER_CALIBRATE_STATE;

// These modes are mutually exclusive and active until changed. 
typedef enum
{
  eTRACKER_MODE_OFF    =  0x00,
  eTRACKER_MODE_RAW    =  0x01,
  eTRACKER_MODE_EULER  =  0x02,
  eTRACKER_MODE_QUAT  =  0x03
} E_TRACKER_REPORT_MODE;

typedef enum
{
  eTRACKER_DETATCHED = 0xFF,
  eTRACKER_GEN_4 = 4,
  eTRACKER_GEN_7 = 7,
  eTRACKER_V720_STM = 19
} E_TRACKER_HWREV;

#endif
