//
// VuzixUSBProtocal.cpp: 
//	Contains All protocal access to a standard Vuzix USB device.
//	 - Read Thread opens a handle to the USB device and reads all recieved packets into a packet que.
//   - Write Thread opens a second handle to the USB device and provides a means of queing up a command to send to the USB device.
//      Contains a startup-connection method to setup all system resources for processing in and out packets.
//      Read que is definable in size see below fields for intialization.
//		Writefile and ReadFile operations have timeout and retry count as defined via setup fields.
// 
// Whats this VuzixUSBProtocal source and header module all about?
//
// An Application that communicates with Vuzix Standard USB devices can be written in one of 3"Many" logic methods.
//		These methods are descibed below.  Every attempt at a clean-lean and fast turnaround solution has been sought after.
//		The input packet que size is sizeable at intialization time.
//		The Wait time for the read-write threads to clean up and terminate is definable.
//		The retry count and timeout paramaters are dynamic and are essential in understanding the USB interface and the device is
//		In good working condition.  As all responces are checked and verified in this circular hand-shake process. 
//		For Methods 2,3,etc...
//			The Packet Recieve queue is a circular array.  The queue will continue to overrun itself(when full) if not flushed between polling and processing.
//			Low frequency applications should Read and process the que till empty, between process calls.
// 
// Three of the top choices for interfacing to the protocal module are as follows.
// 1: MFC Dialog app. Using OnTimer for low frequency status and feedback.
//		In this method the developer would like the ease + convience of windows messaging and dialog thread management.
//		Warning:	PostMessage(...) method is demonstrated here and has been known to lack performance when used for high frequency logic.
//		Uses:		MCUSendIoPacket( &Send_pkt, MCU_RECEIVETIME_MS ); 
//						Relies on application hooking WM_APP messages to process received packets.
//						no retries offered on this command.
//
// 2: MFC Dialog app. with Multimedia timer higher frequency status and feedback.
//		In this method the developer has realized there are high frequency requirements that only an independant command thread 
//		Can resolve, to ensure the processing of received packets is kept in sync. with the interface.
//		As well as offering an independant thread to command and control from a dialog box or other threading foreground process.
//		Uses:		MCUSendCommand( &Send_pkt, &Rcv_pkt, MCU_RETRYCOUNT, MCU_RECEIVETIME_MS );
//						Relies on application providing a high performance Thread that will process all send and recieved packets localy in that thread.
//						With retries for commands being sent.
//
// 3: Win32 Console or Windows application.  A direct at Processor speeds polling method.
//		In this method the developer has a desire to build a very high frequency application. greater than 1000Hz. 
//		Such that a one to one relationship could exist with the application and the Vuzix USB packet streaming device.
//		Uses:		MCUSendCommand( &Send_pkt, &Rcv_pkt, MCU_RETRYCOUNT, MCU_RECEIVETIME_MS );
//						Relies on application providing a high performance Thread that will process all send and recieved packets localy in that thread.
//						With retries for commands being sent.
//
// The VuzixUSBProtocal.cpp,.h source module will provide the nesseccary support for all 3"Many" logic methods presented above.
//		The protocal modules are coded in ".C" Form to support a short-term release and with that can be easily ported to the C++ and C 
//		Source projects.
//
// It is recommended the processing of RAW packets be broken up into real-time vs. non realtime required logic.
//	If no logic is requred to be processed in realtime as the packets are recieved.  
//		Then delay the processing for when the application request the packets be processed into a usable format.
//		This will optimize streaming and storing time.  and Minimize stress-strain on the read and write USB protocal threads.
//	When the application is ready to present or render processed raw data from the USB prootcal device. then cook any and all packets in the 
//		packet que to arrive at a final Cooked answer that is most current with the sensors onboard.
//
// Example:
//  Thread-Procedure 1: Receives packets and identifies them only.
//		Procedure 1 is called for every packet received in the input queue.
//
//  Thread-Procedure 2: Process Tracker packets into cooked final application format.
//		Procedure 2 is only ever called when the application requests new tracker data.
//
#pragma once
#ifndef VC_EXTRALEAN
#define VC_EXTRALEAN            // Exclude rarely-used stuff from Windows headers
#endif
// Including SDKDDKVer.h defines the highest available Windows platform.
// If you wish to build your application for a previous Windows platform, include WinSDKVer.h and
// set the _WIN32_WINNT macro to the platform you wish to support before including SDKDDKVer.h.
#include <SDKDDKVer.h>
// turns off MFC's hiding of some common and often safely ignored warning messages
#define _AFX_ALL_WARNINGS
#include <afxwin.h>         // MFC core and standard components

#include <setupapi.h>
#ifdef __cplusplus
extern "C" {
#endif
#include <hidsdi.h>
#ifdef __cplusplus
}
#endif
#include "VuzixApi.h"
#define _DEF_VZXUSB_PROTOCAL
#include "VuzixUSBProtocal.h"
// Static call definitions.
static bool				MCUKillCommunicationThreads( void );
// Retry and timeout intervals.  Will be passed as args to process calls.
// MCU Global specifics. As set by application calls.
static unsigned long	m_ThreadCancelTimeMS= MCU_CANCELTHRD_TIME;
static int				m_DeviceRetry		= MCU_RETRYCOUNT;
static int				m_DeviceTimeOut		= MCU_RECEIVETIME_MS;
static HWND				m_MCUApplicationWnd	= NULL;
static UINT				m_MCUApplicationMsg = WM_APP;
// Critical Thread globals...
// Write packets/commands.
static HANDLE			m_HidWrite_hDrv		= INVALID_HANDLE_VALUE;
static bool				m_WriteThreadEnabled= false;
static IDCOMMANDPKT		m_Send_Packet;
static CWinThread		*m_WriteThread_hnd	= NULL;
static volatile bool	m_WriteSendPacket	= false;
static unsigned long	m_WriteThread_Error	= MCU_OFFLINE;
static CRITICAL_SECTION	m_csWrite; // Required to ensure WriteFile and Sendio access is protected.
// Read packets/commands.
static HANDLE			m_HidRead_hDrv		= INVALID_HANDLE_VALUE;
static bool				m_ReadThreadEnabled = false;
static PIDCOMMANDPKT	m_Rcv_Packet		= NULL;
static CWinThread		*m_ReadThread_hnd	= NULL;
static unsigned long	*m_ReadThread_Error	= NULL;
static volatile int		m_RDQueue_head		= 0;
static volatile int		m_RDQueue_tail		= 0;
static int				m_RecievePacketQueSize = MCU_INPUT_QUE_SIZE;
static CRITICAL_SECTION	m_csRead; // Required to ensure memory access is protected.
//
// Allocate resources used by MCU-USB protocal.
//	Allocate One-time startup resources for protocal modules and System OS interaction.
//
unsigned long	MCUAllocateResources( unsigned long ThreadCancelTime, unsigned long RecievedPacketQueSize, HWND AppWnd, UINT AppMsg )
{
	// Allocate an Input Packet queue size sufficient enough for specific application development.
	// Ensure proper queue sizes.
	if( RecievedPacketQueSize < MCU_INPUT_QUE_SZ_MIN )
		return MCU_QUE_SIZE_FAILURE;
	m_Rcv_Packet	= new IDCOMMANDPKT[ RecievedPacketQueSize ];
	if( m_Rcv_Packet == NULL ) 
		return MCU_ALLOCATE_QUE_FAILED;
	// Allocate error indexes to parallel packet stream.
	m_ReadThread_Error	= new unsigned long[ RecievedPacketQueSize ];
	if( m_ReadThread_Error == NULL ) 
		return MCU_ALLOCATE_ERRQUE_FAILED;
	// Reset size of recive packet queue.
	m_RecievePacketQueSize	= RecievedPacketQueSize;
	// Hold Thread Cancel Time
	m_ThreadCancelTimeMS	= ThreadCancelTime;
	// Setup global Windows PostMessaging fields;  IF and ONLY if the application is developed a requirement for them.
	m_MCUApplicationWnd		= AppWnd;
	m_MCUApplicationMsg		= AppMsg;
	// Initialize a critical section to provide shareing of time-stamping packets.
	InitializeCriticalSection(&m_csRead);
	InitializeCriticalSection(&m_csWrite);
	return MCU_OK;
}

//
// Release resources used by MCU-USB protocal.
//   -1- Second should be sufficient time to flush out the ques for the Worker threads.
//
unsigned long	MCUReleaseResources( unsigned long ThreadCancelTime )
{
	// Hold Thread Cancel Time
	m_ThreadCancelTimeMS	= ThreadCancelTime;
	// kill communication threads.
	MCUKillCommunicationThreads( );
	// Reset messaging window, to ensure no false message sends.
	m_MCUApplicationWnd		= NULL;
	m_MCUApplicationMsg		= WM_APP;
	// Delete the critical sections, as the threads are no longer operational.
	DeleteCriticalSection(&m_csWrite);
	DeleteCriticalSection(&m_csRead);
	// Release all recieve input packet Que fields.
	if( m_Rcv_Packet ) { 
		delete m_Rcv_Packet;
		m_Rcv_Packet = NULL;
	}
	if( m_ReadThread_Error ) { 
		delete m_ReadThread_Error;
		m_ReadThread_Error = NULL;
	}
	return MCU_OK;
}

// 
// Provides a means of shutting down the Protocal threads prior to exit or USB Un-detection.
//	Dont kill if thread handles are not allocated.
//
static bool		MCUKillCommunicationThreads( void )
{
bool	FirsttimeDetected=false;
	// Proper kill of readFile thread.
	if (m_ReadThread_hnd) {
		EnterCriticalSection(&m_csRead);
		m_ReadThreadEnabled = false;
		LeaveCriticalSection(&m_csRead);
		FirsttimeDetected = true;
		CancelIoEx( m_HidRead_hDrv, NULL ); // Force exit of Thread if Attempting to send command...
		m_HidRead_hDrv = INVALID_HANDLE_VALUE;
		WaitForSingleObject( m_ReadThread_hnd->m_hThread, m_ThreadCancelTimeMS ); // 1 second...
		m_ReadThread_hnd = NULL;
	}
	// Proper kill of writeFile thread.
	if (m_WriteThread_hnd) {
		EnterCriticalSection(&m_csWrite);
		m_WriteThreadEnabled = false;
		LeaveCriticalSection(&m_csWrite);
		FirsttimeDetected = true;
		CancelIoEx( m_HidWrite_hDrv, NULL ); // Force exit of Thread if Attempting to send command...
		m_HidWrite_hDrv = INVALID_HANDLE_VALUE;
		WaitForSingleObject( m_WriteThread_hnd->m_hThread, m_ThreadCancelTimeMS ); // 1 second...
		m_WriteThread_hnd = NULL;
	}	
	return FirsttimeDetected;
}

//
#define ICU_REGBUF_SIZE		1024// Maximum device path name, any Vuzix USB device will ever be.
#define MAX_VZX_DEVS		6	// Total # of Vuzix USB devices this protocal is valid with.
#define HIDREAD_SIZE_331	42	// Apparently 331 only sends back this size packet even when Get_HidUreportlength() is 64...
// crt: New and improved MCUOpenTracker(...) process call.
//	Changes will be required upon moving to new driver-Application architecture.
//	Provides a means of enumerating the system and finding any Vuzix USB device that is currently plugged into the system.
//
HANDLE		MCUOpenTracker( unsigned int *DeviceId )
{
TCHAR	VidPids[ MAX_VZX_DEVS ][MAX_PATH] = {
	_T("VID_1BAE&PID_01A2"),// 418 - Wrap 1200D HDMI Box
	_T("VID_1BAE&PID_01AB"),// 427 - STAR 1200 XLD HDMI Box
	_T("VID_1BAE&PID_01A8"),// 424 - M2000AR ... crt 
	_T("VID_1BAE&PID_019D") // 413 - iWear
};
unsigned int	DeviceIds[ MAX_VZX_DEVS ] = {
	eDID_WRAP_1200D,	// 418 - 1200D device
	eDID_STAR_1200D,	// 427 - Star 1200D device
	eDID_M2000,			// 424 - M2000AR
	eDID_V720			// 413 - iWear
};
HDEVINFO						hDevInfo;
SP_DEVICE_INTERFACE_DATA		devIFData;
PSP_DEVICE_INTERFACE_DETAIL_DATA pDevIFDetail;
HANDLE							hUSBRetHandle = INVALID_HANDLE_VALUE;
GUID							hidGuid;
DWORD							index = 0, length;
int								i;
BOOL							USBHasInterfaces, USBHasDetail, FoundVZXDevice = FALSE;
TCHAR							devpath[ ICU_REGBUF_SIZE ];
	// Enumerate all USB device ports...
	HidD_GetHidGuid(&hidGuid);
	// Only USB Devices that are present...
	hDevInfo = SetupDiGetClassDevs(	&hidGuid, NULL, NULL, (DIGCF_PRESENT | DIGCF_DEVICEINTERFACE));
	if( hDevInfo == INVALID_HANDLE_VALUE ) {
		// Setup as undetected device Id.
		*DeviceId = eDID_NOT;
		// crt: Bug must always return INVALID_HANDLE_VALUE on failure.
		return INVALID_HANDLE_VALUE; 
	}
	// Enumerate: One time through all USB devices...
	devIFData.cbSize = sizeof(SP_DEVICE_INTERFACE_DATA);
	USBHasInterfaces = SetupDiEnumDeviceInterfaces( hDevInfo, NULL, &hidGuid, index, &devIFData );
	while( USBHasInterfaces ) {
		// Make a pass to get the correct buffer size for IF details
		USBHasDetail = SetupDiGetDeviceInterfaceDetail( hDevInfo, &devIFData, NULL, 0, &length, NULL );
		if( !USBHasDetail && (GetLastError() == ERROR_INSUFFICIENT_BUFFER) ) {
			pDevIFDetail = (PSP_DEVICE_INTERFACE_DETAIL_DATA)malloc(length);
			if( pDevIFDetail ) {
				pDevIFDetail->cbSize = sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA);
				USBHasDetail = SetupDiGetDeviceInterfaceDetail( hDevInfo, &devIFData, pDevIFDetail, length, &length, NULL );
				if( USBHasDetail ) {
					memset( devpath, 0, ICU_REGBUF_SIZE );
					memcpy( devpath, pDevIFDetail->DevicePath, min(_tcslen(pDevIFDetail->DevicePath), (ICU_REGBUF_SIZE - 1)) );
					_tcsupr_s( devpath, ICU_REGBUF_SIZE );
					// crt: Good time to check ALL Vid/Pids for VZX devices...
					// On first device found break out and return valid handle...
					for( i = 0; i < MAX_VZX_DEVS; i++ ) {
						if( _tcsstr( devpath, VidPids[i] ) != NULL ) { 
							hUSBRetHandle = CreateFile(pDevIFDetail->DevicePath,
											  GENERIC_READ|GENERIC_WRITE,
											  FILE_SHARE_READ | FILE_SHARE_WRITE,
											  NULL,        // no SECURITY_ATTRIBUTES structure
											  OPEN_EXISTING, // No special create flags
											  0,			// No overlapped io required. CancelIoEx works fine.
											  NULL);       // No template file
							// Stop on First device.
							// NOTE: Multiconnect will require to continue to end of enums.
							// NOTE: Multiconnect will require list of handles.AND
							//		A method to connect each handle to each enumed device.
							FoundVZXDevice	= TRUE;
							*DeviceId		= DeviceIds[ i ]; 
							// Again: in multiconnect continue for all possible Ports.
							break;
						}
					}
				}
				free(pDevIFDetail);
			}
		}
		// Again: in multiconnect this wont be the case...
		if( FoundVZXDevice ) 
			break;
		index++;
		USBHasInterfaces = SetupDiEnumDeviceInterfaces( hDevInfo, NULL, &hidGuid, index, &devIFData );
	}
	if( !FoundVZXDevice ) 
		// Setup as undetected device Id.
		*DeviceId = eDID_NOT;
	// Release SetupDi resources.
	SetupDiDestroyDeviceInfoList( hDevInfo );
	return hUSBRetHandle;
}

void		MCUFlushReadThreadQue( void )
{
	// May require Interlock/Critical section protection...
	// However, Thread should be dead at this point...
	EnterCriticalSection(&m_csRead);
	m_RDQueue_head = m_RDQueue_tail = 0;
	LeaveCriticalSection(&m_csRead);
}

//
// crt: Now no longer requires speical Vuzix project libraries. Those with bugs.
//
DWORD		USB_GetOutputReportLength(HANDLE hDrv)
{
DWORD					rc = (DWORD)-1;
PHIDP_PREPARSED_DATA	phpd;
HIDP_CAPS				hcaps;
	if (HidD_GetPreparsedData(hDrv, &phpd)) {
		if (HidP_GetCaps(phpd, &hcaps) == HIDP_STATUS_SUCCESS) 
			rc = hcaps.OutputReportByteLength;
	}
	return(rc);
}

//
// crt: Now operates as a Que packet system...
//
UINT		ReadThreadWorker( LPVOID pParam )
{
bool			threadEnabled = false;
DWORD			BytesRead;
DWORD			m_Read_length = 0;
BOOL			Read_Return;
IDCOMMANDPKT	Local_packet;
unsigned int	DeviceId;
	// Fast on/off of critical section...
	EnterCriticalSection(&m_csRead);
	// Init first que error buffer flag to ok...
	m_ReadThread_Error[m_RDQueue_head] = MCU_RD_NOT_RECEIVED;
	threadEnabled = m_ReadThreadEnabled	= true;
	// Release critical section ASAP...
	LeaveCriticalSection(&m_csRead);
	// ReadPipe: Attempt to connect to USB device.
	m_HidRead_hDrv		= MCUOpenTracker( &DeviceId ); 
	if( m_HidRead_hDrv != INVALID_HANDLE_VALUE ) {
		// Collect all global details now...
		if( DeviceId == eDID_WRAP_USBVGA )
			m_Read_length = HIDREAD_SIZE_331;
		else
			m_Read_length = USB_GetOutputReportLength(m_HidRead_hDrv);
		// Wait for incomming packets...
		while( true ) {
			// Cancel thread if commanded so.
			if( !threadEnabled ) 
				break;
			// Readfile and wait...
			Read_Return = ReadFile( m_HidRead_hDrv, &Local_packet, m_Read_length, &BytesRead, NULL );
			// Fast on/off of critical section...
			EnterCriticalSection(&m_csRead);
			threadEnabled	= m_ReadThreadEnabled;
			// Check for readfile error...
			if( !Read_Return ) 
				m_ReadThread_Error[m_RDQueue_head] = MCU_RD_IOFAILURE; //Report Read Error...
			else
			// Check and process received packet...
			if( BytesRead != m_Read_length ) 
				m_ReadThread_Error[m_RDQueue_head] = MCU_RD_SZFAILURE;//Report Read size Error...
			else {
				// Move read packet to packet que...
				memcpy( &m_Rcv_Packet[m_RDQueue_head], &Local_packet, sizeof( IDCOMMANDPKT ) );
				// Clear readfile error...
				m_ReadThread_Error[m_RDQueue_head] = MCU_OK;
			}
			// Provided developer has requested a windows message to be posted at every received packet.
			// WARNING: it has been noted, the Windows message que will not always wake and send the message at postmessage(..) time.
			if( m_MCUApplicationWnd ) {
				memcpy( &Local_packet, &m_Rcv_Packet[m_RDQueue_head], sizeof( IDCOMMANDPKT ) );
				// Release critical section ASAP...
				LeaveCriticalSection(&m_csRead);
				// Warning sending a local address to model/modeless dialog thread.  Should be safe.
				// Using SendMessage(...) Appears to fix the PostMessage(...) loss of messages issue. try Get_Version with raw packets on.
				if( threadEnabled ) 
					SendMessage( m_MCUApplicationWnd, m_MCUApplicationMsg, m_ReadThread_Error[m_RDQueue_head], (LPARAM)&Local_packet );
				else 
					break;
				// Fast on/off of critical section...
				EnterCriticalSection(&m_csRead);
				}
			// Prepare for next packet read...
			m_RDQueue_head++;
			if( m_RDQueue_head >= m_RecievePacketQueSize ) 
				m_RDQueue_head = 0; // Clobber older packets as the app had more than enough time...
			// Release critical section ASAP...
			LeaveCriticalSection(&m_csRead);
			// Give back some processor time...This happens in ReadFile...Still good idea.
			SleepEx( 0, TRUE );
		}
		CancelIoEx( m_HidRead_hDrv, NULL );
		CloseHandle( m_HidRead_hDrv );
		// Fast on/off of critical section...
		EnterCriticalSection(&m_csRead);
		// Force the read queue to appear empty
		m_RDQueue_head = m_RDQueue_tail;
		m_HidRead_hDrv = INVALID_HANDLE_VALUE;
		// Release critical section ASAP...
		LeaveCriticalSection(&m_csRead);
	}
    return 0;   // thread completed successfully
}

UINT		WriteThreadWorker( LPVOID pParam )
{
bool			Local_SendPacket=false, threadEnabled=false;
BOOL			ret = MCU_IOBUSY;
DWORD			BytesWritten;
DWORD			m_Write_length = 0;
IDCOMMANDPKT	Local_packet;
unsigned int	DeviceId;
	// Less time with critical section lock.
	EnterCriticalSection(&m_csWrite);
	threadEnabled = m_WriteThreadEnabled = true;
	LeaveCriticalSection(&m_csWrite);
	// WritePipe: Attempt to connect to USB device.
	m_HidWrite_hDrv = MCUOpenTracker( &DeviceId );
	if( m_HidWrite_hDrv != INVALID_HANDLE_VALUE) {
		// Both read and Write pipes are same size...
		m_Write_length	= USB_GetOutputReportLength(m_HidWrite_hDrv);
		// Begin processing foreground commands...
		while( true ) {
			// Protect concurrent access...
			EnterCriticalSection(&m_csWrite);
			if( m_WriteThread_Error == MCU_OFFLINE )
				m_WriteThread_Error	= MCU_OK; // Only if MCU was offline state.
			threadEnabled		= m_WriteThreadEnabled;
			Local_SendPacket	= m_WriteSendPacket;
			LeaveCriticalSection(&m_csWrite);
			// Terminate if thread is killed.
			if( !threadEnabled )
				break;
			if( Local_SendPacket ) {
				// Protect concurrent access...
				EnterCriticalSection(&m_csWrite);
				m_WriteSendPacket = false; 
				memcpy( &Local_packet, &m_Send_Packet, sizeof( IDCOMMANDPKT ) );
				// Release CS...
				LeaveCriticalSection(&m_csWrite);
				// Perform Write of global command packet...
				ret = WriteFile( m_HidWrite_hDrv, &Local_packet, m_Write_length, &BytesWritten, NULL );
				// Protect concurrent access...
				EnterCriticalSection(&m_csWrite);
				if( !ret ) 
					m_WriteThread_Error = MCU_WR_IOFAILURE;// Will get here if foreground CancelsIoEx()...or internal error...
				else
				if( m_Write_length != BytesWritten )
					m_WriteThread_Error = MCU_WR_WRITEFAILED;
				else
					m_WriteThread_Error = MCU_OK;
				// Release CS...
				LeaveCriticalSection(&m_csWrite);
			}
			// Give back some processor time.
			SleepEx( 0, TRUE );
		}
		CancelIoEx( m_HidWrite_hDrv, NULL );
		CloseHandle( m_HidWrite_hDrv );
		// Fast on/off of critical section...
		EnterCriticalSection(&m_csWrite);
		m_HidWrite_hDrv = INVALID_HANDLE_VALUE;
		// Release CS...
		LeaveCriticalSection(&m_csWrite);
	}
	m_WriteThread_Error = MCU_OFFLINE;
    return 0;   
}

//
// If Vuzix USB Device is detected:
//	Find the USB tracker system and connect and startup worker threads.
// If the Vuizx USB Device is NOT Detected:
//	Release startup worker threads and resources.
//
unsigned long	MCUConnectWithDevice( LPARAM StateChange, unsigned int *DeviceId )
{
HANDLE				hDrv;
bool				FirsttimeDetected=false;
unsigned long		iwrStatus=MCU_NOCHANGE;
	// Find USB Tracker Vid-Pid
	hDrv = MCUOpenTracker( DeviceId );
	if (hDrv != INVALID_HANDLE_VALUE) {
		// Shut down test handle...
		CloseHandle(hDrv);
		// Lets get a read thread working for the interrupt in.
		if (m_ReadThread_hnd == NULL) {
			// Flush RD_Input que...
			MCUFlushReadThreadQue();
			m_ReadThread_hnd = AfxBeginThread( ReadThreadWorker, NULL, THREAD_PRIORITY_TIME_CRITICAL );
			FirsttimeDetected = true;
		}
		// Start write thread...
		if (m_WriteThread_hnd == NULL) {
			m_WriteThread_hnd = AfxBeginThread( WriteThreadWorker, NULL, THREAD_PRIORITY_TIME_CRITICAL );
			FirsttimeDetected = true;
		}
		// First time or software restart override.
		if( FirsttimeDetected || StateChange ) {
			// Return to app. transition has occured.
			iwrStatus = MCU_CONNECTED;
			// Ensure Worker threads are up and running.
			SleepEx( m_ThreadCancelTimeMS, FALSE );
		}
	}
	else {
		// Flush RD_Input que...
		MCUFlushReadThreadQue();
		// Re-Set signal for Polling process to discontinue attempting to startup the USB Device.
		FirsttimeDetected = MCUKillCommunicationThreads( );
		// First time or software restart override.
		if( FirsttimeDetected || StateChange ) 
			// Return to app. transition has occured.
			iwrStatus = MCU_DISCONNECTED;
	}
	return iwrStatus;
}

//
// If Vuzix USB Device is detected:
//	Determine the status of the Read or Write thread and return to application.
//  Is relative to last known sent and received packet.
//
unsigned long	MCUGetDeviceStatus( void )
{
unsigned long	ret=MCU_OK;
bool			readThread=false;
	EnterCriticalSection(&m_csWrite);
	// Last known write command is valid or failed?
	ret = m_WriteThread_Error;
	LeaveCriticalSection(&m_csWrite);
	if( ret == MCU_OK ) {
		// Last known recieved packet determines status.
		EnterCriticalSection(&m_csRead);
		readThread = m_ReadThreadEnabled;
		if( !readThread ) 
			ret = MCU_OFFLINE;
		else {
			if( m_RDQueue_head == m_RDQueue_tail )
				ret = MCU_OK;
			else
			if( m_RDQueue_head == 0 )
				ret = m_ReadThread_Error[m_RecievePacketQueSize-1];
			else
				ret = m_ReadThread_Error[m_RDQueue_head-1];
		}
		LeaveCriticalSection(&m_csRead);
	}
	return ret;
}

//
// Meethod to send a command packet to the Vuzix device without waiting for responce.
//  All Vuzix USB devices should be designed with proper handshake protocal on all commands.
//	Only to be used with method 1 logic implimentations.
//
unsigned long	MCUSendIoPacket( PIDCOMMANDPKT Send_pkt, int RetryCnt, int MSTimeout )
{
int				i, j;
unsigned long	ret=MCU_IOBUSY;
	if( Send_pkt == NULL ) 
		return MCU_INVALID_ARG1; // Do not allow Null send packet.
	if( RetryCnt <= 0 ) 
		return MCU_INVALID_ARG2; // Do not allow 0 RetryCounts.
	if( MSTimeout <= 0 ) 
		return MCU_INVALID_ARG3; // Do not allow 0-Ms on Timeout.
	m_DeviceRetry	= RetryCnt;
	m_DeviceTimeOut	= MSTimeout;
	if( m_HidWrite_hDrv != INVALID_HANDLE_VALUE ) {
		// The application does not require counting the total number of retrys...FYI...
		for( i=0; i < m_DeviceRetry; i++ ) {
			// And again protect concurrent access...should have never happened.
			EnterCriticalSection(&m_csWrite);
			// Prepare Write thread to send command...
			memcpy( &m_Send_Packet, Send_pkt, sizeof(IDCOMMANDPKT) );
			// Enforce Product id...on Every write command.
			m_Send_Packet.repid = VUZIXAPI_SEND_CMD_REPORT_ID;
			m_WriteThread_Error = MCU_IOBUSY;	// Force the Write thread status to busy.
			m_WriteSendPacket	= true;
			// Done with Write CS...
			LeaveCriticalSection(&m_csWrite);
			// wait for WriteThread to send or Error or Time-out...
			for( j=0; j < m_DeviceTimeOut; j++ ) {
				EnterCriticalSection(&m_csWrite);
				ret = m_WriteThread_Error;
				LeaveCriticalSection(&m_csWrite);
				if( ret != MCU_IOBUSY )
					return ret; // got a failure or command sent out to device ok.
				SleepEx( 1, TRUE ); // minimize the send/rcv round trip intercept time.
			}
			CancelIoEx( m_HidWrite_hDrv, NULL );// Force Write command thread to abort and try again on next send?
			// flush output writefile buffer to ensure one-one cmd send...
			// crt: 2.3 - Flushing is very costly: observed as much as 20 Ms addition to timeout process.
			FlushFileBuffers( m_HidWrite_hDrv );
		}
		return MCU_IOBUSY; // Timeout while attempting to communicate with Device.
	}
	else
		return MCU_OFFLINE;
}

//
// Poll/Readfile/GetHidReport() AND flush all queued received packets.
// DO NOT BLOCK THE Polling / requesting process here...
// If read operation reports error interpret here.	
//
unsigned long	MCUReceivePackets( PIDCOMMANDPKT Send_pkt, PIDCOMMANDPKT Rcv_pkt )
{
unsigned long	ret = MCU_OK;
	// Check for offline state...
	if( m_HidRead_hDrv == INVALID_HANDLE_VALUE ) 
		return MCU_OFFLINE;
	// If head and tail are == we are being polled faster than we are receiving packets...
	// Did not get a fresh packet.
	EnterCriticalSection(&m_csRead);
	if( m_RDQueue_head == m_RDQueue_tail ) 
		ret = MCU_RD_NOT_RECEIVED;
	else
	// Was there a ReadFile error?
	if( m_ReadThread_Error[m_RDQueue_tail] != MCU_OK ) 
		ret = m_ReadThread_Error[m_RDQueue_tail];
	// If all is ok, grab the current ques packet prior to releasing CS
	if( ret == MCU_OK ) {
		// Process the above most recent aka...WorkerThread buffer read and process...
		// Always stuff packet we processed, app has opportunity to watch...
		memcpy( Rcv_pkt, &m_Rcv_Packet[ m_RDQueue_tail ], sizeof(IDCOMMANDPKT) );
	}
	// Release CS...
	LeaveCriticalSection(&m_csRead);
	// Check results from above...
	if( ret != MCU_OK )
		return ret;
	// Is this the packet the application is looking for?
	// Check for valid commands...
	switch( Rcv_pkt->pkt.pktcmd ) {
		case COMMAND_FAILED: // The MCU is not able to process the command, something internally has failed (i2C?)
			switch( Rcv_pkt->pkt.pktdata.value ) {
				case TRACKER_RAW_DATA:
				case TRACKER_EULER_DATA:
				case TRACKER_QUATS_DATA:
					if( Rcv_pkt->pkt.pktcmd == Send_pkt->pkt.pktcmd ) 
						ret = MCU_TRACKER_FAILED;// The tracker is on however, the MCU fails to receive packets.
					else
						ret = MCU_RD_NOT_RECEIVED;// Not looking for tracker packet errors. ignore them...
				break;
				default:
					ret = MCU_COMMAND_FAILED;// A command was sent to the MCU and the MCU reports failure to execute.
				break;
			}
		break;
		case UNKNOWN_CMD: // An unknown command was sent to this MCU.
			ret = MCU_UNKNOWN;
		break;
		case INVALID_ARG: // An invalid argument for this MCU command was used.
			ret = MCU_INVALIDARG;
		break;
		default: // Could be Raw_data and the calling process is not looking for this type packet?
			if( Rcv_pkt->pkt.pktcmd == Send_pkt->pkt.pktcmd ) 
				ret = MCU_OK;
			else
				ret = MCU_RD_NOT_RECEIVED;
		break;
	}
	// Clear and Prep for next command recieved buffer fill...
	// Move Tail read index to next packet received...
	// Theory: We can never increment past the current _head index we are reading into...
	if( m_HidRead_hDrv == INVALID_HANDLE_VALUE ) 
		return MCU_OFFLINE;
	EnterCriticalSection(&m_csRead);
	m_RDQueue_tail++;
	if( m_RDQueue_tail >= m_RecievePacketQueSize ) 
		m_RDQueue_tail = 0; 
	// Provide means of sensing queue is not flushed...
	if( ret == MCU_RD_NOT_RECEIVED && m_RDQueue_head != m_RDQueue_tail ) 
		ret = MCU_RD_QUE_NOT_EMPTY;
	LeaveCriticalSection(&m_csRead);
	return ret;
}

//
// Used for Method #2,#3 logic applications not interested in windows messaging pipe to process Vuzix USB packets.
//	Command threads or foreground process thread is used to poll and process packets.
//
unsigned long	MCUSendCommand( PIDCOMMANDPKT Send_pkt, PIDCOMMANDPKT Rcv_pkt, int RetryCnt, int MSTimeout )
{
int				i;
unsigned long	ret = MCU_OK;
	if( Send_pkt == NULL ) 
		return MCU_INVALID_ARG1; // Do not allow Null send packet.
	if( Rcv_pkt == NULL ) 
		return MCU_INVALID_ARG2; // Do not allow Null recieve packet.
	if( RetryCnt <= 0 ) 
		return MCU_INVALID_ARG3; // Do not allow 0 RetryCounts.
	if( MSTimeout <= 0 ) 
		return MCU_INVALID_ARG4; // Do not allow 0-Ms on Timeout.
	// Continuous resends to MCU Device...
	ret = MCUSendIoPacket( Send_pkt, RetryCnt, MSTimeout );
	if( ret != MCU_OK )
		return ret; // Never sent the command out ok.
	// wait for responce or time-out...Fail testing...inform developer...
	for( i=0; i < m_DeviceTimeOut; i++ ) {
		// Receive the next packet the process is looking for...
		// Empty the que or stop at first occurance of requested cmd...
		for( ret=MCU_RD_QUE_NOT_EMPTY; ret == MCU_RD_QUE_NOT_EMPTY; ) 
			ret = MCUReceivePackets( Send_pkt, Rcv_pkt );
		if( ret != MCU_RD_NOT_RECEIVED &&
			ret != MCU_RD_QUE_NOT_EMPTY ) 
			return ret; // got a failure or proper responce...
		SleepEx( 1, TRUE ); // minimize the send/rcv round trip intercept time.
	}
	if( i == m_DeviceTimeOut )
		return MCU_SEND_TIMEOUT;// Device did not respond...
	return ret; // could be failing MCUSendIoPacket...
}

//
// Return the # of payload arguments for received commands.
//
int			MCUGetRcvCmdSize( unsigned char pktcmd )
{
	switch( pktcmd ) {
		case GET_VERSION:
			return sizeof( VERSIONPKT );
		case COMMAND_FAILED:
			// COMMAND_FAIL  may contain more than value as a return argument...
			return 2;
		case READ_I2C:
		case GET_SYSTEM_INIT_STATE:
		case GET_UPPER_VERSION:
		case GET_HW_VERSION:
		case GET_BOOT_VERSION:
		case GET_PRODUCT_ID:
		case GET_BAT_LEVEL:
		case TRACKER_GET_STATE:
		case INVALID_ARG:
		case UNKNOWN_CMD:
			return 1;
		case TRACKER_RAW_DATA:
			return 24; // Please check on this. as the raw tracker packet is changing daily.
		case TRACKER_QUATS_DATA:
			return sizeof( QUATSFORMATEDPKT );
		case TRACKER_EULER_DATA:
			return sizeof( EULERFORMATEDPKT );
		case GET_GYRO_ZERO:
		case DEBUG_MSG:
		case READMULTI_I2C:
		case CLEAR_GYRO_ZERO:
			return MAX_PKT_PAYLOAD;// crt: Looks these up in spec...not proper
		// Most commmands only return the CMD byte.
		default:
			return 0;
	}
}

//
// Return the # of payload arguments for send commands.
//
int			MCUGetSendCmdSize( unsigned char pktcmd )
{
	switch( pktcmd ) {
		case READ_I2C:
		case WRITE_I2C:
		case TRACKER_SET_REPORT_MODE:
		case SET_STEREO_MODE:
		case TRACKER_CALIBRATE:
			return 1;
		case WRITEMULTI_I2C:
		case READMULTI_I2C:
		case SET_GYRO_ZERO:
		case GET_GYRO_ZERO:
		case CLEAR_GYRO_ZERO:
			return MAX_PKT_PAYLOAD;// crt: Looks these up in spec...not proper
		// Most commmands only require the CMD byte.
		default:
			return 0;
	}
}

// 
// Returns a Humanly readable error string.
//	Send_pkt can be NULL
//	Rcv_pkt can be NULL
//
void	MCUProcessError( PIDCOMMANDPKT Send_pkt, PIDCOMMANDPKT Rcv_pkt, unsigned long ret, TCHAR *eStr, int szOfstr )
{
	switch( ret ) {
		case MCU_ALLOCATE_QUE_FAILED:
			swprintf_s( eStr, szOfstr, L"Allocation of recieve packet queue failed." );
		break;
		case MCU_QUE_SIZE_FAILURE:
			swprintf_s( eStr, szOfstr, L"Requested Queue size too small." );
		break;
		case MCU_ALLOCATE_ERRQUE_FAILED: 
			swprintf_s( eStr, szOfstr, L"Allocation of recieve packet error queue failed." );
		break;
		case MCU_INVALID_ARG1:
		case MCU_INVALID_ARG2: 
		case MCU_INVALID_ARG3: 
		case MCU_INVALID_ARG4: 
			swprintf_s( eStr, szOfstr, L"Invalid Argument was passed to function: (%d)...", ret );
		break;
		case MCU_OK:
			swprintf_s( eStr, szOfstr, L"MCU interface is ok..." );
		return;
		case MCU_IOBUSY:
			swprintf_s( eStr, szOfstr, L"Fail:IOBUSY USB device not resonding to USB messages..." );
		break;
		case MCU_WR_WRITEFAILED:
			swprintf_s( eStr, szOfstr, L"Fail:Write command did not write all bytes...\n   Could be a bug in firmware or the Device Handle has become invalid...Fix this?" );
		break;
		case MCU_WR_IOFAILURE:
			swprintf_s( eStr, szOfstr, L"Fail:Unable to send commands to firmware...\n   Could be a bug in firmware or the Device Handle has become invalid...Fix this?" );
		break;
		case MCU_RD_IOFAILURE:
			swprintf_s( eStr, szOfstr, L"Fail:Receiveing packets from firmware...\n   Could be a bug in firmware or the Device Handle has become invalid...Fix this?" );
		break;
		case MCU_RD_NOT_RECEIVED:
			if( Send_pkt ) 
				swprintf_s( eStr, szOfstr, L"Fail:Not Receiving command (%x)...\n   Could be a bug in firmware or the Device Handle has become invalid...Fix this?", Send_pkt->pkt.pktcmd );
			else
				swprintf_s( eStr, szOfstr, L"Fail:Not Receiving command...\n   Could be a bug in firmware or the Device Handle has become invalid...Fix this?");
		break;
		case MCU_INVALIDARG:
			if( Rcv_pkt ) 
				swprintf_s( eStr, szOfstr, L"Fail:Invalid Argument for command sent to firmware Sent Cmd(%x) Rcved Cmd (%x) Arguments Received 0=(%x) 1=(%x)...\n   Could be a bug in firmware or missing command for this device...", Send_pkt->pkt.pktcmd, Rcv_pkt->pkt.pktcmd, Rcv_pkt->pkt.pktdata.value, Rcv_pkt->pkt.pktdata.payload[1] );
			else
			if( Send_pkt ) 
				swprintf_s( eStr, szOfstr, L"Fail:Invalid Argument for command sent to firmware Sent Cmd(%x)\n   Could be a bug in firmware or missing command for this device...", Send_pkt->pkt.pktcmd );
			else
				swprintf_s( eStr, szOfstr, L"Fail:Invalid Argument for command sent to firmware\n   Could be a bug in firmware or missing command for this device...");
		break;
		case MCU_COMMAND_FAILED:
			if( Send_pkt ) 
				swprintf_s( eStr, szOfstr, L"Fail:Command sent to MCU failed(%x)...\n   Electronics must be plugged in and powered up, Check command is supported for this device...", Send_pkt->pkt.pktcmd );
			else
				swprintf_s( eStr, szOfstr, L"Fail:Command sent to MCU failed...\n   Electronics must be plugged in and powered up, Check command is supported for this device..." );
		break;
		case MCU_UNKNOWN:
			if( Send_pkt ) 
				swprintf_s( eStr, szOfstr, L"Fail:Unknown command sent to firmware(%x)...\n   Could be a bug in firmware or missing command for this device...", Send_pkt->pkt.pktcmd );
			else
				swprintf_s( eStr, szOfstr, L"Fail:Unknown command sent to firmware...\n   Could be a bug in firmware or missing command for this device..." );
		break;
		case MCU_SEND_TIMEOUT:
			if( Send_pkt ) 
				swprintf_s( eStr, szOfstr, L"Fail:Did not receive command responce, sent cmd(%x)...\n   Device could be offline or timeout exceeds (%d)-MS...", Send_pkt->pkt.pktcmd, m_DeviceTimeOut );
			else
				swprintf_s( eStr, szOfstr, L"Fail:Did not receive command responce, sent cmd...\n   Device could be offline or timeout exceeds (%d)-MS...", m_DeviceTimeOut );
		break;
		case MCU_DEVICE_IN_STARTUP:
			swprintf_s( eStr, szOfstr, L"Fail:Device is in startup phase..." );
		break;
		case MCU_OFFLINE:
			swprintf_s( eStr, szOfstr, L"Fail:Interface appears to be offline...\n   Device could be offline or firmware is not responding..." );
		break;
		default:
			if( Send_pkt ) 
				swprintf_s( eStr, szOfstr, L"Fail:Unknown error or error not processed(%d) from send command(%x)...", ret, Send_pkt->pkt.pktcmd );
			else
				swprintf_s( eStr, szOfstr, L"Fail:Unknown error or error not processed(%d) from send command...", ret );
		break;
	}
}
