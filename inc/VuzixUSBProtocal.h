//
// VuzixUSBProtocal.h:  Needs to define all constants related to USB IO for Vuzix MCU protocal.
//	Please review the description at the top of the VuzixUSBProtocal.cpp file for an in-depth description of "How-This-Works"
//
#include "VuzixApi.h"
#define MCU_RETRYCOUNT			3	// Retry count on outgoing packet transmissions.
#define MCU_RECEIVETIME_MS		64	// What is the maximum time that USB device requires for any given command.
									// 331 = 64Ms, 192Ms = 418, 500Ms = 386-Pcas, 64Ms = V720 
									// PCAS - 386 required 500Ms for RGB Register set command and 200 - 400 for others...
									// ... Please minimize this timeout...
#define MCU_CANCELTHRD_TIME		1000// Time for Worker threads to terminate...Or be cancelioex'ed from the OS.
// Default Read packets Size of input Queue.
#define	MCU_INPUT_QUE_SIZE		2048 // * 4MS = 8 Seconds of buffered packets...This is # of packets. 
#define MCU_INPUT_QUE_SZ_MIN	2	// Minimum number of packets in all queue allocations.
// All MCU modules return codes listed below.
enum MCU_RETURN { 
	MCU_OK						= 0, 
	MCU_OFFLINE					= 1, 
	MCU_UNKNOWN					= 2, 
	MCU_INVALIDARG				= 3, 
	MCU_COMMAND_FAILED			= 4, 
	MCU_TRACKER_FAILED			= 5, 
	MCU_IOBUSY					= 6, 
	MCU_WR_WRITEFAILED			= 7, 
	MCU_WR_IOFAILURE			= 8, 
	MCU_RD_NOT_RECEIVED			= 9, 
	MCU_RD_IOFAILURE			= 10, 
	MCU_RD_SZFAILURE			= 11, 
	MCU_SEND_TIMEOUT			= 12, 
	MCU_DEVICE_IN_STARTUP		= 13, 
	MCU_RD_QUE_NOT_EMPTY		= 14, 
	MCU_RD_QUE_FULL				= 15, 
	MCU_TRACKER_OFF				= 16, 
	MCU_UNSUPPORTED				= 17, 
	MCU_UNSUPPORTED_SIZEOF		= 18,
	MCU_TRACKER_CORRUPT			= 19,
	MCU_NO_TRACKER				= 20,
	MCU_NOTRACKER_INSTANCE		= 21,
	MCU_NOCHANGE				= 22,
	MCU_DISCONNECTED			= 23,
	MCU_CONNECTED				= 24,
	MCU_INVALID_ARG1			= 25,
	MCU_INVALID_ARG2			= 26,
	MCU_INVALID_ARG3			= 27,
	MCU_INVALID_ARG4			= 28,
	MCU_ALLOCATE_QUE_FAILED		= 29,
	MCU_ALLOCATE_ERRQUE_FAILED	= 30,
	MCU_QUE_SIZE_FAILURE		= 31
};
#ifdef __cplusplus
extern "C" {
#endif
#ifdef _DEF_VZXUSB_PROTOCAL
// Startup-shutdown and Error functions.
unsigned long		MCUAllocateResources( unsigned long ThreadCancelTime, unsigned long RecievedPacketQueSize, HWND, UINT );
unsigned long		MCUReleaseResources( unsigned long ThreadCancelTime );
// Processing functions.
unsigned long		MCUConnectWithDevice( LPARAM StateChange, unsigned int *DeviceId );
// Logic Method #2,3,etc...
unsigned long		MCUReceivePackets( PIDCOMMANDPKT Send_pkt, PIDCOMMANDPKT Rcv_pkt );
unsigned long		MCUSendCommand( PIDCOMMANDPKT Send_pkt, PIDCOMMANDPKT Rcv_pkt, int RetryCnt, int MSTimeout );
// Logic Method #1
unsigned long		MCUSendIoPacket( PIDCOMMANDPKT Send_pkt, int RetryCnt, int MSTimeout );
// Convenience calls.
HANDLE				MCUOpenTracker( unsigned int *DeviceId );
unsigned long		MCUGetDeviceStatus( void );
void				MCUProcessError( PIDCOMMANDPKT Send_pkt, PIDCOMMANDPKT Rcv_pkt, unsigned long ret, TCHAR *eStr, int szOfstr );
void				MCUFlushReadThreadQue( void );
int					MCUGetRcvCmdSize( unsigned char pktcmd );
int					MCUGetSendCmdSize( unsigned char pktcmd );
#else
extern unsigned long	MCUAllocateResources( unsigned long ThreadCancelTime, unsigned long RecievedPacketQueSize, HWND, UINT );
extern unsigned long	MCUReleaseResources( unsigned long ThreadCancelTime );
extern unsigned long	MCUConnectWithDevice( LPARAM StateChange, unsigned int *DeviceId );
extern unsigned long	MCUReceivePackets( PIDCOMMANDPKT Send_pkt, PIDCOMMANDPKT Rcv_pkt );
extern unsigned long	MCUSendCommand( PIDCOMMANDPKT Send_pkt, PIDCOMMANDPKT Rcv_pkt, int RetryCnt, int MSTimeout );
extern unsigned long	MCUSendIoPacket( PIDCOMMANDPKT Send_pkt, int RetryCnt, int MSTimeout );
extern HANDLE			MCUOpenTracker( unsigned int *DeviceId );
extern unsigned long	MCUGetDeviceStatus( void );
extern void				MCUProcessError( PIDCOMMANDPKT Send_pkt, PIDCOMMANDPKT Rcv_pkt, unsigned long ret, TCHAR *eStr, int szOfstr );
extern void				MCUFlushReadThreadQue( void );
extern int				MCUGetRcvCmdSize( unsigned char pktcmd );
extern int				MCUGetSendCmdSize( unsigned char pktcmd );
#endif
#ifdef __cplusplus
}
#endif
