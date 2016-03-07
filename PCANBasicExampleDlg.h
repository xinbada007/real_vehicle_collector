
// PCANBasicExampleDlg.h : header file
//

#pragma once

// Includes
//
#include "afxwin.h"
#include "afxcmn.h"

#include <boost/asio/serial_port.hpp> 
#include <boost/asio.hpp> 
#include <boost/bind.hpp>

#include "PCANBasicClass.h"

#include <Math.h>
#include <bitset>
#include <vector>
#include <utility>

using namespace std;

// Constant values to make the code more comprehensible
// to add and read items from the List view
//
#define MSG_TYPE		0
#define MSG_ID			1
#define MSG_LENGTH		2
#define MSG_COUNT		3
#define MSG_TIME		4
#define MSG_DATA		5

#define GPS_DATA			0
#define GPS_SATS			1
#define GPS_TIME			2
#define GPS_LATITUDE		3
#define GPS_LONGITUDE		1
#define GPS_SPEED_KNOTS		2
#define GPS_HEADING			3
#define GPS_ALTITUDE_WGS	1
#define GPS_Vertical_V		2
#define GPS_DGPS			3

#define GPS_NUM_COUNT		1
#define GPS_DATA_COUNT		5
#define GPS_SEND_NUM_COUNT  1	//4

#define GPS_MSG_NUMS		950000
#define XBOW_MSG_NUMS		300000

#define GPS_MK_PAIR(x,y)	std::pair<CString,CString>(x,y)

#define CONNECTION_ERROR	1
#define GPS_LATENCY			1
#define XBOW_LATENCY		1

#define FT232SN				"FTU7GDEE"

const boost::posix_time::ptime time_t_epoch(boost::gregorian::date(2015, 8, 31));

// Critical Section class for thread-safe menbers access
//
#pragma region Critical Section Class
class clsCritical
{
	private:
		CRITICAL_SECTION *m_objpCS;
		LONG volatile m_dwOwnerThread;
		LONG volatile m_dwLocked;
		bool volatile m_bDoRecursive;

	public:
		explicit clsCritical(CRITICAL_SECTION *cs, bool createUnlocked = false, bool lockRecursively = false);
        ~clsCritical();
		int GetRecursionCount();
		bool IsLocked();
		int Enter();
		int Leave();
};
#pragma endregion

// Message Status structure used to show CAN Messages
// in a ListView
//
#pragma region Message Status class
class MessageStatus
{
	private:
		TPCANMsgFD m_Msg;
		TPCANTimestampFD m_TimeStamp;		
		TPCANTimestampFD m_oldTimeStamp;		
		int m_iIndex;
		int m_Count;
        bool m_bShowPeriod;
        bool m_bWasChanged;

	public:
		MessageStatus(TPCANMsgFD canMsg, TPCANTimestampFD canTimestamp, int listIndex);
		void Update(TPCANMsgFD canMsg, TPCANTimestampFD canTimestamp);

		TPCANMsgFD GetCANMsg();
		TPCANTimestampFD GetTimestamp();
		int GetPosition();
		CString GetTypeString();
		CString GetIdString();
		CString GetDataString();
		CString GetTimeString();
		int GetCount();
		bool GetShowingPeriod();
		bool GetMarkedAsUpdated();

		void SetShowingPeriod(bool value);
		void SetMarkedAsUpdated(bool value);

		__declspec(property (get = GetCANMsg)) TPCANMsgFD CANMsg;
        __declspec(property (get = GetTimestamp)) TPCANTimestampFD Timestamp;
        __declspec(property (get = GetPosition)) int Position;
        __declspec(property (get = GetTypeString)) CString TypeString;
        __declspec(property (get = GetIdString)) CString IdString;
        __declspec(property (get = GetDataString)) CString DataString;
		__declspec(property (get = GetTimeString)) CString TimeString;
        __declspec(property (get = GetCount)) int Count;
        __declspec(property (get = GetShowingPeriod, put = SetShowingPeriod)) bool ShowingPeriod;
        __declspec(property (get = GetMarkedAsUpdated, put = SetMarkedAsUpdated)) bool MarkedAsUpdated;		
};
#pragma endregion

// PCANBasicExampleDlg dialog
class CPCANBasicExampleDlg : public CDialog
{
// Construction
public:
	CPCANBasicExampleDlg(CWnd* pParent = NULL);	// standard constructor

// Dialog Data
	enum { IDD = IDD_PCANBASICEXAMPLE_DIALOG };

	protected:
	virtual void DoDataExchange(CDataExchange* pDX);	// DDX/DDV support

	// Connection
	//
	CComboBox cbbChannel;
	CButton btnRefresh;
	CComboBox cbbBaudrates;
	CComboBox cbbHwsType;
	CComboBox cbbIO;
	CComboBox cbbInterrupt;
	CButton btnInit;
	CButton btnRelease;
	CButton chbCanFD;
	CString txtBitrate;

	// Filtering
	CButton chbFilterExtended;
	CButton rdbFilterOpen;
	CButton rdbFilterClose;
	CButton rdbFilterCustom;
	CString txtFilterFrom;
	CString txtFilterTo;
	CSpinButtonCtrl nudFilterFrom;
	CSpinButtonCtrl nudFilterTo;
	CButton btnFilterApply;
	CButton btnFilterQuery;

	//Parameter
	CComboBox cbbParameter;
	CButton rdbParameterActive;
	CButton rdbParameterInactive;
	CEdit editParameterDevNumber;
	CString txtParameterDevNumber;
	CSpinButtonCtrl nudParameterDevNumber;
	CButton btnParameterGet;
	CButton btnParameterSet;

	//Reading
	CButton rdbReadingTimer;
	CButton rdbReadingEvent;
	CButton rdbReadingManual;
	CButton chbReadingTimeStamp;
	CButton btnRead;
	CButton btnReadingClear;
	CListCtrl lstMessages, lstMessages_GPS;

	//Information
	CListBox listBoxInfo;
	CButton btnStatus;
	CButton btnReset;
	CButton btnVersions;	

	// Event functions
	//
	void InitializeControls(void);
	afx_msg void OnTimer(UINT_PTR nIDEvent);
	afx_msg void OnCbnSelchangecbbChannel();
	afx_msg void OnCbnSelchangeCbbbaudrates();
	afx_msg void OnLbnDblclkListinfo();
	afx_msg void OnNMDblclkLstmessages(NMHDR *pNMHDR, LRESULT *pResult);
	afx_msg void OnBnClickedBtninit();
	afx_msg void OnBnClickedBtnrelease();
	afx_msg void OnShowWindow(BOOL bShow, UINT nStatus);
	afx_msg void OnBnClickedRdbtimer();
	afx_msg void OnBnClickedRdbevent();
	afx_msg void OnBnClickedChbtimestamp();
	afx_msg void OnBnClickedButtonHwrefresh();
	afx_msg void OnCbnSelchangeCbbhwstype();
	afx_msg void OnBnClickedChbfilterextended();
	afx_msg void OnDeltaposNudfilterfrom(NMHDR *pNMHDR, LRESULT *pResult);
	afx_msg void OnEnKillfocusTxtfilterfrom();
	afx_msg void OnDeltaposNudfilterto(NMHDR *pNMHDR, LRESULT *pResult);
	afx_msg void OnEnKillfocusTxtfilterto();
	afx_msg void OnBnClickedButtonfilterapply();
	afx_msg void OnBnClickedButtonfilterquery();
	afx_msg void OnBnClickedRdbmanual();
	afx_msg void OnBnClickedButtonread();
	afx_msg void OnBnClickedButtonreadingclear();
	afx_msg void OnCbnSelchangeComboparameter();
	afx_msg void OnDeltaposNudparamdevnumber(NMHDR *pNMHDR, LRESULT *pResult);
	afx_msg void OnEnKillfocusTxtparamdevnumber();
	afx_msg void OnBnClickedButtonparamset();
	afx_msg void OnBnClickedButtonparamget();
	afx_msg void OnClose();
	afx_msg void OnBnClickedButtonversion();
	afx_msg void OnBnClickedButtoninfoclear();
	afx_msg void OnBnClickedButtonstatus();
	afx_msg void OnBnClickedButtonreset();
	afx_msg void OnBnClickedChbfcanfd();

// Implementation
protected:
	HICON m_hIcon;

	// Generated message map functions
	virtual BOOL OnInitDialog();
	afx_msg void OnPaint();
	afx_msg HCURSOR OnQueryDragIcon();
	DECLARE_MESSAGE_MAP()


private:
	// ------------------------------------------------------------------------------------------
	// Private Members
	// ------------------------------------------------------------------------------------------
	// Variables to store the current PCANBasic instance
	//
	bool m_Terminated;
	PCANBasicClass *m_objPCANBasic;

    // Saves the desired connection mode
    //
    bool m_IsFD;

	// Saves the handle of a PCAN hardware
	//
	TPCANHandle m_PcanHandle;

	// Saves the baudrate register for a conenction
	//
	TPCANBaudrate m_Baudrate;

	// Saves the type of a non-plug-and-play hardware
	//
	TPCANType m_HwType;

	// Variables to store the current reading mode
	// 0 : Timer Mode
	// 1 : Event Mode
	// 2 : Manual Mode
	//
	int m_ActiveReadingMode;

	// Read Timer identifier
	//
	UINT_PTR m_tmrRead;

	//Display Timer identifier
	//
	UINT_PTR m_tmrDisplay;

	// CAN messages array. Store the message status for its display
	//
	CPtrList *m_LastMsgsList;

	// Handle to set Received-Event
	//
	HANDLE m_hEvent;

	// Handle to the thread to read using Received-Event method
	//
	HANDLE m_hThread;

	// Handles of the current available PCAN-Hardware
	//
	TPCANHandle m_HandlesArray[59];
	// Handle for a Critical Section 
	//
	CRITICAL_SECTION *m_objpCS;

	// ------------------------------------------------------------------------------------------
	// Help functions
	// ------------------------------------------------------------------------------------------
	// Convert a int value to a CString
	//
	CString IntToStr(int iValue);
	// Convert a int value to a CString formated in Hexadecimal
	//
	CString IntToHex(int iValue, short iDigits);
	// Convert hexadecimal Cstring into int value (Zero if error)
	//
	DWORD HexTextToInt(CString ToConvert);
	// Check txtData in an hexadecimal value
	//
	void CheckHexEditBox(CString* txtData);
	// Enables/Disables Data text boxes according with a given length
	//
	int AddLVItem(CString Caption);
	int ADDLVItem_GPS(CString Caption);
	// Enable/Disable Read Timer
	//
	void SetTimerRead(bool bEnable);
	// Enable/Disable Display Timer
	//
	void SetTimerDisplay(bool bEnable);
	// Configures the data of all ComboBox components of the main-form
	//
	void FillComboBoxData();
	// Gets the formated text for a PCAN-Basic channel handle
	//
	CString FormatChannelName(TPCANHandle handle, bool isFD);
	// Gets the formated text for a PCAN-Basic channel handle
	//
	CString FormatChannelName(TPCANHandle handle);
	// Gets the name for a PCAN-Basic channel handle
	//
	CString GetTPCANHandleName(TPCANHandle handle);
	// Help Function used to get an error as text
	//
	CString GetFormatedError(TPCANStatus error);
	//Activates/deaactivates the different controls of the main-form according
	//with the current connection status
	//
	void SetConnectionStatus(bool bConnected);
	// Gets the current status of the PCAN-Basic message filter
	//
	bool GetFilterStatus(int* status);
	// Includes a new line of text into the information Listview
	//
	void IncludeTextMessage(CString strMsg);
	// Gets ComboBox selected label
	// 
	CString GetComboBoxSelectedLabel(CComboBox* ccb);
	// Configures the Debug-Log file of PCAN-Basic
	//
	void ConfigureLogFile();
	// Configures the PCAN-Trace file for a PCAN-Basic Channel
	//
	void ConfigureTraceFile();

	// ------------------------------------------------------------------------------------------
	// Message-proccessing functions
	// ------------------------------------------------------------------------------------------
	// Display CAN messages in the Message-ListView
	//
	void DisplayMessages();
	// Create new MessageStatus using provided parameters
	//
	void InsertMsgEntry(TPCANMsgFD NewMsg, TPCANTimestampFD MyTimeStamp);
	// Processes a received message, in order to show it in the Message-ListView
	//
	void ProcessMessage(TPCANMsgFD theMsg, TPCANTimestampFD itsTimeStamp);
	void ProcessMessage(TPCANMsg MyMsg, TPCANTimestamp MyTimeStamp);
	// static Thread function to manage reading by event
	//
	static DWORD WINAPI CallCANReadThreadFunc(LPVOID lpParam);
	// member Thread function to manage reading by event
	//
	DWORD WINAPI CANReadThreadFunc(LPVOID lpParam);
	// Manage Reading method (Timer, Event or manual)
	//
	void ReadingModeChanged();
	// Functions for reading PCAN-Basic messages
	//
	TPCANStatus ReadMessageFD();
	TPCANStatus ReadMessage();
	void ReadMessages();

	// Critical section Ini/deinit functions
	//
	void InitializeProtection();
	void FinalizeProtection();

	/*============================================================*/
	//Custom GPS
	void DisplayGPSInformation(CString GPS, int iCurrentItem_GPS, int ID_NUM, bool WriteEnable = false);
	unsigned HexTextToUnsigned(CString ToConvert);
	CString UnsignedToBinString(const std::bitset<8> &bitw);
	CString FormatTimeString(unsigned hour, unsigned minute, unsigned seconds, unsigned remainder);
	void GetGPSXbowInformation(CString Data, int ID_NUM);

	void StoreMsgList();
	void WriteGPSFile();
// 	void ComUninitialize();
	void StoreAccMsgList();
	void WriteAccFile();

	void InitGPSConfig();
	void InitCrossXbow();
	void EnumComPorts();
	void ReadPortsCallback(bool *isread, const boost::system::error_code& error, const unsigned &bytes_transferred, boost::asio::deadline_timer *timer);
	void ConnectCrossXbow();
	static DWORD WINAPI CallReadXbowDataThreadFunc(LPVOID lpParam);
	DWORD WINAPI XbowDataReadThreadFunc(LPVOID lpParam);

	std::string m_GPS_Recorder;
	std::string m_GPS_Str_Time, m_GPS_Str_X, m_GPS_Str_Y, m_GPS_Str_Sats;
	std::string m_GPS_Str_Velocity, m_GPS_Str_Heading, m_GPS_Str_WGS84, m_GPS_Str_VerticalV;
	std::string m_GPS_Str_TrigDist,m_GPS_Str_LongAcc,m_GPS_Str_LatACC,m_GPS_Str_Status;
	std::string m_GPS_Str_TrigTime, m_GPS_Str_TrigV, m_GPS_Str_Distance;
	std::vector<string> m_GPS_Str_RAW;
	int m_GPS_Num_Count;
	double m_GPS_First_Distance;
	double m_GPS_Distance_Offset;

	/*===================================================================================*/
	//WILL be read & written by different threads, so be extremely careful, have to be
	//locked each time of using
	//
	std::vector<std::pair<const CString, const CString>> m_GPS_Msg_List;
	std::vector<const CString> m_Xbow_Msg_List;
	CString m_Xbow_Msg_Latest;
	std::string m_Xbow_Msg_TobeSent;
	unsigned m_Xbow_Msg_Sent_Num;
	unsigned m_GPS_Begin_Time;
	unsigned m_GPS_Is_First;
	std::vector<uint64_t> m_GPS_CPU_Time;
	std::vector<__int64> m_Xbow_CPU_Time;

	std::string m_GPS_Msg_TobeSent;
	std::string m_Msg_ReadytoGo;
	CString m_GPS_Msg_Latest;
	unsigned m_GPS_Msg_Sent_Num;
	
	unsigned m_GPS_Send_Num_Count;
	unsigned m_Xbow_Count;
	unsigned m_Xbow_Effictive_Count;
	unsigned m_Xbow_Algin;
	//WILL be read & written by different threads, so be extremely careful, have to be
	//locked each time of using
	/*===================================================================================*/

	static DWORD WINAPI CallNetworkOutputFunc(LPVOID lpParam);
	DWORD WINAPI NetworkOutFunc(LPVOID lpParam);
	void NetworkWriteHandler(boost::asio::deadline_timer *timer, const boost::system::error_code &ec, unsigned bytestransferred);
	void NetworkAccpetorHandler(boost::asio::ip::tcp::socket *socket, boost::asio::deadline_timer *timer, const boost::system::error_code &ec);
	void SendTimeOut(boost::asio::ip::tcp::socket *socket, const boost::system::error_code &ec);
	void ConnectionTimeOut(boost::asio::ip::tcp::socket *socket, bool *isconnected, const boost::system::error_code &ec);
	HANDLE m_Network_hThread;
	HANDLE m_GPS_Net_Event;
	HANDLE m_Xbow_Net_Event;
	HANDLE m_Send_Event;
	unsigned m_Send_Terminated;

	std::string m_Xbow_Recorder;

	std::string m_Xbow_RollAngle, m_Xbow_PitchAngle, m_Xbow_RollRate, m_Xbow_PitchRate;
	std::string m_Xbow_YawRate, m_Xbow_AccX, m_Xbow_AccY, m_Xbow_AccZ, m_Xbow_Tem, m_Xbow_Time;
	std::string m_Xbow_Badratio;

	bool m_Xbow_AddedItem;
	bool m_Xbow_Added2Item;

	HANDLE m_Xbow_hThread;
	unsigned m_XbowTerminated;
	std::string m_CurrentComPort;
	CRITICAL_SECTION m_xBow_CriticalSection;

	double m_Shutter_Time;
	double m_Shutter_Time_Constant;
	boost::posix_time::ptime m_Shutter_CPUTIME_BEGIN;
	long double m_Shutter_Duration;
	std::vector<std::pair<__int64,std::string>> m_Shutter_Time_Rec;

	double m_LaneChange_Time;
	double m_LaneChange_Time_Constant;
	double m_Shutter_Offset;

	void SetShutterGlass(const bool swt = true);
	void WriteShutterGlass();
	bool FT232RCOM_Exist(char SN[]);

	bool SubsExist();
	bool TrialsExist();

public:				
	CStatic labelHwType;
	CComboBox ccbHwXbow;
	CStatic labelIOPort;
	CStatic labelInterrupt;
	afx_msg void OnBnClickedButtonHwrefresh2();
	CButton btnRefreshCom;
	afx_msg void OnBnClickedButtonreset2();
	afx_msg void OnBnClickedButtonfilterquery2();
	CButton btnSend;
	CString txtGlass;
	afx_msg void OnBnClickedButtonglass();
	CEdit txtCtrlGlass;
	CButton btnShutterApply;
	afx_msg void OnBnClickedButtonsw();
	afx_msg void OnBnClickedButtonsw2();
	CButton btnSetGlassOFF;
	CButton btnSetGlassON;
	CString txtFilterSubs;
	CString txtFilterTrials;
	afx_msg void OnDeltaposNudfiltersubs(NMHDR *pNMHDR, LRESULT *pResult);
	CSpinButtonCtrl nudFilterSubs;
	CSpinButtonCtrl nudFilterTrials;
	afx_msg void OnDeltaposNudfiltertrials(NMHDR *pNMHDR, LRESULT *pResult);
	CEdit txtCtrlLaneChange;
	CString txtLaneChange;
	CEdit txtLC2;
	CString txtLC2Value;
	CButton btnApplySub;
	afx_msg void OnBnClickedButtonsub();
	CEdit TXTZERO;
	CString ValueZero;
	CEdit TXTERATE;
	CString ValueErate;
	afx_msg void OnBnClickedButtonzero();
	CButton btnZero;

	int m_zerotime;
	int m_erate;
	afx_msg void OnBnClickedButtonerate();
	CButton btnerate;
	CEdit mTXTOffset;
	CString mValueOffset;
	CSpinButtonCtrl mNUDOffset;
	afx_msg void OnDeltaposNudfilteroffset(NMHDR *pNMHDR, LRESULT *pResult);
};