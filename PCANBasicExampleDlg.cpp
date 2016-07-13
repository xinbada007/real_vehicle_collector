MessageBox

// PCANBasicExampleDlg.cpp : implementation file
//

#include "stdafx.h"
#include "PCANBasicExample.h"
#include "PCANBasicExampleDlg.h"

#include <sstream>
#include <iomanip>
#include <fstream>
#include <boost/filesystem/operations.hpp>
#include <boost/filesystem/path.hpp>
#include <mmsystem.h>

#include <ftd2xx.h>
#include <FTChipID.h>

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

//////////////////////////////////////////////////////////////////////////////////////////////
// CriticalSection class
//
#pragma region Critical Section Class
clsCritical::clsCritical(CRITICAL_SECTION *cs, bool createUnlocked, bool lockRecursively)
{
	ASSERT(cs != NULL);	
	
	m_objpCS = cs;
	m_dwLocked = -1;
	m_bDoRecursive = lockRecursively;
	m_dwOwnerThread = GetCurrentThreadId();

	if(!createUnlocked)
		Enter();
}	

clsCritical::~clsCritical()
{
	int iFail = (int)0x80000000;

	while(m_dwLocked >= 0)
		if(Leave() == iFail) 
			break;
}

int clsCritical::Enter()
{
	if(m_dwOwnerThread != GetCurrentThreadId())
		throw "class clsCritical: Thread cross-over error. ";

	try
	{
		if(m_bDoRecursive || (m_dwLocked == -1))
		{
			EnterCriticalSection(m_objpCS);
			InterlockedIncrement(&m_dwLocked);
		}
		return m_dwLocked;
	}
	catch(...)
	{	
		return 0x80000000;
	}
}

int clsCritical::Leave()
{
	if(m_dwOwnerThread != GetCurrentThreadId())
		throw "class clsCritical: Thread cross-over error. ";

	try
	{
		if(m_dwLocked >= 0)
		{
			LeaveCriticalSection(m_objpCS);
			InterlockedDecrement(&m_dwLocked);
			return m_dwLocked;
		}
		return -1;
	}
	catch(...)
	{
		return 0x80000000;
	}
}

bool clsCritical::IsLocked()
{
	return (m_dwLocked > -1);
}

int clsCritical::GetRecursionCount()
{
	return m_dwLocked;
}
#pragma endregion

/// <summary>
/// Convert a CAN DLC value into the actual data length of the CAN/CAN-FD frame.
/// </summary>
/// <param name="dlc">A value between 0 and 15 (CAN and FD DLC range)</param>
/// <param name="isSTD">A value indicating if the msg is a standard CAN (FD Flag not checked)</param>
/// <returns>The length represented by the DLC</returns>
static int GetLengthFromDLC(int dlc, bool isSTD)
{
    if (dlc <= 8)
        return dlc;

     if (isSTD)
        return 8;

     switch (dlc)
     {
        case 9: return 12;
        case 10: return 16;
        case 11: return 20;
        case 12: return 24;
        case 13: return 32;
        case 14: return 48;
        case 15: return 64;
        default: return dlc;
    }
}


//////////////////////////////////////////////////////////////////////////////////////////////
// MessageStatus class
//
#pragma region Message Status class
MessageStatus::MessageStatus(TPCANMsgFD canMsg, TPCANTimestampFD canTimestamp, int listIndex)
{
    m_Msg = canMsg;
    m_TimeStamp = canTimestamp;
    m_oldTimeStamp = canTimestamp;
    m_iIndex = listIndex;
    m_Count = 1;
    m_bShowPeriod = true;
	m_bWasChanged = false;
}

void MessageStatus::Update(TPCANMsgFD canMsg, TPCANTimestampFD canTimestamp)
{
    m_Msg = canMsg;
    m_oldTimeStamp = m_TimeStamp;
    m_TimeStamp = canTimestamp;
    m_bWasChanged = true;
    m_Count += 1;
}

TPCANMsgFD MessageStatus::GetCANMsg()
{
	return m_Msg;
}

TPCANTimestampFD MessageStatus::GetTimestamp()
{
	return m_TimeStamp;
}

int MessageStatus::GetPosition()
{
	return m_iIndex;
}

CString MessageStatus::GetTypeString()
{
	CString strTemp;

	// Add the new ListView Item with the type of the message
	//
    if ((m_Msg.MSGTYPE & PCAN_MESSAGE_STATUS) != 0)
        return "STATUS";

	if((m_Msg.MSGTYPE & PCAN_MESSAGE_EXTENDED) != 0)
		strTemp = "EXT";
	else
		strTemp = "STD";

	if((m_Msg.MSGTYPE & PCAN_MESSAGE_RTR) == PCAN_MESSAGE_RTR)
		strTemp = (strTemp + "/RTR");
	else
		if(m_Msg.MSGTYPE > PCAN_MESSAGE_EXTENDED)
		{
			strTemp.Append(" [ ");
            if (m_Msg.MSGTYPE & PCAN_MESSAGE_FD)
                strTemp.Append(" FD");
            if (m_Msg.MSGTYPE & PCAN_MESSAGE_BRS)
                strTemp.Append(" BRS");
            if (m_Msg.MSGTYPE & PCAN_MESSAGE_ESI)
                strTemp.Append(" ESI");
            strTemp.Append(" ]");
		}

	return strTemp;
}

CString MessageStatus::GetIdString()
{
	CString strTemp;

	// We format the ID of the message and show it
	//
	if((m_Msg.MSGTYPE & PCAN_MESSAGE_EXTENDED) != 0)
		strTemp.Format("%08Xh",m_Msg.ID);
	else
		strTemp.Format("%03Xh",m_Msg.ID);

	return strTemp;
}

CString MessageStatus::GetDataString()
{
	CString strTemp, strTemp2;

	strTemp = "";
	strTemp2 = "";

	if((m_Msg.MSGTYPE & PCAN_MESSAGE_RTR) == PCAN_MESSAGE_RTR)
		return "Remote Request";
	else
		for(int i=0; i < GetLengthFromDLC(m_Msg.DLC, !(m_Msg.MSGTYPE & PCAN_MESSAGE_FD)); i++)
		{
			strTemp.Format("%s %02X", strTemp2, m_Msg.DATA[i]);
			strTemp2 = strTemp;
		}

	return strTemp2;
}

CString MessageStatus::GetTimeString()
{
	double fTime;
	CString str;

    fTime = (m_TimeStamp / 1000.0);
    if (m_bShowPeriod)
        fTime -= (m_oldTimeStamp / 1000.0);
	str.Format("%.1f", fTime);

	return str;
}

int MessageStatus::GetCount()
{
	return m_Count;
}

bool MessageStatus::GetShowingPeriod()
{
	return m_bShowPeriod;
}

bool MessageStatus::GetMarkedAsUpdated()
{
	return m_bWasChanged;
}

void MessageStatus::SetShowingPeriod(bool value)
{
    if (m_bShowPeriod ^ value)
    {
        m_bShowPeriod = value;
        m_bWasChanged = true;
    }
}

void MessageStatus::SetMarkedAsUpdated(bool value)
{
	m_bWasChanged = value;
}
#pragma endregion

//////////////////////////////////////////////////////////////////////////////////////////////
// PCANBasicExampleDlg dialog
//
CPCANBasicExampleDlg::CPCANBasicExampleDlg(CWnd* pParent)
	: CDialog(CPCANBasicExampleDlg::IDD, pParent), txtBitrate(_T(""))
	, txtGlass(_T(""))
	, txtFilterSubs(_T(""))
	, txtFilterTrials(_T(""))
	, txtLaneChange(_T(""))
	, txtLC2Value(_T(""))
	, ValueZero(_T(""))
	, ValueErate(_T(""))
	, mValueOffset(_T(""))
{
	m_hIcon = AfxGetApp()->LoadIcon(IDR_MAINFRAME);
}

void CPCANBasicExampleDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);

	DDX_Control(pDX, IDC_CBBCHANNEL, cbbChannel);
	DDX_Control(pDX, IDC_BUTTON_HWREFRESH, btnRefresh);
	DDX_Control(pDX, IDC_CBBBAUDRATES, cbbBaudrates);
	DDX_Control(pDX, IDC_CBBHWSTYPE, cbbHwsType);
	DDX_Control(pDX, IDC_CBBIO, cbbIO);
	DDX_Control(pDX, IDC_CBBINTERRUPT, cbbInterrupt);
	DDX_Control(pDX, IDC_BTNINIT, btnInit);
	DDX_Control(pDX, IDC_BTNRELEASE, btnRelease);

	DDX_Control(pDX, IDC_CHBFILTEREXTENDED, chbFilterExtended);
	DDX_Control(pDX, IDC_RADIOFILTEROPEN, rdbFilterOpen);
	DDX_Control(pDX, IDC_RADIOFILTERCLOSE, rdbFilterClose);
	DDX_Control(pDX, IDC_RADIOFILTERCUSTOM, rdbFilterCustom);
	DDX_Control(pDX, IDC_NUDFILTERFROM, nudFilterFrom);
	DDX_Text(pDX, IDC_TXTFILTERFROM, txtFilterFrom);
	DDX_Control(pDX, IDC_NUDFILTERTO, nudFilterTo);
	DDX_Text(pDX, IDC_TXTFILTERTO, txtFilterTo);
	DDX_Control(pDX, IDC_BUTTONFILTERAPPLY, btnFilterApply);
	DDX_Control(pDX, IDC_BUTTONFILTERQUERY, btnFilterQuery);

	DDX_Control(pDX, IDC_COMBOPARAMETER, cbbParameter);
	DDX_Control(pDX, IDC_RADIOPARAMACTIVE, rdbParameterActive);
	DDX_Control(pDX, IDC_RADIOPARAMINACTIVE, rdbParameterInactive);
	DDX_Control(pDX, IDC_TXTPARAMDEVNUMBER, editParameterDevNumber);
	DDX_Text(pDX, IDC_TXTPARAMDEVNUMBER, txtParameterDevNumber);
	DDX_Control(pDX, IDC_NUDPARAMDEVNUMBER, nudParameterDevNumber);
	DDX_Control(pDX, IDC_BUTTONPARAMSET, btnParameterSet);
	DDX_Control(pDX, IDC_BUTTONPARAMGET, btnParameterGet);

	DDX_Control(pDX, IDC_RDBTIMER, rdbReadingTimer);
	DDX_Control(pDX, IDC_RADIO_BY_EVENT, rdbReadingEvent);
	DDX_Control(pDX, IDC_RDBMANUAL, rdbReadingManual);
	DDX_Control(pDX, IDC_CHBTIMESTAMP, chbReadingTimeStamp);
	DDX_Control(pDX, IDC_BUTTONREAD, btnRead);
	DDX_Control(pDX, IDC_BUTTONREADINGCLEAR, btnReadingClear);

	DDX_Control(pDX, IDC_LISTINFO, listBoxInfo);
	DDX_Control(pDX, IDC_BUTTONSTATUS, btnStatus);
	DDX_Control(pDX, IDC_BUTTONRESET, btnReset);

	DDX_Control(pDX, IDC_LSTMESSAGES, lstMessages);
	DDX_Control(pDX, IDC_LSTMESSAGES2, lstMessages_GPS);
	DDX_Control(pDX, IDC_BUTTONVERSION, btnVersions);
	DDX_Control(pDX, IDC_CHBCANFD, chbCanFD);
	DDX_Text(pDX, IDC_TXTBITRATE, txtBitrate);
	DDX_Control(pDX, IDC_LAHWTYPE, labelHwType);
	DDX_Control(pDX, IDC_CBBCHANNEL2, ccbHwXbow);
	DDX_Control(pDX, IDC_LAIOPORT, labelIOPort);
	DDX_Control(pDX, IDC_LAINTERRUPT, labelInterrupt);
	DDX_Control(pDX, IDC_BUTTON_HWREFRESH2, btnRefreshCom);
	DDX_Control(pDX, IDC_BUTTONFILTERQUERY2, btnSend);
	DDX_Text(pDX, IDC_SHUTTERGLASS, txtGlass);
	DDX_Control(pDX, IDC_SHUTTERGLASS, txtCtrlGlass);
	DDX_Control(pDX, IDC_BUTTONGLASS, btnShutterApply);
	DDX_Control(pDX, IDC_BUTTONSW, btnSetGlassOFF);
	DDX_Control(pDX, IDC_BUTTONSW2, btnSetGlassON);
	DDX_Text(pDX, IDC_TXTSUBS, txtFilterSubs);
	DDX_Text(pDX, IDC_TXTTRIALS, txtFilterTrials);
	DDX_Control(pDX, IDC_NUDFILTERSUBS, nudFilterSubs);
	DDX_Control(pDX, IDC_NUDFILTERTRIALS, nudFilterTrials);
	DDX_Control(pDX, IDC_LANECHANGE, txtCtrlLaneChange);
	DDX_Text(pDX, IDC_LANECHANGE, txtLaneChange);
	DDX_Control(pDX, IDC_LANECHANGE2, txtLC2);
	DDX_Text(pDX, IDC_LANECHANGE2, txtLC2Value);
	DDX_Control(pDX, IDC_BUTTONSUB, btnApplySub);
	DDX_Control(pDX, IDC_TXTZERO, TXTZERO);
	DDX_Text(pDX, IDC_TXTZERO, ValueZero);
	DDX_Control(pDX, IDC_TXTERATE, TXTERATE);
	DDX_Text(pDX, IDC_TXTERATE, ValueErate);
	DDX_Control(pDX, IDC_BUTTONZERO, btnZero);
	DDX_Control(pDX, IDC_BUTTONERATE, btnerate);
	DDX_Control(pDX, IDC_TXTOFFSET, mTXTOffset);
	DDX_Text(pDX, IDC_TXTOFFSET, mValueOffset);
	DDX_Control(pDX, IDC_NUDFILTEROFFSET, mNUDOffset);
}

BEGIN_MESSAGE_MAP(CPCANBasicExampleDlg, CDialog)
ON_WM_PAINT()
	ON_WM_QUERYDRAGICON()
	ON_WM_TIMER()
	ON_CBN_SELCHANGE(IDC_CBBCHANNEL, OnCbnSelchangecbbChannel)
	ON_CBN_SELCHANGE(IDC_CBBBAUDRATES, OnCbnSelchangeCbbbaudrates)
	ON_NOTIFY(NM_DBLCLK, IDC_LSTMESSAGES, OnNMDblclkLstmessages)
	ON_BN_CLICKED(IDC_BTNINIT, OnBnClickedBtninit)
	ON_BN_CLICKED(IDC_BTNRELEASE, OnBnClickedBtnrelease)
	ON_WM_SHOWWINDOW()
	ON_BN_CLICKED(IDC_RDBTIMER, &CPCANBasicExampleDlg::OnBnClickedRdbtimer)
	ON_BN_CLICKED(IDC_RDBEVENT, &CPCANBasicExampleDlg::OnBnClickedRdbevent)
	ON_BN_CLICKED(IDC_CHBTIMESTAMP, &CPCANBasicExampleDlg::OnBnClickedChbtimestamp)
	ON_BN_CLICKED(IDC_BUTTON_HWREFRESH, &CPCANBasicExampleDlg::OnBnClickedButtonHwrefresh)
	ON_CBN_SELCHANGE(IDC_CBBHWSTYPE, &CPCANBasicExampleDlg::OnCbnSelchangeCbbhwstype)
	ON_BN_CLICKED(IDC_CHBFILTEREXTENDED, &CPCANBasicExampleDlg::OnBnClickedChbfilterextended)
	ON_NOTIFY(UDN_DELTAPOS, IDC_NUDFILTERFROM, &CPCANBasicExampleDlg::OnDeltaposNudfilterfrom)
	ON_EN_KILLFOCUS(IDC_TXTFILTERFROM, &CPCANBasicExampleDlg::OnEnKillfocusTxtfilterfrom)
	ON_NOTIFY(UDN_DELTAPOS, IDC_NUDFILTERTO, &CPCANBasicExampleDlg::OnDeltaposNudfilterto)
	ON_EN_KILLFOCUS(IDC_TXTFILTERTO, &CPCANBasicExampleDlg::OnEnKillfocusTxtfilterto)
	ON_BN_CLICKED(IDC_BUTTONFILTERAPPLY, &CPCANBasicExampleDlg::OnBnClickedButtonfilterapply)
	ON_BN_CLICKED(IDC_BUTTONFILTERQUERY, &CPCANBasicExampleDlg::OnBnClickedButtonfilterquery)
	ON_BN_CLICKED(IDC_RDBMANUAL, &CPCANBasicExampleDlg::OnBnClickedRdbmanual)
	ON_BN_CLICKED(IDC_BUTTONREAD, &CPCANBasicExampleDlg::OnBnClickedButtonread)
	ON_BN_CLICKED(IDC_BUTTONREADINGCLEAR, &CPCANBasicExampleDlg::OnBnClickedButtonreadingclear)
	ON_CBN_SELCHANGE(IDC_COMBOPARAMETER, &CPCANBasicExampleDlg::OnCbnSelchangeComboparameter)
	ON_NOTIFY(UDN_DELTAPOS, IDC_NUDPARAMDEVNUMBER, &CPCANBasicExampleDlg::OnDeltaposNudparamdevnumber)
	ON_EN_KILLFOCUS(IDC_TXTPARAMDEVNUMBER, &CPCANBasicExampleDlg::OnEnKillfocusTxtparamdevnumber)
	ON_BN_CLICKED(IDC_BUTTONPARAMSET, &CPCANBasicExampleDlg::OnBnClickedButtonparamset)
	ON_BN_CLICKED(IDC_BUTTONPARAMGET, &CPCANBasicExampleDlg::OnBnClickedButtonparamget)
	ON_WM_CLOSE()
	ON_BN_CLICKED(IDC_BUTTONVERSION, &CPCANBasicExampleDlg::OnBnClickedButtonversion)
	ON_BN_CLICKED(IDC_BUTTONINFOCLEAR, &CPCANBasicExampleDlg::OnBnClickedButtoninfoclear)
	ON_BN_CLICKED(IDC_BUTTONSTATUS, &CPCANBasicExampleDlg::OnBnClickedButtonstatus)
	ON_BN_CLICKED(IDC_BUTTONRESET, &CPCANBasicExampleDlg::OnBnClickedButtonreset)
	ON_BN_CLICKED(IDC_CHBCANFD, &CPCANBasicExampleDlg::OnBnClickedChbfcanfd)
	ON_LBN_DBLCLK(IDC_LISTINFO, &CPCANBasicExampleDlg::OnLbnDblclkListinfo)
	ON_BN_CLICKED(IDC_BUTTON_HWREFRESH2, &CPCANBasicExampleDlg::OnBnClickedButtonHwrefresh2)
	ON_BN_CLICKED(IDC_BUTTONRESET2, &CPCANBasicExampleDlg::OnBnClickedButtonreset2)
	ON_BN_CLICKED(IDC_BUTTONFILTERQUERY2, &CPCANBasicExampleDlg::OnBnClickedButtonfilterquery2)
	ON_BN_CLICKED(IDC_BUTTONGLASS, &CPCANBasicExampleDlg::OnBnClickedButtonglass)
	ON_BN_CLICKED(IDC_BUTTONSW, &CPCANBasicExampleDlg::OnBnClickedButtonsw)
	ON_BN_CLICKED(IDC_BUTTONSW2, &CPCANBasicExampleDlg::OnBnClickedButtonsw2)
	ON_NOTIFY(UDN_DELTAPOS, IDC_NUDFILTERSUBS, &CPCANBasicExampleDlg::OnDeltaposNudfiltersubs)
	ON_NOTIFY(UDN_DELTAPOS, IDC_NUDFILTERTRIALS, &CPCANBasicExampleDlg::OnDeltaposNudfiltertrials)
	ON_BN_CLICKED(IDC_BUTTONSUB, &CPCANBasicExampleDlg::OnBnClickedButtonsub)
	ON_BN_CLICKED(IDC_BUTTONZERO, &CPCANBasicExampleDlg::OnBnClickedButtonzero)
	ON_BN_CLICKED(IDC_BUTTONERATE, &CPCANBasicExampleDlg::OnBnClickedButtonerate)
	ON_NOTIFY(UDN_DELTAPOS, IDC_NUDFILTEROFFSET, &CPCANBasicExampleDlg::OnDeltaposNudfilteroffset)
END_MESSAGE_MAP()


// PCANBasicExampleDlg message handlers
//
BOOL CPCANBasicExampleDlg::OnInitDialog()
{
	CDialog::OnInitDialog();

	// Set the icon for this dialog.  The framework does this automatically
	//  when the application's main window is not a dialog
	SetIcon(m_hIcon, TRUE);			// Set big icon
	SetIcon(m_hIcon, FALSE);		// Set small icon

	// Extra initialization here
	InitializeControls();
	
	return TRUE;  // return TRUE  unless you set the focus to a control
}

// If you add a minimize button to your dialog, you will need the code below
//  to draw the icon.  For MFC applications using the document/view model,
//  this is automatically done for you by the framework.

void CPCANBasicExampleDlg::OnPaint()
{
	if (IsIconic())
	{
		CPaintDC dc(this); // device context for painting

		SendMessage(WM_ICONERASEBKGND, reinterpret_cast<WPARAM>(dc.GetSafeHdc()), 0);

		// Center icon in client rectangle
		int cxIcon = GetSystemMetrics(SM_CXICON);
		int cyIcon = GetSystemMetrics(SM_CYICON);
		CRect rect;
		GetClientRect(&rect);
		int x = (rect.Width() - cxIcon + 1) / 2;
		int y = (rect.Height() - cyIcon + 1) / 2;

		// Draw the icon
		dc.DrawIcon(x, y, m_hIcon);
	}
	else
	{
		CDialog::OnPaint();
	}
}

// The system calls this function to obtain the cursor to display while the user drags
//  the minimized window.
HCURSOR CPCANBasicExampleDlg::OnQueryDragIcon()
{
	return static_cast<HCURSOR>(m_hIcon);
}

void CPCANBasicExampleDlg::InitializeControls(void)
{
	// Initialize the Critical Section
	//
	InitializeProtection();

	// Creates an array with all possible PCAN-Channels
	// 
	m_HandlesArray[0] = PCAN_ISABUS1;
	m_HandlesArray[1] = PCAN_ISABUS2;
	m_HandlesArray[2] = PCAN_ISABUS3;
	m_HandlesArray[3] = PCAN_ISABUS4;
	m_HandlesArray[4] = PCAN_ISABUS5;
	m_HandlesArray[5] = PCAN_ISABUS6;
	m_HandlesArray[6] = PCAN_ISABUS7;
	m_HandlesArray[7] = PCAN_ISABUS8;
	m_HandlesArray[8] = PCAN_DNGBUS1;
	m_HandlesArray[9] = PCAN_PCIBUS1;
	m_HandlesArray[10] = PCAN_PCIBUS2;
	m_HandlesArray[11] = PCAN_PCIBUS3;
	m_HandlesArray[12] = PCAN_PCIBUS4;
	m_HandlesArray[13] = PCAN_PCIBUS5;
	m_HandlesArray[14] = PCAN_PCIBUS6;
	m_HandlesArray[15] = PCAN_PCIBUS7;
	m_HandlesArray[16] = PCAN_PCIBUS8;
	m_HandlesArray[17] = PCAN_PCIBUS9;
	m_HandlesArray[18] = PCAN_PCIBUS10;
	m_HandlesArray[19] = PCAN_PCIBUS11;
	m_HandlesArray[20] = PCAN_PCIBUS12;
	m_HandlesArray[21] = PCAN_PCIBUS13;
	m_HandlesArray[22] = PCAN_PCIBUS14;
	m_HandlesArray[23] = PCAN_PCIBUS15;
	m_HandlesArray[24] = PCAN_PCIBUS16;
	m_HandlesArray[25] = PCAN_USBBUS1;
	m_HandlesArray[26] = PCAN_USBBUS2;
	m_HandlesArray[27] = PCAN_USBBUS3;
	m_HandlesArray[28] = PCAN_USBBUS4;
	m_HandlesArray[29] = PCAN_USBBUS5;
	m_HandlesArray[30] = PCAN_USBBUS6;
	m_HandlesArray[31] = PCAN_USBBUS7;
	m_HandlesArray[32] = PCAN_USBBUS8;
	m_HandlesArray[33] = PCAN_USBBUS9;
	m_HandlesArray[34] = PCAN_USBBUS10;
	m_HandlesArray[35] = PCAN_USBBUS11;
	m_HandlesArray[36] = PCAN_USBBUS12;
	m_HandlesArray[37] = PCAN_USBBUS13;
	m_HandlesArray[38] = PCAN_USBBUS14;
	m_HandlesArray[39] = PCAN_USBBUS15;
	m_HandlesArray[40] = PCAN_USBBUS16;
	m_HandlesArray[41] = PCAN_PCCBUS1;
	m_HandlesArray[42] = PCAN_PCCBUS2;
	m_HandlesArray[43] = PCAN_LANBUS1;
	m_HandlesArray[44] = PCAN_LANBUS2;
	m_HandlesArray[45] = PCAN_LANBUS3;
	m_HandlesArray[46] = PCAN_LANBUS4;
	m_HandlesArray[47] = PCAN_LANBUS5;
	m_HandlesArray[48] = PCAN_LANBUS6;
	m_HandlesArray[49] = PCAN_LANBUS7;
	m_HandlesArray[50] = PCAN_LANBUS8;
	m_HandlesArray[51] = PCAN_LANBUS9;
	m_HandlesArray[52] = PCAN_LANBUS10;
	m_HandlesArray[53] = PCAN_LANBUS11;
	m_HandlesArray[54] = PCAN_LANBUS12;
	m_HandlesArray[55] = PCAN_LANBUS13;
	m_HandlesArray[56] = PCAN_LANBUS14;
	m_HandlesArray[57] = PCAN_LANBUS15;
	m_HandlesArray[58] = PCAN_LANBUS16;

	// List Control
	//
	lstMessages.InsertColumn(MSG_TYPE,"Type",LVCFMT_LEFT,110,1);
	lstMessages.InsertColumn(MSG_ID,"ID",LVCFMT_LEFT,90,2);
	lstMessages.InsertColumn(MSG_LENGTH,"Length",LVCFMT_LEFT,50,3);		
	lstMessages.InsertColumn(MSG_COUNT,"Count",LVCFMT_LEFT,49,5);	
	lstMessages.InsertColumn(MSG_TIME,"Rcv Time",LVCFMT_LEFT,70,5);	
	lstMessages.InsertColumn(MSG_DATA,"Data",LVCFMT_LEFT,170,4);		
	lstMessages.SetExtendedStyle(lstMessages.GetExtendedStyle()|LVS_EX_FULLROWSELECT);

	lstMessages_GPS.InsertColumn(GPS_DATA, "Data", LVCFMT_LEFT, 130);
	lstMessages_GPS.InsertColumn(GPS_SATS, "GPS_1", LVCFMT_CENTER, 200);
	lstMessages_GPS.InsertColumn(GPS_TIME, "GPS_2", LVCFMT_CENTER, 160);
	lstMessages_GPS.InsertColumn(GPS_LATITUDE, "GPS_3", LVCFMT_CENTER, 200);

	lstMessages_GPS.SetExtendedStyle(lstMessages_GPS.GetExtendedStyle() | LVS_EX_FULLROWSELECT);

	// As defautl, normal CAN hardware is used
	//
	m_IsFD = false;

    // We set the variable for the current 
	// PCAN Basic Class instance to use it
	//
	m_objPCANBasic = new PCANBasicClass();
	
	// We set the variable to know which reading mode is
	// currently selected (Event by default)
	//
	m_ActiveReadingMode = 1;

	// Create a list to store the displayed messages
	//
	m_LastMsgsList = new CPtrList();

	// Create Event to use Received-event
	//
	m_hEvent = CreateEvent(NULL, FALSE, FALSE, "");

	// Prepares the PCAN-Basic's debug-Log file
	//
	FillComboBoxData();

	// UpDown Filter From
	//
	nudFilterFrom.SetRange32(0,0x1FFFFFFF - 1);
	nudFilterFrom.SetPos32(0x301);
	txtFilterFrom = "300";

	// UpDown Filter To
	//
	nudFilterTo.SetRange32(0,0x7FF - 1);
	nudFilterTo.SetPos32(0x306);
	txtFilterTo = "306";

	// UpDown Device Number
	//
	nudParameterDevNumber.SetRange32(0,254);
	nudParameterDevNumber.SetPos32(0);
	txtParameterDevNumber = "0";

	// Init CheckBox
	chbFilterExtended.SetCheck(1);
	rdbFilterCustom.SetCheck(1);
	rdbReadingEvent.SetCheck(1);
	rdbParameterActive.SetCheck(1);
	chbReadingTimeStamp.SetCheck(1);

	// Set default connection status
	SetConnectionStatus(false);

	// Init log parameters
	ConfigureLogFile();

	//init GPS
	{
		InitGPSConfig();
		EnumComPorts();
		InitCrossXbow();
		m_GPS_Net_Event = CreateEvent(NULL, FALSE, FALSE, "");
		m_Xbow_Net_Event = CreateEvent(NULL, FALSE, FALSE, "");
		m_Send_Event = CreateEvent(NULL, FALSE, FALSE, "");
		InterlockedExchange(&m_Send_Terminated, 0);
		m_Network_hThread = NULL;

		txtGlass = (_T("0"));
		m_Shutter_Time = 0;
		m_Shutter_Time_Constant = m_Shutter_Time;
		m_Shutter_Duration = 0;

		txtFilterSubs = "s01";
		txtFilterTrials = "t01";

		nudFilterSubs.SetRange32(1, 99);
		nudFilterSubs.SetPos32(1);

		nudFilterTrials.SetRange32(1, 99);
		nudFilterTrials.SetPos(1);

		txtLaneChange = (_T("0"));
		m_LaneChange_Time = 0;
		m_LaneChange_Time_Constant = m_LaneChange_Time;

		txtLC2Value = (_T("0"));

		ValueZero = (_T("-1"));
		ValueErate = (_T("-1"));
		m_zerotime = -1;
		m_erate = -1;

		mNUDOffset.SetRange32(0, 99);
		mNUDOffset.SetPos(0);

		mValueOffset = (_T("0"));
		m_Shutter_Offset = 0.0f;
	}
	

	// Update UI
	UpdateData(FALSE);
}

void CPCANBasicExampleDlg::InitializeProtection()
{
	m_objpCS = new CRITICAL_SECTION();
	InitializeCriticalSection(m_objpCS);

	InitializeCriticalSection(&m_xBow_CriticalSection);
}

void CPCANBasicExampleDlg::FinalizeProtection()
{
	try
	{
		DeleteCriticalSection(m_objpCS);
		delete m_objpCS;
		m_objpCS = NULL;
	}
	catch(...)
	{
		throw;
	}

	try
	{
		DeleteCriticalSection(&m_xBow_CriticalSection);
	}
	catch (...)
	{
		throw;
	}
}

void CPCANBasicExampleDlg::OnTimer(UINT_PTR nIDEvent)
{
	if(nIDEvent == 1)
		// Read Pending Messages
		//
		ReadMessages();
	if(nIDEvent == 2)
		// Display messages
		//
		DisplayMessages();
	if (nIDEvent == 2 && !btnShutterApply.IsWindowEnabled())
	{
		boost::posix_time::ptime now = boost::posix_time::microsec_clock::local_time();
		boost::posix_time::time_duration diff = now - m_Shutter_CPUTIME_BEGIN;
		m_Shutter_Duration = diff.total_milliseconds();
		m_Shutter_Duration /= 1000.0f;

		txtLC2Value.Format("%f", ((double)m_Shutter_Duration));
		txtLC2.SetWindowTextA(txtLC2Value);

		// Check whether should trigger shutter glass
		if (m_Shutter_Time > 0 || m_LaneChange_Time > 0)
		{
			bool set = false;

			if (m_Shutter_Time > 0)
			{
				set = false;

				if (m_Shutter_Offset && m_Shutter_Duration + m_Shutter_Offset >= m_Shutter_Time && m_Shutter_Time_Rec.back().second != "OFF")
				{
					SetShutterGlass();
					
					boost::posix_time::ptime now = boost::posix_time::microsec_clock::local_time();
					boost::posix_time::time_duration diff = now - time_t_epoch;

					std::pair<__int64, std::string> temp(diff.total_milliseconds(), "OFF");
					m_Shutter_Time_Rec.push_back(temp);
				}
				if (m_Shutter_Duration >= m_Shutter_Time)
				{
					set = true;

					PlaySound(TEXT("alert.wav"), NULL, SND_FILENAME | SND_ASYNC);

					boost::posix_time::ptime now = boost::posix_time::microsec_clock::local_time();
					boost::posix_time::time_duration diff = now - time_t_epoch;

					std::pair<__int64, std::string> temp(diff.total_milliseconds(), "LC");
					m_Shutter_Time_Rec.push_back(temp);
				}

				txtGlass.Format("%f", (m_Shutter_Time - (double)m_Shutter_Duration));
				txtCtrlGlass.SetWindowTextA(txtGlass);

				if (set)
				{
					m_Shutter_Time = 0;
					txtGlass = (_T("0"));
					txtCtrlGlass.SetWindowTextA(txtGlass);
				}
			}

			if (m_LaneChange_Time > 0)
			{
				set = false;
				if (m_Shutter_Duration >= m_LaneChange_Time)
				{
					set = true;

					PlaySound(TEXT("low.wav"), NULL, SND_FILENAME | SND_ASYNC);

					boost::posix_time::ptime now = boost::posix_time::microsec_clock::local_time();
					boost::posix_time::time_duration diff = now - time_t_epoch;

					std::pair<__int64, std::string> temp(diff.total_milliseconds(), "Alarm");
					m_Shutter_Time_Rec.push_back(temp);
				}

				txtLaneChange.Format("%f", (m_LaneChange_Time - (double)m_Shutter_Duration));
				txtCtrlLaneChange.SetWindowTextA(txtLaneChange);

				if (set)
				{
					m_LaneChange_Time = 0;
					txtLaneChange = (_T("0"));
					txtCtrlLaneChange.SetWindowTextA(txtLaneChange);
				}
			}
		}
	}

	CDialog::OnTimer(nIDEvent);
}

void CPCANBasicExampleDlg::OnCbnSelchangecbbChannel()
{
	bool bNonPnP;
	CString strTemp;
	int pcanHandleTemp;

	// Get the handle from the text being shown
	//
	strTemp = GetComboBoxSelectedLabel(&cbbChannel);
	strTemp = strTemp.Mid(strTemp.Find('(') + 1, 3);

	strTemp.Replace('h', ' ');
	strTemp.Trim(' ');

	// Determines if the handle belong to a No Plug&Play hardware 
	//
	pcanHandleTemp = HexTextToInt(strTemp);
	m_PcanHandle = pcanHandleTemp;
	bNonPnP = m_PcanHandle <= PCAN_DNGBUS1;

	// Activates/deactivates configuration controls according with the 
	// kind of hardware
	//
	cbbIO.EnableWindow(bNonPnP);
	cbbInterrupt.EnableWindow(bNonPnP);
	cbbHwsType.EnableWindow(bNonPnP);
}

void CPCANBasicExampleDlg::OnCbnSelchangeCbbbaudrates()
{
	// We save the corresponding Baudrate enumeration
	// type value for every selected Baudrate from the
	// list.
	//
	switch(cbbBaudrates.GetCurSel())
	{
	case 0:
		m_Baudrate = PCAN_BAUD_1M;
		break;
	case 1:
		m_Baudrate = PCAN_BAUD_500K;
		break;
	case 2:
		m_Baudrate = PCAN_BAUD_250K;
		break;
	case 3:
		m_Baudrate = PCAN_BAUD_125K;
		break;
	case 4:
		m_Baudrate = PCAN_BAUD_125K;
		break;
	default:
		m_Baudrate = (TPCANBaudrate)0;
		break;
	}
}

void CPCANBasicExampleDlg::OnNMDblclkLstmessages(NMHDR *pNMHDR, LRESULT *pResult)
{
	*pResult = 0;
	OnBnClickedButtonreadingclear();
}

typedef struct 
{
	DWORD a;
	BYTE b;
	BYTE c;
	BYTE d;
	WORD e;
	BYTE f[64];
}Temp1;

void CPCANBasicExampleDlg::OnBnClickedBtninit()
{
	HWND current = ::GetActiveWindow();

	if (btnApplySub.IsWindowEnabled())
	{
		int msgboxID = ::MessageBox(current, "WILL Overwrite!", NULL, MB_ICONWARNING | MB_YESNO | MB_DEFBUTTON2);

		switch (msgboxID)
		{
		case IDYES:
			break;
		case IDNO:
			return;
		default:
			return;
		}
	}

	TPCANStatus stsResult;
	int selectedIO;
	int selectedInterrupt;

	UpdateData(TRUE);

	// Parse IO and Interrupt
	//
	selectedIO = HexTextToInt(GetComboBoxSelectedLabel(&cbbIO));
	selectedInterrupt = atoi(GetComboBoxSelectedLabel(&cbbInterrupt));

	// Connects a selected PCAN-Basic channel
	//
	if (m_IsFD)
		stsResult = m_objPCANBasic->InitializeFD(m_PcanHandle, txtBitrate.GetBuffer());
	else
		stsResult = m_objPCANBasic->Initialize(m_PcanHandle, m_Baudrate, m_HwType, selectedIO, selectedInterrupt);

	if (stsResult != PCAN_ERROR_OK)
		if (stsResult != PCAN_ERROR_CAUTION)
			::MessageBox(NULL, GetFormatedError(stsResult), "Error!",MB_ICONERROR);
		else
		{
            IncludeTextMessage("******************************************************");
            IncludeTextMessage("The bitrate being used is different than the given one");
            IncludeTextMessage("******************************************************");
            stsResult = PCAN_ERROR_OK;
		}
	else
        // Prepares the PCAN-Basic's PCAN-Trace file
        //
		ConfigureTraceFile();

	// Sets the connection status of the main-form
	//
	SetConnectionStatus(stsResult == PCAN_ERROR_OK);

	//Init GPS
	{
		if (stsResult == PCAN_ERROR_OK)
		{
			InitGPSConfig();
			cbbParameter.SetCurSel(7);
			OnBnClickedButtonparamset();

//			cbbParameter.SetCurSel(3);	//default:Listen-Only Mode
//			OnBnClickedButtonparamset();

			OnCbnSelchangeComboparameter();
		}
// 
  		InitCrossXbow();
		btnRefreshCom.EnableWindow(FALSE);
		if (ccbHwXbow.GetCount() > 0)
		{
			ConnectCrossXbow();
			SetTimerDisplay(true);
			btnInit.EnableWindow(FALSE);
			btnRelease.EnableWindow(TRUE);
			ccbHwXbow.EnableWindow(FALSE);
		}

		m_Shutter_Time_Rec.resize(0);
		m_Shutter_Time_Rec.clear();
	}
}

void CPCANBasicExampleDlg::OnBnClickedBtnrelease()
{
	// Terminate Read Thread if it exists
	//
	if(m_hThread != NULL)
	{
		m_Terminated = true;
		WaitForSingleObject(m_hThread,-1);
		m_hThread = NULL;
	}

	// We stop to read from the CAN queue
	//
	SetTimerRead(false);

	// Releases a current connected PCAN-Basic channel
	//
	m_objPCANBasic->Uninitialize(m_PcanHandle);

	// Sets the connection status of the main-form
	//
	SetConnectionStatus(false);

	//Write GPS
	{
		SetTimerDisplay(false);
		IncludeTextMessage("Saving GPS Data...");
		StoreMsgList();
		WriteGPSFile();
		IncludeTextMessage("GPS Data Saved");
	}

	//Write Accelerometer
	{
		bool terminated = true;
		if (m_Xbow_hThread != NULL)
		{
			InterlockedExchange(&m_XbowTerminated, 1);
			DWORD result = WaitForSingleObject(m_Xbow_hThread, 1000);
			if (result == WAIT_TIMEOUT)
			{
				DWORD threadTerminated;
				GetExitCodeThread(m_Xbow_hThread, &threadTerminated);
				TerminateThread(m_Xbow_hThread, threadTerminated);
				terminated = false;
			}
			m_Xbow_hThread = NULL;
		}
		IncludeTextMessage("Saving Accelerometer Data...");
		StoreAccMsgList();
		WriteAccFile();
		btnRefreshCom.EnableWindow(TRUE);
		if (terminated)
		{
			IncludeTextMessage("Accelerometer Data Saved");
		}
		else
		{
			IncludeTextMessage("Accelerometer No Response! Data may be damaged!");
		}
		ccbHwXbow.EnableWindow(TRUE);
// 		ComUninitialize();
	}

	//Write Shutter Glass
	{
		WriteShutterGlass();
		m_Shutter_Time = m_Shutter_Time_Constant;
		txtGlass.Format("%-.3f",m_Shutter_Time);
		txtCtrlGlass.SetWindowTextA(txtGlass);

		m_LaneChange_Time = m_LaneChange_Time_Constant;
		txtLaneChange.Format("%-.3f",m_LaneChange_Time);
		txtCtrlLaneChange.SetWindowTextA(txtLaneChange);

		OnBnClickedButtonglass();
	}

	{
		OnBnClickedButtonsub();
		mNUDOffset.EnableWindow(TRUE);
	}

	OnBnClickedButtonreadingclear();
}

void CPCANBasicExampleDlg::OnShowWindow(BOOL bShow, UINT nStatus)
{
	CDialog::OnShowWindow(bShow, nStatus);
}


void CPCANBasicExampleDlg::OnBnClickedRdbtimer()
{
	// Check reading mode selection change
	//
	if(rdbReadingTimer.GetCheck() && (m_ActiveReadingMode != 0))
	{
		// Process change
		//
		m_ActiveReadingMode = 0;
		ReadingModeChanged();
	}
}
void CPCANBasicExampleDlg::OnBnClickedRdbevent()
{
	// Check reading mode selection change
	//
	if(rdbReadingEvent.GetCheck() && (m_ActiveReadingMode != 1))
	{
		// Process change
		//
		m_ActiveReadingMode = 1;
		ReadingModeChanged();
	}
}
void CPCANBasicExampleDlg::OnBnClickedChbtimestamp()
{
	MessageStatus* msgStsCurrentMessage;
	CString str;	
	POSITION pos;
	BOOL bChecked;

	// According with the check-value of this checkbox,
	// the recieved time of a messages will be interpreted as 
	// period (time between the two last messages) or as time-stamp
	// (the elapsed time since windows was started).
	// - (Protected environment)
	//
	{
		clsCritical locker(m_objpCS);

		pos = m_LastMsgsList->GetHeadPosition();
		bChecked = chbReadingTimeStamp.GetCheck();
		while(pos)
		{
			msgStsCurrentMessage = (MessageStatus*)m_LastMsgsList->GetNext(pos);
			msgStsCurrentMessage->ShowingPeriod = bChecked > 0;
		}
	}
}

void CPCANBasicExampleDlg::OnBnClickedButtonHwrefresh()
{
	int iBuffer;
	TPCANStatus stsResult;
	bool isFD;

	// Clears the Channel combioBox and fill it againa with 
	// the PCAN-Basic handles for no-Plug&Play hardware and
	// the detected Plug&Play hardware
	//
	cbbChannel.ResetContent();
	for (int i = 0; i < (sizeof(m_HandlesArray) /sizeof(TPCANHandle)) ; i++)
	{
		// Includes all no-Plug&Play Handles
		//
		if (m_HandlesArray[i] <= PCAN_DNGBUS1)
//			cbbChannel.AddString(FormatChannelName(m_HandlesArray[i]));
			continue;	// get rid of no-Plug&Play Handles
		else
		{
			// Checks for a Plug&Play Handle and, according with the return value, includes it
			// into the list of available hardware channels.
			//
			stsResult = m_objPCANBasic->GetValue((TPCANHandle)m_HandlesArray[i], PCAN_CHANNEL_CONDITION, (void*)&iBuffer, sizeof(iBuffer));
			if (((stsResult) == PCAN_ERROR_OK) && ((iBuffer & PCAN_CHANNEL_AVAILABLE) == PCAN_CHANNEL_AVAILABLE))
			{
				stsResult = m_objPCANBasic->GetValue((TPCANHandle)m_HandlesArray[i], PCAN_CHANNEL_FEATURES, (void*)&iBuffer, sizeof(iBuffer));
				isFD = (stsResult == PCAN_ERROR_OK) && (iBuffer & FEATURE_FD_CAPABLE);
				cbbChannel.AddString(FormatChannelName(m_HandlesArray[i], isFD));
			}
		}
	}

	// Select Last One
	//
	cbbChannel.SetCurSel(cbbChannel.GetCount() - 1);
	OnCbnSelchangecbbChannel();
}

void CPCANBasicExampleDlg::OnCbnSelchangeCbbhwstype()
{
	// Saves the current type for a no-Plug&Play hardware
	//
	switch (cbbHwsType.GetCurSel())
	{
	case 0:
		m_HwType = PCAN_TYPE_ISA;
		break;
	case 1:
		m_HwType = PCAN_TYPE_ISA_SJA;
		break;
	case 2:
		m_HwType = PCAN_TYPE_ISA_PHYTEC;
		break;
	case 3:
		m_HwType = PCAN_TYPE_DNG;
		break;
	case 4:
		m_HwType = PCAN_TYPE_DNG_EPP;
		break;
	case 5:
		m_HwType = PCAN_TYPE_DNG_SJA;
		break;
	case 6:
		m_HwType = PCAN_TYPE_DNG_SJA_EPP;
		break;
	}
}

void CPCANBasicExampleDlg::OnBnClickedChbfilterextended()
{
	int iMaxValue;

	iMaxValue = (chbFilterExtended.GetCheck()) ? 0x1FFFFFFF : 0x7FF;

	// We check that the maximum value for a selected filter 
	// mode is used
	//
	nudFilterTo.SetRange32(0,iMaxValue - 1);
	if (nudFilterTo.GetPos32() > iMaxValue)
	{
		nudFilterTo.SetPos32(iMaxValue);
		txtFilterTo.Format("%X", iMaxValue);
	}
	
	nudFilterFrom.SetRange32(0,iMaxValue - 1);
	if (nudFilterFrom.GetPos32() > iMaxValue)
	{
		nudFilterFrom.SetPos32(iMaxValue);
		txtFilterFrom.Format("%X", iMaxValue);
	}

	UpdateData(FALSE);
}

void CPCANBasicExampleDlg::OnDeltaposNudfilterfrom(NMHDR *pNMHDR, LRESULT *pResult)
{
	LPNMUPDOWN pNMUpDown = reinterpret_cast<LPNMUPDOWN>(pNMHDR);
	int iNewVal;

	//Compute new selected From value
	iNewVal =  pNMUpDown->iPos + ((pNMUpDown->iDelta > 0) ? 1 : -1);
	if(iNewVal < 0)
		iNewVal = 0;

	//Update textBox
	txtFilterFrom.Format("%X", iNewVal);
	UpdateData(FALSE);

	*pResult = 0;
}

void CPCANBasicExampleDlg::OnEnKillfocusTxtfilterfrom()
{
	int iMaxValue;
	iMaxValue = (chbFilterExtended.GetCheck()) ? 0x1FFFFFFF : 0x7FF;
	int newValue;
	UpdateData(TRUE);

	// Compute new edited value
	//
	newValue = HexTextToInt(txtFilterFrom);
	if(newValue > iMaxValue)
		newValue = iMaxValue;
	else if(newValue < 0)
		newValue = 0;
	// Update Nud control
	//
	nudFilterFrom.SetPos32(newValue);
	txtFilterFrom.Format("%X", newValue);
	UpdateData(FALSE);
}

void CPCANBasicExampleDlg::OnDeltaposNudfilterto(NMHDR *pNMHDR, LRESULT *pResult)
{
	LPNMUPDOWN pNMUpDown = reinterpret_cast<LPNMUPDOWN>(pNMHDR);
	int iNewVal;

	// Compute new selected From value
	//
	iNewVal =  pNMUpDown->iPos + ((pNMUpDown->iDelta > 0) ? 1 : -1);
	if(iNewVal < 0)
		iNewVal = 0;

	// Update textBox
	//
	txtFilterTo.Format("%X", iNewVal);
	UpdateData(FALSE);

	*pResult = 0;
}

void CPCANBasicExampleDlg::OnEnKillfocusTxtfilterto()
{
	int iMaxValue;
	iMaxValue = (chbFilterExtended.GetCheck()) ? 0x1FFFFFFF : 0x7FF;
	int newValue;
	UpdateData(TRUE);

	// Compute new edited value
	//
	newValue = HexTextToInt(txtFilterTo);
	if(newValue > iMaxValue)
		newValue = iMaxValue;
	else if(newValue < 0)
		newValue = 0;
	// Update Nud control
	//
	nudFilterTo.SetPos32(newValue);
	txtFilterTo.Format("%X", newValue);
	UpdateData(FALSE);
}

void CPCANBasicExampleDlg::OnBnClickedButtonfilterapply()
{
	int iBuffer;
	CString info;
	TPCANStatus stsResult;

	// Gets the current status of the message filter
	//
	if (!GetFilterStatus(&iBuffer))
		return;

	// Configures the message filter for a custom range of messages
	//
	if (rdbFilterCustom.GetCheck())
	{
		// Sets the custom filter
		//
		stsResult = m_objPCANBasic->FilterMessages(m_PcanHandle, nudFilterFrom.GetPos32(), nudFilterTo.GetPos32(), chbFilterExtended.GetCheck() ? PCAN_MODE_EXTENDED : PCAN_MODE_STANDARD);
		// If success, an information message is written, if it is not, an error message is shown
		//
		if (stsResult == PCAN_ERROR_OK)
		{
			info.Format("The filter was customized. IDs from {%X} to {%X} will be received", nudFilterFrom.GetPos32(), nudFilterTo.GetPos32());
			IncludeTextMessage(info);
		}
		else
			::MessageBox(NULL, GetFormatedError(stsResult), "Error!",MB_ICONERROR);

		return;
	}

	// The filter will be full opened or complete closed
	//
	if (rdbFilterClose.GetCheck())
		iBuffer = PCAN_FILTER_CLOSE;
	else
		iBuffer = PCAN_FILTER_OPEN;

	// The filter is configured
	//
	stsResult = m_objPCANBasic->SetValue(m_PcanHandle, PCAN_MESSAGE_FILTER, (void*)&iBuffer, sizeof(int));

	// If success, an information message is written, if it is not, an error message is shown
	//
	if (stsResult == PCAN_ERROR_OK)
	{
		info.Format("The filter was successfully %s", rdbFilterClose.GetCheck() ? "closed." : "opened.");
		IncludeTextMessage(info);
	}
	else
		::MessageBox(NULL, GetFormatedError(stsResult), "Error!",MB_ICONERROR);
}


void CPCANBasicExampleDlg::OnBnClickedButtonfilterquery()
{
	int iBuffer;

	// Queries the current status of the message filter
	//
	if (GetFilterStatus(&iBuffer))
	{
		switch(iBuffer)
		{
			// The filter is closed
			//
		case PCAN_FILTER_CLOSE:
			IncludeTextMessage("The Status of the filter is: closed.");
			break;
			// The filter is fully opened
			//
		case PCAN_FILTER_OPEN:
			IncludeTextMessage("The Status of the filter is: full opened.");
			break;
			// The filter is customized
			//
		case PCAN_FILTER_CUSTOM:
			IncludeTextMessage("The Status of the filter is: customized.");
			break;
			// The status of the filter is undefined. (Should never happen)
			//
		default:
			IncludeTextMessage("The Status of the filter is: Invalid.");
			break;
		}
	}
}

void CPCANBasicExampleDlg::OnBnClickedRdbmanual()
{
	// Check reading mode selection change
	//
	if(rdbReadingManual.GetCheck() && (m_ActiveReadingMode != 2))
	{
		// Process change
		//
		m_ActiveReadingMode = 2;
		ReadingModeChanged();
	}
}

void CPCANBasicExampleDlg::OnBnClickedButtonread()
{
	TPCANStatus stsResult;

	// We execute the "Read" function of the PCANBasic                
	//
	stsResult = m_IsFD ? ReadMessageFD() : ReadMessage();
	if (stsResult != PCAN_ERROR_OK)
		// If an error occurred, an information message is included
		//
		IncludeTextMessage(GetFormatedError(stsResult));
}

void CPCANBasicExampleDlg::OnBnClickedButtonreadingclear()
{
	// (Protected environment)
	//
	{
		clsCritical locker(m_objpCS);
		
		// Remove all messages
		//
		lstMessages.DeleteAllItems();
		lstMessages_GPS.DeleteAllItems();
		//This Piece of code cause bugs!!!
		while(m_LastMsgsList->GetCount())
			delete m_LastMsgsList->RemoveHead();
		//This Piece of code cause bugs!!!
		m_Xbow_AddedItem = false;
		m_Xbow_Added2Item = false;
	}
}

void CPCANBasicExampleDlg::OnCbnSelchangeComboparameter()
{
	// Activates/deactivates controls according with the selected 
	// PCAN-Basic parameter 
	//
	rdbParameterActive.EnableWindow(cbbParameter.GetCurSel() != 0);
	rdbParameterInactive.EnableWindow(rdbParameterActive.IsWindowEnabled());
	nudParameterDevNumber.EnableWindow(!rdbParameterActive.IsWindowEnabled());
	editParameterDevNumber.EnableWindow(!rdbParameterActive.IsWindowEnabled());
}

void CPCANBasicExampleDlg::OnDeltaposNudparamdevnumber(NMHDR *pNMHDR, LRESULT *pResult)
{
	LPNMUPDOWN pNMUpDown = reinterpret_cast<LPNMUPDOWN>(pNMHDR);
	int iNewVal;

	// Compute new selected From value
	//
	iNewVal =  pNMUpDown->iPos + ((pNMUpDown->iDelta > 0) ? 1 : -1);
	if(iNewVal < 0)
		iNewVal = 0;
	// Update textBox value
	//
	txtParameterDevNumber.Format("%d", iNewVal);
	UpdateData(FALSE);

	*pResult = 0;
}

void CPCANBasicExampleDlg::OnEnKillfocusTxtparamdevnumber()
{
	int newValue;
	UpdateData(TRUE);
	// Compute new edited value
	//
	newValue = atoi(txtParameterDevNumber);
	if(newValue > 255)
		newValue = 255;
	else if(newValue < 0)
		newValue = 0;

	// Update Nud control
	//
	nudParameterDevNumber.SetPos32(newValue);
	txtParameterDevNumber.Format("%d", newValue);
	UpdateData(FALSE);
}

void CPCANBasicExampleDlg::OnBnClickedButtonparamset()
{
	TPCANStatus stsResult;
	int iBuffer;
	CString info;
	TCHAR szDirectory[MAX_PATH] = "";
	bool bActivate;

	bActivate = rdbParameterActive.GetCheck() > 0;

	// Sets a PCAN-Basic parameter value
	//
	switch (cbbParameter.GetCurSel())
	{
		// The Device-Number of an USB channel will be set
		//
	case 0:
		iBuffer = nudParameterDevNumber.GetPos32();
		stsResult = m_objPCANBasic->SetValue(m_PcanHandle, PCAN_DEVICE_NUMBER, (void*)&iBuffer, sizeof(iBuffer));
		if(stsResult == PCAN_ERROR_OK)
			IncludeTextMessage("The desired Device-Number was successfully configured");
		break;

		// The 5 Volt Power feature of a PC-card or USB will be set
		//
	case 1:
		iBuffer = bActivate ? PCAN_PARAMETER_ON : PCAN_PARAMETER_OFF;
		stsResult = m_objPCANBasic->SetValue(m_PcanHandle, PCAN_5VOLTS_POWER, (void*)&iBuffer, sizeof(iBuffer));
		if(stsResult == PCAN_ERROR_OK)
		{
			info.Format("The USB/PC-Card 5 power was successfully %s", bActivate ? "activated" : "deactivated");
			IncludeTextMessage(info);
		}
		break;

		// The feature for automatic reset on BUS-OFF will be set
		//
	case 2:
		iBuffer = bActivate ? PCAN_PARAMETER_ON : PCAN_PARAMETER_OFF;
		stsResult = m_objPCANBasic->SetValue(m_PcanHandle, PCAN_BUSOFF_AUTORESET, (void*)&iBuffer, sizeof(iBuffer));
		if(stsResult == PCAN_ERROR_OK)
		{
			info.Format("The automatic-reset on BUS-OFF was successfully %s", bActivate ? "activated" : "deactivated");
			IncludeTextMessage(info);
		}		
		break;

		// The CAN option "Listen Only" will be set
		//
	case 3:
		iBuffer = bActivate ? PCAN_PARAMETER_ON : PCAN_PARAMETER_OFF;
		stsResult = m_objPCANBasic->SetValue(m_PcanHandle, PCAN_LISTEN_ONLY, (void*)&iBuffer, sizeof(iBuffer));
		if(stsResult == PCAN_ERROR_OK)
		{
			info.Format("The CAN option \"Listen Only\" was successfully %s", bActivate ? "activated" : "deactivated");
			IncludeTextMessage(info);
		}
		break;

		// The feature for logging debug-information will be set
		//
	case 4:
		iBuffer = bActivate ? PCAN_PARAMETER_ON : PCAN_PARAMETER_OFF;
		stsResult = m_objPCANBasic->SetValue(PCAN_NONEBUS, PCAN_LOG_STATUS, (void*)&iBuffer, sizeof(iBuffer));
		if(stsResult == PCAN_ERROR_OK)
		{
			info.Format("The feature for logging debug information was successfully %s", bActivate ? "activated" : "deactivated");
			IncludeTextMessage(info);
			::GetCurrentDirectory(sizeof(szDirectory) - 1, szDirectory);
			info.Format("Log file folder: %s" , szDirectory);
			IncludeTextMessage(info);
		}
		break;

		// The channel option "Receive Status" will be set
		//
	case 5:
		iBuffer = bActivate ? PCAN_PARAMETER_ON : PCAN_PARAMETER_OFF;
		stsResult = m_objPCANBasic->SetValue(m_PcanHandle, PCAN_RECEIVE_STATUS, (void*)&iBuffer, sizeof(iBuffer));
		if(stsResult == PCAN_ERROR_OK)
		{
			info.Format("The channel option \"Receive Status\" was set to %s", bActivate ? "ON" : "OFF");
			IncludeTextMessage(info);
		}
		break;

		// The feature for tracing will be set
		//
	case 7:
		iBuffer = bActivate ? PCAN_PARAMETER_ON : PCAN_PARAMETER_OFF;
		stsResult = m_objPCANBasic->SetValue(m_PcanHandle, PCAN_TRACE_STATUS, (void*)&iBuffer, sizeof(iBuffer));
		if(stsResult == PCAN_ERROR_OK)
		{
			info.Format("The feature for tracing data was successfully %s", bActivate ? "activated" : "deactivated");
			IncludeTextMessage(info);
		}
		break;

		// The feature for identifying an USB Channel will be set
		//
	case 8:
		iBuffer = bActivate ? PCAN_PARAMETER_ON : PCAN_PARAMETER_OFF;
		stsResult = m_objPCANBasic->SetValue(m_PcanHandle, PCAN_CHANNEL_IDENTIFYING, (void*)&iBuffer, sizeof(iBuffer));
		if(stsResult == PCAN_ERROR_OK)
		{
			info.Format("The procedure for channel identification was successfully %s", bActivate ? "activated" : "deactivated");
			IncludeTextMessage(info);
		}
		break;

		// The feature for using an already configured speed will be set
		//
	case 10:
		iBuffer = bActivate ? PCAN_PARAMETER_ON : PCAN_PARAMETER_OFF;
		stsResult = m_objPCANBasic->SetValue(m_PcanHandle, PCAN_BITRATE_ADAPTING, (void*)&iBuffer, sizeof(iBuffer));
		if(stsResult == PCAN_ERROR_OK)
		{
			info.Format("The feature for bit rate adaptation was successfully %s", bActivate ? "activated" : "deactivated");
			IncludeTextMessage(info);
		}
		break;

		// The current parameter is invalid
		//
	default:
		stsResult = PCAN_ERROR_UNKNOWN;
		::MessageBox(NULL, "Wrong parameter code.", "Error!",MB_ICONERROR);
		return;
	}

	// If the function fail, an error message is shown
	//
	if(stsResult != PCAN_ERROR_OK)
		::MessageBox(NULL, GetFormatedError(stsResult), "Error!",MB_ICONERROR);
}

void CPCANBasicExampleDlg::OnBnClickedButtonparamget()
{
	TPCANStatus stsResult;
	int iBuffer;
	char strBuffer[256];
	CString info;

	// Sets a PCAN-Basic parameter value
	//
	switch (cbbParameter.GetCurSel())
	{
		// The Device-Number of an USB channel will be get
		//
	case 0:
		stsResult = m_objPCANBasic->GetValue(m_PcanHandle, PCAN_DEVICE_NUMBER, (void*)&iBuffer, sizeof(iBuffer));
		if(stsResult == PCAN_ERROR_OK)
		{
			info.Format("The configured Device-Number is %d", iBuffer);
			IncludeTextMessage(info);
		}
		break;

		// The 5 Volt Power feature of a PC-card or USB will be get
		//
	case 1:
		stsResult = m_objPCANBasic->GetValue(m_PcanHandle, PCAN_5VOLTS_POWER, (void*)&iBuffer, sizeof(iBuffer));
		if(stsResult == PCAN_ERROR_OK)
		{
			info.Format("The 5-Volt Power of the USB/PC-Card is %s", (iBuffer == PCAN_PARAMETER_ON) ? "activated" : "deactivated");
			IncludeTextMessage(info);
		}
		break;

		// The feature for automatic reset on BUS-OFF will be get
		//
	case 2:
		stsResult = m_objPCANBasic->GetValue(m_PcanHandle, PCAN_BUSOFF_AUTORESET, (void*)&iBuffer, sizeof(iBuffer));
		if(stsResult == PCAN_ERROR_OK)
		{
			info.Format("The automatic-reset on BUS-OFF is %s", (iBuffer == PCAN_PARAMETER_ON) ? "activated" : "deactivated");
			IncludeTextMessage(info);
		}		
		break;

		// The CAN option "Listen Only" will be get
		//
	case 3:
		stsResult = m_objPCANBasic->GetValue(m_PcanHandle, PCAN_LISTEN_ONLY, (void*)&iBuffer, sizeof(iBuffer));
		if(stsResult == PCAN_ERROR_OK)
		{
			info.Format("The CAN option \"Listen Only\" is %s", (iBuffer == PCAN_PARAMETER_ON) ? "activated" : "deactivated");
			IncludeTextMessage(info);
		}
		break;

		// The feature for logging debug-information will be get
		//
	case 4:
		stsResult = m_objPCANBasic->GetValue(PCAN_NONEBUS, PCAN_LOG_STATUS, (void*)&iBuffer, sizeof(iBuffer));
		if(stsResult == PCAN_ERROR_OK)
		{
			info.Format("The feature for logging debug information is %s", (iBuffer == PCAN_PARAMETER_ON) ? "activated" : "deactivated");
			IncludeTextMessage(info);
		}
		break;

		// The activation status of the channel option "Receive Status"  will be retrieved
        //
	case 5:
		stsResult = m_objPCANBasic->GetValue(m_PcanHandle, PCAN_RECEIVE_STATUS, (void*)&iBuffer, sizeof(iBuffer));
		if(stsResult == PCAN_ERROR_OK)
		{
			info.Format("The channel option \"Receive Status\" is %s", (iBuffer == PCAN_PARAMETER_ON) ? "ON" : "OFF");
			IncludeTextMessage(info);
		}
		break;

        // The Number of the CAN-Controller used by a PCAN-Channel
        //
	case 6:
		stsResult = m_objPCANBasic->GetValue(m_PcanHandle, PCAN_CONTROLLER_NUMBER, (void*)&iBuffer, sizeof(iBuffer));
		if(stsResult == PCAN_ERROR_OK)
		{
			info.Format("The CAN Controller number is %d", iBuffer);
			IncludeTextMessage(info);
		}
		break;

        // The activation status for the feature for tracing data will be retrieved
        //
	case 7:
		stsResult = m_objPCANBasic->GetValue(m_PcanHandle, PCAN_TRACE_STATUS, (void*)&iBuffer, sizeof(iBuffer));
		if(stsResult == PCAN_ERROR_OK)
		{
			info.Format("The feature for tracing data is %s", (iBuffer == PCAN_PARAMETER_ON) ? "ON" : "OFF");
			IncludeTextMessage(info);
		}
		break;

        // The activation status of the Channel Identifying procedure will be retrieved
        //
	case 8:
		stsResult = m_objPCANBasic->GetValue(m_PcanHandle, PCAN_CHANNEL_IDENTIFYING, (void*)&iBuffer, sizeof(iBuffer));
		if(stsResult == PCAN_ERROR_OK)
		{
			info.Format("The identification procedure of the selected channel is %s", (iBuffer == PCAN_PARAMETER_ON) ? "ON" : "OFF");
			IncludeTextMessage(info);
		}
		break;

        // The activation status of the Channel Identifying procedure will be retrieved
        //
	case 9:
		stsResult = m_objPCANBasic->GetValue(m_PcanHandle, PCAN_CHANNEL_FEATURES, (void*)&iBuffer, sizeof(iBuffer));
		if(stsResult == PCAN_ERROR_OK)
		{
			info.Format("The channel %s Flexible Data-Rate (CAN-FD)", (iBuffer & FEATURE_FD_CAPABLE) ? "does support" : "DOESN'T SUPPORT");
			IncludeTextMessage(info);
		}
		break;

        // The status of the speed adapting feature will be retrieved
        //
    case 10:
        stsResult = m_objPCANBasic->GetValue(m_PcanHandle, PCAN_BITRATE_ADAPTING, (void*)&iBuffer, sizeof(iBuffer));
        if (stsResult == PCAN_ERROR_OK)
		{
			info.Format("The feature for bit rate adaptation is %s", (iBuffer == PCAN_PARAMETER_ON) ? "ON" : "OFF");
			IncludeTextMessage(info);
		}
        break;

		// The bitrate of the connected channel will be retrieved (BTR0-BTR1 value)
		//
	case 11:
		stsResult = m_objPCANBasic->GetValue(m_PcanHandle, PCAN_BITRATE_INFO, (void*)&iBuffer, sizeof(iBuffer));
		if (stsResult == PCAN_ERROR_OK)
		{
			info.Format("The bit rate of the channel is %.4Xh", iBuffer);
			IncludeTextMessage(info);
		}
		break;

        // The bitrate of the connected FD channel will be retrieved (String value)
        //
    case 12:
        stsResult = m_objPCANBasic->GetValue(m_PcanHandle, PCAN_BITRATE_INFO_FD, strBuffer, 255);
        if (stsResult == PCAN_ERROR_OK)
		{			
            info.Format("The bit rate of the channel is %s", strBuffer);
			IncludeTextMessage(info);
		}
        break;

		// The nominal speed configured on the CAN bus
        //
	case 13:
		stsResult = m_objPCANBasic->GetValue(m_PcanHandle, PCAN_BUSSPEED_NOMINAL, (void*)&iBuffer, sizeof(iBuffer));
		if (stsResult == PCAN_ERROR_OK)
		{
			info.Format("The nominal speed of the channel is %d bit/s", iBuffer);
			IncludeTextMessage(info);
		}
		break;
		// The data speed configured on the CAN bus
        //
	case 14:
		stsResult = m_objPCANBasic->GetValue(m_PcanHandle, PCAN_BUSSPEED_DATA, (void*)&iBuffer, sizeof(iBuffer));
		if (stsResult == PCAN_ERROR_OK)
		{
			info.Format("The data speed of the channel is %d bit/s", iBuffer);
			IncludeTextMessage(info);
		}
		break;
		// The IP address of a LAN channel as string, in IPv4 format
        //
	case 15:
        stsResult = m_objPCANBasic->GetValue(m_PcanHandle, PCAN_IP_ADDRESS, strBuffer, 255);
        if (stsResult == PCAN_ERROR_OK)
		{			
            info.Format("The IP address of the channel is %s", strBuffer);
			IncludeTextMessage(info);
		}
		break;

		// The current parameter is invalid
		//
	default:
		stsResult = PCAN_ERROR_UNKNOWN;
		::MessageBox(NULL, "Wrong parameter code.", "Error!",MB_ICONERROR);
		return;
	}

	// If the function fail, an error message is shown
	//
	if(stsResult != PCAN_ERROR_OK)
		::MessageBox(NULL, GetFormatedError(stsResult), "Error!",MB_ICONERROR);
}


void CPCANBasicExampleDlg::OnClose()
{
	// Release Hardware if need be
	//
	if(btnRelease.IsWindowEnabled())
		OnBnClickedBtnrelease();

	// Close the Read-Event
	//
	CloseHandle(m_hEvent);

	// (Protected environment)
	//
	{
		clsCritical locker(m_objpCS);
		//Free Ressources
		//
		delete m_objPCANBasic;		

		while(m_LastMsgsList->GetCount())
			delete m_LastMsgsList->RemoveHead();
		delete m_LastMsgsList;

		// Delete GPS Record;
		{
			m_GPS_Msg_List.resize(0);
			m_GPS_Msg_List.clear();

			m_GPS_Str_RAW.resize(0);
			m_GPS_Str_RAW.clear();

			m_GPS_CPU_Time.resize(0);
			m_GPS_CPU_Time.clear();

			m_Xbow_Msg_List.clear();
			m_Xbow_Msg_List.resize(0);

			m_Xbow_CPU_Time.resize(0);
			m_Xbow_CPU_Time.clear();

			m_Shutter_Time_Rec.resize(0);
			m_Shutter_Time_Rec.clear();
		}
	}

	// Uninitialize the Critical Section
	//
	FinalizeProtection();

	{
		CloseHandle(m_GPS_Net_Event);
		CloseHandle(m_Xbow_Net_Event);
		CloseHandle(m_Send_Event);
		if (m_Network_hThread)
		{
			InterlockedExchange(&m_Send_Terminated, 1);
			WaitForSingleObject(m_Network_hThread, INFINITE);
			m_Network_hThread = NULL;
		}
	}
	

	CDialog::OnClose();
}

void CPCANBasicExampleDlg::OnBnClickedButtonstatus()
{
	TPCANStatus status;
	CString errorName;
	CString info;

	// Gets the current BUS status of a PCAN Channel.
	//
	status = m_objPCANBasic->GetStatus(m_PcanHandle);

	// Switch On Error Name
	//
	switch(status)
	{
		case PCAN_ERROR_INITIALIZE:
			errorName = "PCAN_ERROR_INITIALIZE";
			break;

		case PCAN_ERROR_BUSLIGHT:
			errorName = "PCAN_ERROR_BUSLIGHT";
			break;

		case PCAN_ERROR_BUSHEAVY: // PCAN_ERROR_BUSWARNING
			errorName = m_IsFD ? "PCAN_ERROR_BUSWARNING" : "PCAN_ERROR_BUSHEAVY";
			break;

        case PCAN_ERROR_BUSPASSIVE: 
            errorName = "PCAN_ERROR_BUSPASSIVE";
            break;

		case PCAN_ERROR_BUSOFF:
			errorName = "PCAN_ERROR_BUSOFF";
			break;

		case PCAN_ERROR_OK:
			errorName = "PCAN_ERROR_OK";
			break;

		default:
			errorName = "See Documentation";
			break;
	}

	// Display Message
	//
	info.Format("Status: %s (%Xh)", errorName, status);
	IncludeTextMessage(info);
}

void CPCANBasicExampleDlg::OnBnClickedButtonreset()
{
	TPCANStatus stsResult;

	// Resets the receive and transmit queues of a PCAN Channel.
	//
	stsResult = m_objPCANBasic->Reset(m_PcanHandle);

	// If it fails, a error message is shown
	//
	if (stsResult != PCAN_ERROR_OK)
		::MessageBox(NULL, GetFormatedError(stsResult), "Error!",MB_ICONERROR);
	else
		IncludeTextMessage("Receive and transmit queues successfully reset");
}


void CPCANBasicExampleDlg::OnBnClickedButtonversion()
{
	TPCANStatus stsResult;
	char buffer[256];
	CString info, strToken;
	int iPos = 0;

	memset(buffer,'\0',255);

	// We get the vesion of the PCAN-Basic API
	//
	stsResult = m_objPCANBasic->GetValue(PCAN_NONEBUS, PCAN_API_VERSION, buffer, 256);
	if (stsResult == PCAN_ERROR_OK)
	{
		info.Format("API Version: %s", buffer);
		IncludeTextMessage(info);
		// We get the driver version of the channel being used
		//
		stsResult = m_objPCANBasic->GetValue(m_PcanHandle, PCAN_CHANNEL_VERSION, buffer, 256);
		if (stsResult == PCAN_ERROR_OK)
		{
			info = buffer;
			IncludeTextMessage("Channel/Driver Version: ");			

			// Because this information contains line control characters (several lines)
			// we split this also in several entries in the Information List-Box
			//						
			strToken = info.Tokenize("\n",iPos); 
			while(strToken != "")
			{			
				strToken.Insert(0,"     * ");
				IncludeTextMessage(strToken);
				strToken = info.Tokenize("\n",iPos);			
			}
		}
	}

	// If the function fail, an error message is shown
	//
	if(stsResult != PCAN_ERROR_OK)
		::MessageBox(NULL, GetFormatedError(stsResult), "Error!",MB_ICONERROR);
}


void CPCANBasicExampleDlg::OnBnClickedButtoninfoclear()
{
	//Reset listBox Content
	listBoxInfo.ResetContent();
	UpdateData(TRUE);
}


CString CPCANBasicExampleDlg::IntToStr(int iValue)
{
	char chToReceive[20];

	_itoa_s(iValue,chToReceive,10);
	return chToReceive;
}

CString CPCANBasicExampleDlg::IntToHex(int iValue, short iDigits)
{	
	CString strTemp, strtest;

	strTemp.Format("%0" + IntToStr(iDigits) + "X",iValue);

	return strTemp;
}
DWORD CPCANBasicExampleDlg::HexTextToInt(CString ToConvert)
{
	DWORD iToReturn = 0;
	int iExp = 0;
	char chByte;

	// The string to convert is empty
	//
	if(ToConvert == "")
		return 0;
	// The string have more than 8 character (the equivalent value
	// exeeds the DWORD capacyty
	//
	if(ToConvert.GetLength() > 8)
		return 0;
	// We convert any character to its Upper case
	//
	ToConvert = ToConvert.MakeUpper();

	try
	{
		// We calculate the number using the Hex To Decimal formula
		//
		for(int i= ToConvert.GetLength()-1; i >= 0; i--){
			chByte = ToConvert[i];
			switch(int(chByte)){
				case 65:
					iToReturn += (DWORD)(10*pow(16.0f,iExp));
					break;
				case 66:
					iToReturn += (DWORD)(11*pow(16.0f,iExp));
					break;
				case 67:
					iToReturn += (DWORD)(12*pow(16.0f,iExp));
					break;
				case 68:
					iToReturn += (DWORD)(13*pow(16.0f,iExp));
					break;
				case 69:
					iToReturn += (DWORD)(14*pow(16.0f,iExp));
					break;
				case 70:
					iToReturn += (DWORD)(15*pow(16.0f,iExp));
					break;
				default:
					if((int(chByte) <48)||(int(chByte)>57))
						return -1;
					iToReturn += (DWORD)(atoi(&chByte)*pow(16.0f,iExp));
					break;

			}
			iExp++;
		}
	}
	catch(...)
	{
		// Error, return 0
		//
		return 0;
	}

	return iToReturn;
}

void CPCANBasicExampleDlg::CheckHexEditBox(CString* txtData)
{
	int iTest;

	txtData->MakeUpper();

	// We test if the given ID is a valid hexadecimal number.
	// 
	iTest = HexTextToInt(*txtData);
	if(iTest > 0)
		*txtData = IntToHex(iTest,2);
	else
		*txtData = "00";
}

int CPCANBasicExampleDlg::AddLVItem(CString Caption)
{
	LVITEM NewItem;

	// Initialize LVITEM
	//
	NewItem.mask = LVIF_TEXT;
	NewItem.iSubItem = 0;	
	NewItem.pszText = Caption.GetBuffer();
	NewItem.iItem = lstMessages.GetItemCount();

	// Insert it in the message list
	//
	lstMessages.InsertItem(&NewItem);
	return NewItem.iItem;	
}

int CPCANBasicExampleDlg::ADDLVItem_GPS(CString Caption)
{
	LVITEM NewItem;

	// Initialize LVITEM
	//
	NewItem.mask = LVIF_TEXT;
	NewItem.iSubItem = 0;
	NewItem.pszText = Caption.GetBuffer();
	NewItem.iItem = lstMessages_GPS.GetItemCount();

	// Insert it in the message list
	//
	lstMessages_GPS.InsertItem(&NewItem);
	return NewItem.iItem;
}

void CPCANBasicExampleDlg::SetTimerRead(bool bEnable)
{
	// Init Timer
	//
	if(bEnable)
		m_tmrRead = SetTimer(1, 50, 0);
	else
	{
		//Delete Timer
		KillTimer(m_tmrRead);
		m_tmrRead = 0;
	}
}

void CPCANBasicExampleDlg::SetTimerDisplay(bool bEnable)
{
	if(bEnable)
		m_tmrDisplay = SetTimer(2, 100, 0);
	else
	{
		KillTimer(m_tmrDisplay);
		m_tmrDisplay = 0;
	}
}

void CPCANBasicExampleDlg::DisplayMessages()
{
	POSITION pos;
	int iCurrentItem, iCurrentItem_GPS;
	MessageStatus *msgStatus;
	CString strTempCount;

    // We search if a message (Same ID and Type) is 
    // already received or if this is a new message
	// (in a protected environment)
	//
	{
		clsCritical locker(m_objpCS);

		pos = m_LastMsgsList->GetHeadPosition();
		for(int i=0; i < m_LastMsgsList->GetCount(); i++)
		{
			msgStatus = (MessageStatus*)m_LastMsgsList->GetNext(pos);
			if(msgStatus->MarkedAsUpdated)
			{
				msgStatus->MarkedAsUpdated = false;

				iCurrentItem = msgStatus->Position;

				strTempCount = lstMessages.GetItemText(iCurrentItem,MSG_COUNT);
				CString DATA = msgStatus->DataString;

				lstMessages.SetItemText(iCurrentItem,MSG_LENGTH,IntToStr(GetLengthFromDLC(msgStatus->CANMsg.DLC, !(msgStatus->CANMsg.MSGTYPE & PCAN_MESSAGE_FD))));
				lstMessages.SetItemText(iCurrentItem,MSG_COUNT,IntToStr(msgStatus->Count));
				lstMessages.SetItemText(iCurrentItem, MSG_TIME, msgStatus->TimeString);
				lstMessages.SetItemText(iCurrentItem, MSG_DATA, DATA);

				CString ID = msgStatus->IdString;
				ID.Truncate(ID.GetLength() - 1);
				int ID_NUM = HexTextToUnsigned(ID);
				if (ID_NUM >= 0x301 && ID_NUM <= 0x305)
				{
					iCurrentItem_GPS = min(ID_NUM - 0x301, lstMessages_GPS.GetItemCount() - 1);
					DisplayGPSInformation(DATA, iCurrentItem_GPS, ID_NUM);
				}
			}
		}

		CString strTemp = "";
		// EnterCriticalSection(&m_xBow_CriticalSection);
		strTemp = m_Xbow_Msg_Latest;
		// LeaveCriticalSection(&m_xBow_CriticalSection);

		if (!m_Xbow_AddedItem && strTemp != "")
		{
			ADDLVItem_GPS(strTemp);
			m_Xbow_AddedItem = true;
		}
		else if (strTemp != "")
		{
			iCurrentItem_GPS = min(0x306 - 0x301, lstMessages_GPS.GetItemCount() - 1);
			int ID_NUM = 0x306;
			DisplayGPSInformation(strTemp, iCurrentItem_GPS, ID_NUM);
		}
	}
}

void CPCANBasicExampleDlg::InsertMsgEntry(TPCANMsgFD NewMsg, TPCANTimestampFD timeStamp)
{
	MessageStatus *msgStsCurrentMsg;
	int iCurrentItem, iCurrentItem_GPS;

	// (Protected environment)
	//
	{
		clsCritical locker(m_objpCS);

		// We add this status in the last message list
		//
		msgStsCurrentMsg = new MessageStatus(NewMsg, timeStamp, lstMessages.GetItemCount());
		msgStsCurrentMsg->ShowingPeriod = chbReadingTimeStamp.GetCheck() > 0;
		m_LastMsgsList->AddTail(msgStsCurrentMsg);

		CString ID = msgStsCurrentMsg->IdString;
		CString Data = msgStsCurrentMsg->DataString;

		// Add the new ListView Item with the Type of the message
		//
		iCurrentItem = AddLVItem(msgStsCurrentMsg->TypeString);
		// We set the ID of the message
        //
		lstMessages.SetItemText(iCurrentItem, MSG_ID, ID);
        // We set the length of the Message
        //		
		lstMessages.SetItemText(iCurrentItem,MSG_LENGTH,IntToStr(GetLengthFromDLC(NewMsg.DLC, !(NewMsg.MSGTYPE & PCAN_MESSAGE_FD))));
        // we set the message count message (this is the First, so count is 1)
        //
		lstMessages.SetItemText(iCurrentItem,MSG_COUNT,IntToStr(msgStsCurrentMsg->Count));
        // Add timestamp information
        //
		lstMessages.SetItemText(iCurrentItem, MSG_TIME, msgStsCurrentMsg->TimeString);
		// We set the data of the message. 	
        //
		lstMessages.SetItemText(iCurrentItem, MSG_DATA, Data);

		{
			m_GPS_Msg_List.push_back(GPS_MK_PAIR(ID, Data));
			boost::posix_time::ptime now = boost::posix_time::microsec_clock::local_time();
			boost::posix_time::time_duration diff = now - time_t_epoch;

			if (m_GPS_Msg_List.size() == 1) InterlockedExchange(&m_GPS_Is_First, 1);

			ID.Truncate(ID.GetLength() - 1);
			const int ID_NUM = HexTextToUnsigned(ID);
			if (ID_NUM >= 0x301 && ID_NUM <= 0x305)
			{
				iCurrentItem_GPS = ADDLVItem_GPS(msgStsCurrentMsg->TypeString);
			}
		}
	}
}

void CPCANBasicExampleDlg::InitGPSConfig()
{
	//Custom GPS Recorder
	m_GPS_Num_Count = 0;
	m_GPS_Send_Num_Count = 0;
	m_GPS_First_Distance = -1;
	m_GPS_Distance_Offset = 0;

	m_GPS_Recorder = "Time\tX\tY\tSpeed\tHeading\tWGS84\tVerticalV\tTrigDist\tLongAcc\tLatAcc\tStatus\tTrigTime\tTrigV\tDistance\tSats.\tRAW\tID\tCPUTIME\n";
	m_GPS_Str_Time = "";
	m_GPS_Str_X = "";
	m_GPS_Str_Y = "";
	m_GPS_Str_Sats = "";
	m_GPS_Str_Velocity = "";
	m_GPS_Str_Heading = "";
	m_GPS_Str_WGS84 = "";
	m_GPS_Str_VerticalV = "";
	m_GPS_Str_TrigDist = "";
	m_GPS_Str_LongAcc = "";
	m_GPS_Str_LatACC = "";
	m_GPS_Str_Status = "";
	m_GPS_Str_TrigTime = "";
	m_GPS_Str_TrigV = "";
	m_GPS_Str_Distance = "";

	m_GPS_Str_RAW.clear();
	m_GPS_Str_RAW.resize(GPS_DATA_COUNT,"");

	if (!m_GPS_Msg_List.empty()) m_GPS_Msg_List.clear();
	if (!m_GPS_CPU_Time.empty()) m_GPS_CPU_Time.clear();

	if (m_GPS_Msg_List.capacity() < GPS_MSG_NUMS)
	{
		try
		{
			m_GPS_Msg_List.reserve(GPS_MSG_NUMS);
		}
		catch (std::exception &e)
		{
			::MessageBox(NULL, _T(e.what()), "ERROR", MB_ICONERROR);
		}
	}
	if (m_GPS_CPU_Time.capacity() < GPS_MSG_NUMS)
	{
		try
		{
			m_GPS_CPU_Time.reserve(GPS_MSG_NUMS);
		}
		catch (std::exception &e)
		{
			::MessageBox(NULL, _T(e.what()), "ERROR", MB_ICONERROR);
		}
	}
	if (m_GPS_Msg_List.capacity() >= GPS_MSG_NUMS && m_GPS_CPU_Time.capacity() >= GPS_MSG_NUMS)
	{
		IncludeTextMessage("GPS Message Memory successfully allocated (30 mins)");
	}

	m_GPS_Msg_Latest = "";
	m_GPS_Msg_TobeSent = "";
	m_Msg_ReadytoGo = "";
	m_GPS_Msg_Sent_Num = 0;
}

DWORD WINAPI CPCANBasicExampleDlg::CallNetworkOutputFunc(LPVOID lpParam)
{
	CPCANBasicExampleDlg* dialog = (CPCANBasicExampleDlg*)lpParam;
	dialog->m_GPS_Msg_Sent_Num = 0;
	dialog->m_Xbow_Msg_Sent_Num = 0;

// 	DWORD result;
// 	
// 	bool btnpressed(false);
// 	while (result = WaitForSingleObject(dialog->m_Send_Event, 200))
// 	{
// 		if (result == WAIT_OBJECT_0)
// 		{
// 			btnpressed = true;
// 		}
// 	}
	
	while (!dialog->m_Send_Terminated)
	{
		dialog->NetworkOutFunc(NULL);
	}

	return 0;
}

DWORD WINAPI CPCANBasicExampleDlg::NetworkOutFunc(LPVOID lpParam)
{
	boost::asio::io_service io;
	boost::asio::ip::tcp::acceptor acceptor(io, boost::asio::ip::tcp::endpoint(boost::asio::ip::tcp::v4(), 55555));
	boost::asio::ip::tcp::socket socket(io);
	boost::asio::deadline_timer timer(io);

	bool isconnected = true;

	acceptor.async_accept(socket, boost::bind(&CPCANBasicExampleDlg::NetworkAccpetorHandler, this, &socket, &timer, boost::asio::placeholders::error));
	timer.expires_from_now(boost::posix_time::millisec(5000));
	timer.async_wait(boost::bind(&CPCANBasicExampleDlg::ConnectionTimeOut, this, &socket, &isconnected, boost::asio::placeholders::error));

	io.run();

	if (!isconnected)
	{
		return CONNECTION_ERROR;
	}

	return 0;
}

void CPCANBasicExampleDlg::ConnectionTimeOut(boost::asio::ip::tcp::socket *socket, bool *isconnected, const boost::system::error_code &ec)
{
	if (m_Send_Terminated)
	{
 		socket->close();
		return;
	}

	if (!ec)
	{
		*isconnected = false;
		try
		{
 			socket->close();
		}
		catch (std::exception &ec)
		{
			::MessageBox(NULL, _T(ec.what()), "ERROR", MB_ICONERROR);
		}
		btnSend.SetWindowText(_T("Connection Time-Out"));
	}

	else
	{
	}
}

void CPCANBasicExampleDlg::NetworkAccpetorHandler(boost::asio::ip::tcp::socket *socket, boost::asio::deadline_timer *timer, const boost::system::error_code &ec)
{
	if (ec)
	{
		return;
	}

	timer->cancel();
	btnSend.SetWindowText(_T("Sending..."));

	//boost::asio::deadline_timer writeTimer(socket->get_io_service());
	boost::system::error_code ercode;
	size_t bytetransferred = 0;
	while (!m_Send_Terminated)
	{

		if (WaitForSingleObject(m_GPS_Net_Event, GPS_LATENCY) == WAIT_OBJECT_0)
		{
			{
 				clsCritical locker(m_objpCS);
				m_Msg_ReadytoGo = m_GPS_Msg_TobeSent;
			}
			try
			{
				bytetransferred = socket->write_some(boost::asio::buffer(m_Msg_ReadytoGo), ercode);
			}
			catch (std::exception &e)
			{
				::MessageBox(NULL, e.what(), "ERROR", MB_ICONERROR);
				break;
			}
			if (ec || !bytetransferred)
			{
				break;
			}
			InterlockedIncrement(&m_GPS_Msg_Sent_Num);
// 			boost::asio::async_write(*socket, boost::asio::buffer(m_Msg_ReadytoGo), boost::bind(&CPCANBasicExampleDlg::NetworkWriteHandler, this, timer, boost::asio::placeholders::error, boost::asio::placeholders::bytes_transferred));
// 			timer->expires_from_now(boost::posix_time::millisec(100));
// 			timer->async_wait(boost::bind(&CPCANBasicExampleDlg::SendTimeOut, this, socket, boost::asio::placeholders::error));
		}
// 		if (WaitForSingleObject(m_Xbow_Net_Event, XBOW_LATENCY) == WAIT_OBJECT_0)
// 		{
// 			{
//  				clsCritical locker(m_objpCS);
// 				m_Msg_ReadytoGo = m_Xbow_Msg_TobeSent;
// 			}
// 			try
// 			{
// 				bytetransferred = socket->write_some(boost::asio::buffer(m_Msg_ReadytoGo), ercode);
// 			}
// 			catch (std::exception &e)
// 			{
// 				::MessageBox(NULL, e.what(), "ERROR", MB_ICONERROR);
// 				break;
// 			}
// 			if (ercode || !bytetransferred)
// 			{
// 				break;
// 			}
// 			InterlockedIncrement(&m_Xbow_Msg_Sent_Num);
// // 			boost::asio::async_write(*socket, boost::asio::buffer(m_Msg_ReadytoGo), boost::bind(&CPCANBasicExampleDlg::NetworkWriteHandler, this, timer, boost::asio::placeholders::error, boost::asio::placeholders::bytes_transferred));
// // 			timer->expires_from_now(boost::posix_time::millisec(100));
// // 			timer->async_wait(boost::bind(&CPCANBasicExampleDlg::SendTimeOut, this, socket, boost::asio::placeholders::error));
// 		}
	}
	try
	{
		socket->close();
	}
	catch (std::exception &e)
	{
		::MessageBox(NULL, e.what(), "ERROR", MB_ICONERROR);
	}
}

void CPCANBasicExampleDlg::NetworkWriteHandler(boost::asio::deadline_timer *timer, const boost::system::error_code &ec, unsigned bytestransferred)
{
	if (!ec && bytestransferred)
	{
		timer->cancel();
		InterlockedIncrement(&m_GPS_Msg_Sent_Num);
		InterlockedIncrement(&m_Xbow_Msg_Sent_Num);
	}
}

void CPCANBasicExampleDlg::SendTimeOut(boost::asio::ip::tcp::socket *socket, const boost::system::error_code &ec)
{
	if (!ec)
	{
		socket->close();
		InterlockedExchange(&m_Send_Terminated, 1);
	}
}

DWORD WINAPI CPCANBasicExampleDlg::CallReadXbowDataThreadFunc(LPVOID lpParam)
{
	CPCANBasicExampleDlg* dialog = (CPCANBasicExampleDlg*)lpParam;

	return dialog->XbowDataReadThreadFunc(NULL);
}

DWORD WINAPI CPCANBasicExampleDlg::XbowDataReadThreadFunc(LPVOID lpParam)
{
	const char *SN = m_CurrentComPort.c_str();
	FT_HANDLE ftHandle;
	FT_STATUS ftStatus;
	CString ErrorMsg;

	ftStatus = FT_OpenEx((void*)SN, FT_OPEN_BY_SERIAL_NUMBER, &ftHandle);
	if (ftStatus != FT_OK)
	{
		ErrorMsg.Format("%s\n","Port Open Failed");
		::MessageBox(NULL, ErrorMsg, "Error!", MB_ICONERROR);
	}

	ftStatus = FT_SetBaudRate(ftHandle, 38400);
	if (ftStatus == FT_OK) IncludeTextMessage("Set Baud Rate 38400");

	ftStatus = FT_SetDataCharacteristics(ftHandle, FT_BITS_8, FT_STOP_BITS_1, FT_PARITY_NONE);
	if (ftStatus == FT_OK) IncludeTextMessage("Bit = 8, Stop Bits = 1, No Parity");

	FT_SetFlowControl(ftHandle, FT_FLOW_NONE, FT_FLOW_XON_XOFF, FT_FLOW_XON_XOFF);
	if (ftStatus == FT_OK) IncludeTextMessage("No Flow Control");

	ftStatus = FT_SetTimeouts(ftHandle, 1000, 1000);
	if (ftStatus == FT_OK) IncludeTextMessage("Read Timeout = 1000, Write Timeout = 1000");

	unsigned char w;
	DWORD BytesWritten;

	unsigned char flag[1024];
	DWORD RxBytes;
	DWORD BytesReceived = 65535;

	while (BytesReceived != 0)
	{
		w = 0x50;
		ftStatus = FT_Write(ftHandle, &w, sizeof(w), &BytesWritten);
		if (ftStatus == FT_OK)
			if (BytesWritten == sizeof(w))
			{
				IncludeTextMessage("Write 'P' OK");
			}
			else
			{
				IncludeTextMessage("Write 'P' Failed");
			}
		Sleep(20);

		RxBytes = 256;
		ftStatus = FT_Read(ftHandle, flag, RxBytes, &BytesReceived);
		if (ftStatus == FT_OK)
			if (BytesReceived == RxBytes)
			{
				CString temp;
				temp.Format("%s%s", "Read OK, Read:", flag);
				IncludeTextMessage(temp);
			}
			else
			{
				CString temp;
				temp.Format("%s%d", "Read Failed, Read:", BytesReceived);
				IncludeTextMessage(temp);
			}
	}

	flag[0] = 0;
	RxBytes = 1;
	while (flag[0] != 0x48)
	{
		w = 0x52;
		ftStatus = FT_Write(ftHandle, &w, sizeof(w), &BytesWritten);
		if (ftStatus == FT_OK)
			if (BytesWritten == sizeof(w))
			{
				IncludeTextMessage("Write 'R' OK");
			}
			else
			{
				IncludeTextMessage("Write 'R' Failed");
			}
		Sleep(20);

		ftStatus = FT_Read(ftHandle, flag, RxBytes, &BytesReceived);
		if (ftStatus == FT_OK)
			if (BytesReceived == RxBytes)
			{
// 				printf("Read OK\n");
// 				display(flag, 1);
				CString temp;
				temp.Format("%s%c", "Read OK, Read:", flag[0]);
				IncludeTextMessage(temp);
			}
			else
			{
				CString temp;
				temp.Format("%s%d", "Read Failed, Read:", BytesReceived);
 				IncludeTextMessage(temp);
			}
	}

	w = 0x09;
	ftStatus = FT_Write(ftHandle, &w, sizeof(w), &BytesWritten);
	if (ftStatus == FT_OK)
		if (BytesWritten == sizeof(w))
		{
			IncludeTextMessage("Write '0x09' OK");
		}
		else
		{
			IncludeTextMessage("Write '0x09' Failed");
		}
	Sleep(20);

	RxBytes = 22;
	BytesReceived = 65535;
	while (BytesReceived != 0)
	{
		ftStatus = FT_Read(ftHandle, flag, RxBytes, &BytesReceived);

	}

	w = 0x57;
	ftStatus = FT_Write(ftHandle, &w, sizeof(w), &BytesWritten);
	if (ftStatus == FT_OK)
		if (BytesWritten == sizeof(w))
		{
			IncludeTextMessage("Write 'W' OK");
		}
		else
		{
			IncludeTextMessage("Write 'W' Failed");
		}
	Sleep(20);

	RxBytes = 22;
	BytesReceived = 65535;
	while (BytesReceived != 0)
	{
		ftStatus = FT_Read(ftHandle, flag, RxBytes, &BytesReceived);
	}

	if (m_zerotime > 0)
	{
		char zero[] = { 0x7A, 0x05 };
		ftStatus = FT_Write(ftHandle, zero, sizeof(zero), &BytesWritten);
		if (ftStatus == FT_OK)
			if (BytesWritten == sizeof(zero))
			{
				IncludeTextMessage("Write Zero Command OK");
			}
			else
			{
				IncludeTextMessage("Write Zero Command Failed");
			}
		Sleep(20);

		RxBytes = 1;
		ftStatus = FT_Read(ftHandle, flag, RxBytes, &BytesReceived);
		bool ifZeroed = false;
		if (ftStatus == FT_OK)
			if (BytesReceived == RxBytes)
			{
				CString temp;
				temp.Format("%s%c", "Read OK, Read:", flag[0]);
				IncludeTextMessage(temp);
				ifZeroed = true;
			}
			else
			{
				CString temp;
				temp.Format("%s%d", "Read Failed, Read:", BytesReceived);
				IncludeTextMessage(temp);
			}

		if (ifZeroed)
		{
			IncludeTextMessage("Zeroing...Please keep the Gyro MOTIONLESS for about 5 minutes...");
			Sleep(1000*60*5);
		}
	}

	if (m_erate > 0)
	{
		char erate[] = { 0x54, (char)m_erate};
		ftStatus = FT_Write(ftHandle, erate, sizeof(erate), &BytesWritten);
		if (ftStatus == FT_OK)
			if (BytesWritten == sizeof(erate))
			{
				IncludeTextMessage("Write Erate Command OK");
			}
			else
			{
				IncludeTextMessage("Write Erate Command Failed");
			}
		Sleep(20);
	}

	w = 0x61;
	ftStatus = FT_Write(ftHandle, &w, sizeof(w), &BytesWritten);
	if (ftStatus == FT_OK)
		if (BytesWritten == sizeof(w))
		{
			IncludeTextMessage("Write 'a' OK");
		}
		else
		{
			IncludeTextMessage("Write 'a' Failed");
		}
	Sleep(20);

	RxBytes = 1;
	ftStatus = FT_Read(ftHandle, flag, RxBytes, &BytesReceived);
	if (ftStatus == FT_OK)
		if (BytesReceived == RxBytes)
		{
			CString temp;
			temp.Format("%s%c", "Read OK, Read:", flag[0]);
			IncludeTextMessage(temp);
		}
		else
		{
			CString temp;
			temp.Format("%s%d", "Read Failed, Read:", BytesReceived);
			IncludeTextMessage(temp);
		}

	w = 0x47;
	ftStatus = FT_Write(ftHandle, &w, sizeof(w), &BytesWritten);
	if (ftStatus == FT_OK)
		if (BytesWritten == sizeof(w))
		{
			IncludeTextMessage("Write 'G' OK");
		}
		else
		{
			IncludeTextMessage("Write 'G' Failed");
		}
	Sleep(20);

	RxBytes = 45;
	ftStatus = FT_Read(ftHandle, flag, RxBytes, &BytesReceived);

	w = 0x43;
	ftStatus = FT_Write(ftHandle, &w, sizeof(w), &BytesWritten);
	if (ftStatus == FT_OK)
		if (BytesWritten == sizeof(w))
		{
			IncludeTextMessage("Write 'C' OK");
		}
		else
		{
			IncludeTextMessage("Write 'C' Failed");
		}
	Sleep(20);

	RxBytes = 45;
	unsigned num = 0;
	unsigned char content[450];
	memset(content, 0, sizeof(content));

	ftStatus = FT_Read(ftHandle, content, RxBytes, &BytesReceived);
	ftStatus = FT_Read(ftHandle, content, RxBytes, &BytesReceived);
	ftStatus = FT_Read(ftHandle, content, RxBytes, &BytesReceived);

	BytesReceived = 65535;
	while (BytesReceived != RxBytes || ftStatus != FT_OK)
	{
		IncludeTextMessage("Reading...");
		ftStatus = FT_Read(ftHandle, content, RxBytes, &BytesReceived);
	}

	for (int i = 0; i < 45 / 2; i++)
	{
		const unsigned char &temp = content[i];
		const unsigned char &tempend = content[i + 22];
		if (temp == 0xFF && tempend == 0xFF)
		{
			unsigned char c = 0;
			int j = i;
			for (j = i + 1; j < i + 21; j++)
			{
				c += content[j];
			}
			c = c % 255;
			if (c == content[j])
			{
				num = (45 - j - 1);
				num = (num <= 22) ? 22 - num : num % 22;
				break;
			}
		}
	}

	RxBytes = num;
	BytesReceived = 65535;
	while (BytesReceived != RxBytes || ftStatus != FT_OK)
	{
		IncludeTextMessage("Reading...");
		ftStatus = FT_Read(ftHandle, content, RxBytes, &BytesReceived);
	}

	CString tempstr, tempstr2;
	const unsigned perdata = 1;
	unsigned resetflag = 0;
	while (!m_XbowTerminated)
	{
		BytesReceived = 65535;
		RxBytes = 22 * perdata;
		while (BytesReceived != RxBytes || ftStatus != FT_OK)
		{
			ftStatus = FT_Read(ftHandle, content, RxBytes, &BytesReceived);
		}

		for (int k = 0; k < perdata; k++)
		{
			int i = k * 22 + 1;
			int ii = i + 20;
			unsigned char c = 0;
			tempstr.Format("%02X ", content[i - 1]);
			tempstr2 += tempstr;
			for (; i < ii; i++)
			{
				c += content[i];
				tempstr.Format("%02X ", content[i]);
				tempstr2 += tempstr;
			}
			c = c % 255;
			tempstr.Format("%02X", content[ii]);
			tempstr2 += tempstr;

			// 			EnterCriticalSection(&m_xBow_CriticalSection);
			// 			if (c == content[ii] && content[0] == 0xFF)
			// 			{
			// 				m_Xbow_Msg_List.push_back(tempstr2);
			// 				m_Xbow_Msg_Latest = tempstr2;
			// 				++m_Xbow_Effictive_Count;
			// 			}
			// 			++m_Xbow_Count;
			// 			resetflag = m_Xbow_Algin;
			// 			LeaveCriticalSection(&m_xBow_CriticalSection);

			{
				clsCritical locker(m_objpCS);
				if (c == content[ii] && content[0] == 0xFF)
				{
					m_Xbow_Msg_List.push_back(tempstr2);
					boost::posix_time::ptime now = boost::posix_time::microsec_clock::local_time();
					boost::posix_time::time_duration diff = now - time_t_epoch;
					m_Xbow_CPU_Time.push_back(diff.total_milliseconds());
					m_Xbow_Msg_Latest = tempstr2;
					GetGPSXbowInformation(m_Xbow_Msg_Latest, 0x306);
					++m_Xbow_Effictive_Count;
				}
				++m_Xbow_Count;
				resetflag = m_Xbow_Algin;
			}

			tempstr.Empty();
			tempstr2.Empty();
		}

		if (resetflag)
		{			
			FT_Read(ftHandle, content, 45, &BytesReceived);
			FT_Read(ftHandle, content, 45, &BytesReceived);
			
			RxBytes = 45;
			BytesReceived = 65535;
			while (BytesReceived != RxBytes || ftStatus != FT_OK)
			{
				ftStatus = FT_Read(ftHandle, content, RxBytes, &BytesReceived);
			}

			for (int i = 0; i < 45 / 2; i++)
			{
				const unsigned char &temp = content[i];
				const unsigned char &tempend = content[i + 22];
				if (temp == 0xFF && tempend == 0xFF)
				{
					unsigned char c = 0;
					int j = i;
					for (j = i + 1; j < i + 21; j++)
					{
						c += content[j];
					}
					c = c % 255;
					if (c == content[j])
					{
						num = (45 - j - 1);
						num = (num <= 22) ? 22 - num : num % 22;
						break;
					}
				}
			}

			RxBytes = num;
			BytesReceived = 65535;

			while (BytesReceived != RxBytes || ftStatus != FT_OK)
			{
				ftStatus = FT_Read(ftHandle, content, num, &BytesReceived);
			}

// 			EnterCriticalSection(&m_xBow_CriticalSection);
// 			m_Xbow_Algin = 0;
// 			LeaveCriticalSection(&m_xBow_CriticalSection);
			InterlockedExchange(&m_Xbow_Algin, 0);
		}
	}

	ftStatus = FT_Close(ftHandle);
	if (ftStatus != FT_OK)
	{
		InterlockedExchange(&m_XbowTerminated, 1);
		return 1;
	}

	return 0;
}

void CPCANBasicExampleDlg::ConnectCrossXbow()
{
	CString strTemp;
	int item = ccbHwXbow.GetCurSel();
	if (item != CB_ERR)
		ccbHwXbow.GetLBText(item, strTemp);

// 	strTemp.Replace(" ", "");
// 	strTemp = strTemp.Mid(0, 4);

	CT2CA pszConvertedAnsiString(strTemp);
	std::string strStd(pszConvertedAnsiString);
	m_CurrentComPort = strTemp;

	InterlockedExchange(&m_XbowTerminated, 0);

	if (!m_Xbow_hThread)
	{
		m_Xbow_hThread = CreateThread(NULL, NULL, CPCANBasicExampleDlg::CallReadXbowDataThreadFunc, (LPVOID)this, NULL, NULL);
	}
	else
	{
		::MessageBox(NULL, "Thread already exists", "ERROR", MB_ICONERROR);
		return;
	}
	if (!m_Xbow_hThread)
	{
		::MessageBox(NULL, "Cannot create Thread", "ERROR", MB_ICONERROR);
		return;
	}
}

void CPCANBasicExampleDlg::InitCrossXbow()
{
	if (!m_Xbow_Msg_List.empty()) m_Xbow_Msg_List.clear();
	if (!m_Xbow_CPU_Time.empty()) m_Xbow_CPU_Time.clear();

	if (m_Xbow_Msg_List.capacity() < XBOW_MSG_NUMS)
	{
		try
		{
			m_Xbow_Msg_List.reserve(XBOW_MSG_NUMS);
		}
		catch (std::exception &e)
		{
			::MessageBox(NULL, _T(e.what()), "ERROR", MB_ICONERROR);
		}
	}
	if (m_Xbow_CPU_Time.capacity() < XBOW_MSG_NUMS)
	{
		try
		{
			m_Xbow_CPU_Time.reserve(XBOW_MSG_NUMS);
		}
		catch (std::exception &e)
		{
			::MessageBox(NULL, _T(e.what()), "ERROR", MB_ICONERROR);
		}
	}
	if (m_Xbow_Msg_List.capacity() >= XBOW_MSG_NUMS && m_Xbow_CPU_Time.capacity() >= XBOW_MSG_NUMS)
	{
		IncludeTextMessage("Accelerometer Message Memory successfully allocated (30 mins)");
	}

	m_CurrentComPort = "";
	InterlockedExchange(&m_XbowTerminated, 1);
	m_Xbow_hThread = NULL;
	m_Xbow_Count = 0;
	m_Xbow_Effictive_Count = 0;
	m_Xbow_Msg_Latest = "";
	m_Xbow_AddedItem = false;
	m_Xbow_Added2Item = false;
	m_Xbow_Algin = 0;
	m_Xbow_Msg_TobeSent = "";
	m_Xbow_Msg_Sent_Num = 0;
	m_GPS_Begin_Time = 0;
	m_GPS_Is_First = 0;

	m_Xbow_Recorder.clear(); 
	m_Xbow_RollAngle = ""; m_Xbow_PitchAngle = "";
	m_Xbow_RollRate = ""; m_Xbow_PitchRate = "";
	m_Xbow_YawRate = ""; m_Xbow_AccX = ""; m_Xbow_AccY = ""; m_Xbow_AccZ = "", m_Xbow_Tem = "";
	m_Xbow_Time = ""; m_Xbow_Badratio = "";

	m_Xbow_Recorder = "Roll_Angle\tPitch_Angle\tRoll_Rate\tPitch_Rate\tYaw_Rate\tAcc_X\tACC_Y\tACC_Z\tTempature\tTime\tBadRatio\tRaw\tCPUTIME\n";

	cbbHwsType.ShowWindow(SW_HIDE);
	cbbIO.ShowWindow(SW_HIDE);
	cbbInterrupt.ShowWindow(SW_HIDE);
	chbCanFD.ShowWindow(SW_HIDE);

	labelIOPort.ShowWindow(SW_HIDE);
	labelInterrupt.ShowWindow(SW_HIDE);

	cbbBaudrates.EnableWindow(FALSE);
	lstMessages.ShowWindow(SW_HIDE);

	labelHwType.SetWindowTextA("HardWare:");
}

void CPCANBasicExampleDlg::ReadPortsCallback(bool *isread, const boost::system::error_code& error, const unsigned &bytes_transferred, boost::asio::deadline_timer *timer)
{
	*isread = !(error || bytes_transferred == 0);

	if (*isread)
 		(*timer).cancel();
}

void CPCANBasicExampleDlg::EnumComPorts()
{
	if(FT232RCOM_Exist(FT232SN))
		ccbHwXbow.AddString(FT232SN);

	if (ccbHwXbow.GetCount())
	{
		ccbHwXbow.SetCurSel(0);
	}
}

void CPCANBasicExampleDlg::StoreAccMsgList()
{
// 	EnterCriticalSection(&m_xBow_CriticalSection);
// 
// 	std::vector<const CString>::const_iterator msgI = m_Xbow_Msg_List.cbegin();
// 	const unsigned &NUM_ELEMENTS = m_Xbow_Msg_List.size();
// 	unsigned i = 0;
// 	const unsigned ID_NUM = 0x306;
// 
// 	while (i != m_Xbow_Msg_List.size())
// 	{
// 		CString Data;
// 		Data = (*msgI);
// 		DisplayGPSInformation(Data, 0, ID_NUM, true);
// 
// 		++i;
// 		++msgI;
// 	}
// 
// 	LeaveCriticalSection(&m_xBow_CriticalSection);

	clsCritical locker(m_objpCS);

	std::vector<CString>::const_iterator msgI = m_Xbow_Msg_List.cbegin();
	std::vector<CString>::const_iterator msgEND = m_Xbow_Msg_List.cend();
	const unsigned ID_NUM = 0x306;

	std::vector<__int64>::const_iterator timeI = m_Xbow_CPU_Time.cbegin();
	std::vector<__int64>::const_iterator timeEnd = m_Xbow_CPU_Time.cend();
	CString time;
	std::string temp;
	while (msgI != msgEND)
	{
		CString Data;
		Data = (*msgI);
		DisplayGPSInformation(Data, 0, ID_NUM, true);
		if (timeI != timeEnd)
		{
			time.Format("\t%I64u", *timeI);
			temp = CT2CA(time);
			++timeI;
		}
		else
		{
			temp = "CPU_TIME_MISSING";
		}
		m_Xbow_Recorder += temp;
		m_Xbow_Recorder += "\n";

		++msgI;
	}
}

void CPCANBasicExampleDlg::StoreMsgList()
{
	clsCritical locker(m_objpCS);

	int iCurrentItem_GPS;

	std::vector<std::pair<CString, CString>>::const_iterator msg_i = m_GPS_Msg_List.cbegin();
	std::vector<std::pair<CString, CString>>::const_iterator msg_END = m_GPS_Msg_List.cend();
	std::vector<uint64_t>::const_iterator timeI = m_GPS_CPU_Time.cbegin();
	std::vector<uint64_t>::const_iterator timeEnd = m_GPS_CPU_Time.cend();

	CString time;
	std::string temp;
	while (msg_i != msg_END)
	{
		CString ID = (*msg_i).first;
		CString Data;
		ID.Truncate(ID.GetLength() - 1);
		int ID_NUM = HexTextToUnsigned(ID);
		if (ID_NUM >= 0x301 && ID_NUM <= 0x305)
		{
			iCurrentItem_GPS = min(ID_NUM - 0x301, lstMessages_GPS.GetItemCount() - 1);
			Data = (*msg_i).second;
			DisplayGPSInformation(Data, iCurrentItem_GPS, ID_NUM, true);
			if (timeI != timeEnd)
			{
				time.Format("%I64u", *timeI);
				temp = CT2CA(time);
				++timeI;
			}
			else
			{
				temp = "CPU_TIME_MISSING";
			}
			m_GPS_Recorder += temp;
			m_GPS_Recorder += "\n";
		}

		++msg_i;
	}
}

void CPCANBasicExampleDlg::GetGPSXbowInformation(CString Data, int ID_NUM)
{
	Data.Replace(" ", "");

	const unsigned UNIT = 2;
	unsigned POS = 0;

	CString Latitude(""), Longitude(""), Speed_Knots(""), Heading(""), Altitude_WGS("");
	CString Vertical_V("");
	CString Long_Acc(""), Lat_Acc("");

	CString RollAngle(""), PitchAngle(""), RollRate(""), PitchRate(""), YawRate("");
	CString AccX(""), AccY(""), AccZ("");

	int temp = -1, subtemp = -1;
	int upt = -1, lowt = -1;
	short signed stemp = -1;
	unsigned utemp = 0, tempT = 0, tempH = 0, tempM = 0, tempM1 = 0, tempS = 0;
	double lowv(-1);
	std::bitset<8> bitw(0);

	CString MSG = "PlaceHolder_Xbow";

	switch (ID_NUM)
	{
// 	case 0x301:
// 		POS += UNIT;
// 
// 		POS += 3 * UNIT;
// 
// 		Latitude = Data.Mid(POS, 4 * UNIT);
// 		temp = (int)HexTextToUnsigned(Latitude);
// 
// 		m_GPS_Msg_Latest += "LatitudeBEGIN" + Latitude + "LatitudeEND";
// 		InterlockedIncrement(&m_GPS_Send_Num_Count);
// 		break;
	case 0x302:
// 		Longitude = Data.Mid(0, 4 * UNIT);
// 		temp = (int)HexTextToUnsigned(Longitude);
// 		m_GPS_Msg_Latest += "LongitudeBEGIN" + Longitude + "LongitudeEND";
// 		POS += 4 * UNIT;

		Speed_Knots = Data.Mid(POS, 2 * UNIT);
		temp = HexTextToUnsigned(Speed_Knots);
		m_GPS_Msg_Latest.Format("%.3f\n",double(temp)*0.01f*1.852f);
		POS += 2 * UNIT;

// 		Heading = Data.Mid(POS, 2 * UNIT);
// 		temp = HexTextToUnsigned(Heading);
// 		m_GPS_Msg_Latest += "HeadingBEGIN" + Heading + "HeadingEND";
		
		InterlockedIncrement(&m_GPS_Send_Num_Count);
		break;
// 	case 0x303:
// 		Altitude_WGS = Data.Mid(0, 3 * UNIT);
// 		temp = HexTextToUnsigned(Altitude_WGS);
// 		if (Altitude_WGS.GetAt(0) >= '8')
// 			temp -= (0xFFFFFF + 0x1); //24bit signed
// 		m_GPS_Msg_Latest += "AltitudeBEGIN" + Altitude_WGS + "AltitudeEND";
// 		POS += 3 * UNIT;
// 
// 		Vertical_V = Data.Mid(POS, 2 * UNIT);
// 		stemp = (short signed)HexTextToUnsigned(Vertical_V);
// 		m_GPS_Msg_Latest += "VSpeedBEGIN" + Vertical_V + "VSpeedEND";
// 		POS += 2 * UNIT;
// 
// 		POS += 2 * UNIT;
// 		
// 		InterlockedIncrement(&m_GPS_Send_Num_Count);
// 		break;
// 	case 0x304:
// 		POS += 4 * UNIT;
// 
// 		Long_Acc = Data.Mid(POS, 2 * UNIT);
// 		stemp = (short signed)HexTextToUnsigned(Long_Acc);
// 		POS += 2 * UNIT;
// 		m_GPS_Msg_Latest += "LongAccBEGIN" + Long_Acc + "LongAccEND";
// 
// 		Lat_Acc = Data.Mid(POS, 2 * UNIT);
// 		stemp = (short signed)HexTextToUnsigned(Lat_Acc);
// 		m_GPS_Msg_Latest += "LatAccBEGIN" + Lat_Acc + "LatAccEND";
// 
// 		InterlockedIncrement(&m_GPS_Send_Num_Count);
// 		break;
// 	case 0x305:
// 		POS += 4 * UNIT;
// 
// 		POS += 2 * UNIT;
// 		break;
	case 0x306:
		RollAngle = Data.Mid(1 * UNIT, 2 * UNIT);
		PitchAngle = Data.Mid(3 * UNIT, 2 * UNIT);
		RollRate = Data.Mid(5 * UNIT, 2 * UNIT);
		PitchRate = Data.Mid(7 * UNIT, 2 * UNIT);
		YawRate = Data.Mid(9 * UNIT, 2 * UNIT);
		AccX = Data.Mid(11 * UNIT, 2 * UNIT);
		AccY = Data.Mid(13 * UNIT, 2 * UNIT);
		AccZ = Data.Mid(15 * UNIT, 2 * UNIT);

		stemp = (short signed)HexTextToUnsigned(RollAngle);
		lowv = (double)stemp * (180.0f) / pow(2, 15);
		RollAngle.Format("%s: %.6f  ", "X_Angle", lowv);

		stemp = (short signed)HexTextToUnsigned(PitchAngle);
		lowv = (double)stemp * (180.0f) / pow(2, 15);
		PitchAngle.Format("%s: %.6f", "Y_Angle", lowv);

		stemp = (short signed)HexTextToUnsigned(RollRate);
		lowv = (double)stemp * 200.0f * 1.5f / pow(2, 15);
		RollRate.Format("%s: %.6f  ", "X_Rate", lowv);

		stemp = (short signed)HexTextToUnsigned(PitchRate);
		lowv = (double)stemp * 200.0f * 1.5f / pow(2, 15);
		PitchRate.Format("%s: %.6f", "Y_Rate", lowv);

		stemp = (short signed)HexTextToUnsigned(YawRate);
		lowv = (double)stemp * 200.0f * 1.5f / pow(2, 15);
		YawRate.Format("%s: %.6f  ", "Z_Rate", lowv);

		stemp = (short signed)HexTextToUnsigned(AccX);
		lowv = (double)stemp * 2.0f * 1.5f / pow(2, 15);
		AccX.Format("%s: %.6f", "AccX", lowv);

		stemp = (short signed)HexTextToUnsigned(AccY);
		lowv = (double)stemp * 2.0f * 1.5f / pow(2, 15);
		AccY.Format("%s: %.6f  ", "AccY", lowv);

		stemp = (short signed)HexTextToUnsigned(AccZ);
		lowv = (double)stemp * 2.0f * 1.5f / pow(2, 15);
		AccZ.Format("%s: %.6f", "AccZ", lowv);

	break;
	default:
		break;
	}

	if (m_GPS_Send_Num_Count == GPS_SEND_NUM_COUNT)
	{
		CT2CA pszConvertedAnsiString(m_GPS_Msg_Latest);
		{
 			clsCritical locker(m_objpCS);
			m_GPS_Msg_TobeSent = pszConvertedAnsiString;
		}
		m_GPS_Send_Num_Count = 0;
		m_GPS_Msg_Latest = "";
		SetEvent(m_GPS_Net_Event);
	}

	else if (ID_NUM == 0x306)
	{
		CT2CA pszConvertedAnsiString(MSG);
		{
			clsCritical locker(m_objpCS);
			m_Xbow_Msg_TobeSent = pszConvertedAnsiString;
		}
		SetEvent(m_Xbow_Net_Event);
	}
}

void CPCANBasicExampleDlg::DisplayGPSInformation(CString GPS, int iCurrentItem_GPS, int ID_NUM, bool WriteEnable /* = False*/)
{
	lstMessages_GPS.SetItemText(iCurrentItem_GPS, GPS_DATA, GPS);
	GPS.Replace(" ", "");

	const unsigned UNIT = 2;
	unsigned POS = 0;

	CString Sats(""), Time(""), Latitude(""), Longitude(""), Speed_Knots(""), Heading(""), Altitude_WGS("");
	CString VQuality(""), D_GPS(""), Vertical_V("");
	CString Trig_Dist(""), Long_Acc(""), Lat_Acc("");
	CString Trig_Time(""), Trig_V(""), Distance("");
	
	CString RollAngle(""), PitchAngle(""), RollRate(""), PitchRate(""), YawRate("");
	CString AccX(""), AccY(""), AccZ(""), Tempature("");
	CString Badratio("");

	unsigned placeholder(0);
	
	int temp = -1,subtemp = -1;
	int upt = -1, lowt = -1;
	short signed stemp = -1;
	unsigned utemp = 0, tempT = 0, tempH = 0, tempM = 0, tempM1 = 0, tempS = 0;
	double lowv(-1);
	char double2char[33];
	unsigned sizeofdouble2str(sizeof(double2char));
	CString sign("");
	std::bitset<8> bitw(0);

	switch (ID_NUM) 
	{
	case 0x301:
		Sats = GPS.Mid(0, UNIT);
		temp = HexTextToUnsigned(Sats);
		_itoa_s(temp, double2char, sizeofdouble2str, 10);
		m_GPS_Str_Sats = double2char;
		m_GPS_Str_Sats += "\t";
		Sats = "Sats.:  " + IntToStr(temp);
		InterlockedExchange(&utemp, m_GPS_Msg_Sent_Num);
		Sats += "  Sent:  " + IntToStr(utemp);
		POS += UNIT;

		Time = GPS.Mid(POS, 3 * UNIT);
		tempT = HexTextToUnsigned(Time);
		placeholder = tempT;
		_itoa_s(tempT, double2char, sizeofdouble2str, 10);
		m_GPS_Str_Time = double2char;
		m_GPS_Str_Time += "\t";

		utemp = tempT % 100;
		tempT /= 100;
		tempH = tempT / 3600;
		tempM = tempT % 3600;
		tempM1 = tempM / 60;
		tempS = tempM % 60;

		//UTC+10
		tempH += 10;
		tempH = tempH % 24;

		Time = "UTC+10: " + FormatTimeString(tempH, tempM1, tempS, utemp);
		POS += 3 * UNIT;

		Latitude = GPS.Mid(POS, 4 * UNIT);
		temp = (int) HexTextToUnsigned(Latitude);
		_itoa_s(temp, double2char, sizeofdouble2str, 10);
		m_GPS_Str_X = double2char;
		m_GPS_Str_X += "\t";

		subtemp = temp % 100000;
		temp /= 100000;
		upt = temp / 60;
		lowt = temp % 60;
		temp < 0 ? sign = "- " : sign = "+ ";
		Latitude = "X:  " + sign + IntToStr(abs(upt)) + "d " + IntToStr(abs(lowt)) + "." + IntToStr(abs(subtemp)) + "m";
		
		lowv = (double)lowt + (double)subtemp / 100000.f;
		lowv /= 60.f;
		lowv += upt;
		_gcvt_s(double2char, sizeofdouble2str, abs(lowv), 10);
		Latitude += " (" + sign + double2char + ")";

		lstMessages_GPS.SetItemText(iCurrentItem_GPS, GPS_SATS, Sats);
		lstMessages_GPS.SetItemText(iCurrentItem_GPS, GPS_TIME, Time);
		lstMessages_GPS.SetItemText(iCurrentItem_GPS, GPS_LATITUDE, Latitude);

		m_GPS_Str_RAW.at(0) = GPS + "\t" + "301" + "\t";
		++m_GPS_Num_Count;
		break;
	case 0x302:
		Longitude = GPS.Mid(0, 4 * UNIT);
		temp = (int) HexTextToUnsigned(Longitude);
		_itoa_s(temp, double2char, sizeofdouble2str, 10);
		m_GPS_Str_Y = double2char;
		m_GPS_Str_Y += "\t";

		subtemp = temp % 100000;
		temp /= 100000;
		upt = temp / 60;
		lowt = temp % 60;
		temp < 0 ? sign = "- " : sign = "+ ";
		Longitude = "Y: " + sign + IntToStr(abs(upt)) + "d " + IntToStr(abs(lowt)) + "." + IntToStr(abs(subtemp)) + "m";

		lowv = (double)lowt + (double)subtemp / 100000.f;
		lowv /= 60.f;
		lowv += upt;
		_gcvt_s(double2char, sizeofdouble2str, abs(lowv), 10);
		Longitude += " (" + sign + double2char + ")";

		POS += 4 * UNIT;

		Speed_Knots = GPS.Mid(POS, 2 * UNIT);
		temp = HexTextToUnsigned(Speed_Knots);
		_itoa_s(temp, double2char, sizeofdouble2str, 10);
		m_GPS_Str_Velocity = double2char;
		m_GPS_Str_Velocity += "\t";

		upt = temp / 100;
		lowt = temp % 100;
		Speed_Knots = "Speed Knots:  " + IntToStr(upt) + "." + IntToStr(lowt);

		lowv = ((double)temp * 0.01f) * 1.852f; // km/h
		_gcvt_s(double2char, sizeofdouble2str, lowv, 5);
		Speed_Knots += " (" + (CString)double2char + ")";

		POS += 2 * UNIT;

		Heading = GPS.Mid(POS, 2 * UNIT);
		temp = HexTextToUnsigned(Heading);
		_itoa_s(temp, double2char, sizeofdouble2str, 10);
		m_GPS_Str_Heading = double2char;
		m_GPS_Str_Heading += "\t";

		upt = temp / 100;
		lowt = temp % 100;
		Heading = "Heading:  " + IntToStr(upt) + "." + IntToStr(lowt);

		lstMessages_GPS.SetItemText(iCurrentItem_GPS, GPS_LONGITUDE, Longitude);
		lstMessages_GPS.SetItemText(iCurrentItem_GPS, GPS_SPEED_KNOTS, Speed_Knots);
		lstMessages_GPS.SetItemText(iCurrentItem_GPS, GPS_HEADING, Heading);

		m_GPS_Str_RAW.at(1) = GPS + "\t" + "302" + "\t";
		++m_GPS_Num_Count;
		break;
	case 0x303:
		Altitude_WGS = GPS.Mid(0, 3 * UNIT);
		temp = HexTextToUnsigned(Altitude_WGS);
		if (Altitude_WGS.GetAt(0) >= '8')
			temp -= (0xFFFFFF + 0x1); //24bit signed
		_itoa_s(temp, double2char, sizeofdouble2str, 10);
		m_GPS_Str_WGS84 = double2char;
		m_GPS_Str_WGS84 += "\t";

		upt = temp / 100;
		lowt = temp % 100;
		temp < 0 ? sign = "- " : sign = "+ ";
		Altitude_WGS = "WGS.84: " + sign + IntToStr(abs(upt)) + "." + IntToStr(abs(lowt));
		POS += 3 * UNIT;

		Vertical_V = GPS.Mid(POS, 2 * UNIT);
		stemp = (short signed) HexTextToUnsigned(Vertical_V);
		_itoa_s(stemp, double2char, sizeofdouble2str, 10);
		m_GPS_Str_VerticalV = double2char;
		m_GPS_Str_VerticalV += "\t";

		upt = stemp / 100;
		lowt = stemp % 100;
		stemp < 0 ? sign = "- " : sign = "+ ";
		Vertical_V = "Vertical Speed: " + sign + IntToStr(abs(upt)) + "." + IntToStr(abs(lowt));
		POS += 2 * UNIT;

		POS += 2 * UNIT;
		D_GPS = GPS.Mid(POS, UNIT);
		bitw = HexTextToUnsigned(D_GPS);
		D_GPS = UnsignedToBinString(bitw);
		D_GPS.Truncate(D_GPS.GetLength() / 2);
		m_GPS_Str_Status = D_GPS;
		m_GPS_Str_Status += "\t";

		lstMessages_GPS.SetItemText(iCurrentItem_GPS, GPS_ALTITUDE_WGS, Altitude_WGS);
		lstMessages_GPS.SetItemText(iCurrentItem_GPS, GPS_Vertical_V, Vertical_V);
		lstMessages_GPS.SetItemText(iCurrentItem_GPS, GPS_DGPS, D_GPS);

		m_GPS_Str_RAW.at(2) = GPS + "\t" + "303" + "\t";
		++m_GPS_Num_Count;
		break;
	case 0x304:
		Trig_Dist = GPS.Mid(0, 4 * UNIT);
		tempT = HexTextToUnsigned(Trig_Dist);
		_itoa_s(tempT, double2char, sizeofdouble2str, 10);
		m_GPS_Str_TrigDist = double2char;
		m_GPS_Str_TrigDist += "\t";

		lowv = (double)tempT * 0.000078125f;
		_gcvt_s(double2char, sizeofdouble2str, lowv, 9);
		Trig_Dist = "Trig_Dist: " + (CString)double2char;

		POS += 4 * UNIT;

		Long_Acc = GPS.Mid(POS, 2 * UNIT);
		stemp = (short signed)HexTextToUnsigned(Long_Acc);
		_itoa_s(stemp, double2char, sizeofdouble2str, 10);
		m_GPS_Str_LongAcc = double2char;
		m_GPS_Str_LongAcc += "\t";

		upt = stemp / 100;
		lowt = stemp % 100;
		stemp < 0 ? sign = "- " : sign = "+ ";
		Long_Acc = "Long_Acc: " + sign + IntToStr(abs(upt)) + "." + IntToStr(abs(lowt));

		POS += 2 * UNIT;

		Lat_Acc = GPS.Mid(POS, 2 * UNIT);
		stemp = (short signed)HexTextToUnsigned(Lat_Acc);
		_itoa_s(stemp, double2char, sizeofdouble2str, 10);
		m_GPS_Str_LatACC = double2char;
		m_GPS_Str_LatACC += "\t";

		stemp < 0 ? sign = "- " : sign = "+ ";
		upt = stemp / 100;
		lowt = stemp % 100;
		Lat_Acc = "Lat_ACC: " + sign + IntToStr(abs(upt)) + "." + IntToStr(abs(lowt));

		lstMessages_GPS.SetItemText(iCurrentItem_GPS, 1, Trig_Dist);
		lstMessages_GPS.SetItemText(iCurrentItem_GPS, 2, Long_Acc);
		lstMessages_GPS.SetItemText(iCurrentItem_GPS, 3, Lat_Acc);

		m_GPS_Str_RAW.at(3) = GPS + "\t" + "304" + "\t";
		++m_GPS_Num_Count;
		break;
	case 0x305:
		Distance = GPS.Mid(POS, 4 * UNIT);
		tempT = HexTextToUnsigned(Distance);
		_itoa_s(tempT, double2char, sizeofdouble2str, 10);
		m_GPS_Str_Distance = double2char;
		m_GPS_Str_Distance += "\t";

		lowv = (double)tempT * 0.000078125f;
		m_GPS_Distance_Offset = (lowv < m_GPS_Distance_Offset ? m_GPS_First_Distance + max(lowv, 0.000078125f) : m_GPS_Distance_Offset);
		lowv += m_GPS_Distance_Offset;

		m_GPS_First_Distance = (m_GPS_First_Distance < 0 ? lowv : m_GPS_First_Distance);
		_gcvt_s(double2char, sizeofdouble2str, lowv - m_GPS_First_Distance, 9);
		
		Distance = "Distance: " + (CString)double2char;
		Distance += "\t";
		m_GPS_First_Distance = lowv;

		POS += 4 * UNIT;

		Trig_Time = GPS.Mid(POS, 2 * UNIT);
		temp = HexTextToUnsigned(Trig_Time);
		_itoa_s(temp, double2char, sizeofdouble2str, 10);
		m_GPS_Str_TrigTime = double2char;
		m_GPS_Str_TrigTime += "\t";

		upt = temp / 100;
		lowt = temp % 100;
		Trig_Time = "Trig_Time: " + IntToStr(upt) + "." + IntToStr(lowt);

		POS += 2 * UNIT;

		Trig_V = GPS.Mid(POS, 2 * UNIT);
		temp = HexTextToUnsigned(Trig_V);
		_itoa_s(temp, double2char, sizeofdouble2str, 10);
		m_GPS_Str_TrigV = double2char;
		m_GPS_Str_TrigV += "\t";

		upt = temp / 100;
		lowt = temp % 100;
		Trig_V = "Trig_Speed: " + IntToStr(upt) + "." + IntToStr(lowt);

		lowv = (double)temp * 1.852f;
		_gcvt_s(double2char, sizeofdouble2str, lowv, 5);
		Trig_V += " (" + (CString)double2char + ")";

		lstMessages_GPS.SetItemText(iCurrentItem_GPS, 1, Distance);
		lstMessages_GPS.SetItemText(iCurrentItem_GPS, 2, Trig_Time);
		lstMessages_GPS.SetItemText(iCurrentItem_GPS, 3, Trig_V);

		m_GPS_Str_RAW.at(4) = GPS + "\t" + "305" + "\t";
		++m_GPS_Num_Count;
		break;
	case 0x306:
		// 		EnterCriticalSection(&m_xBow_CriticalSection);
		// 		utemp = m_Xbow_Count;
		// 		tempT = m_Xbow_Effictive_Count;
		// 		LeaveCriticalSection(&m_xBow_CriticalSection);

		{
			clsCritical locker(m_objpCS);
			utemp = m_Xbow_Count;
			tempT = m_Xbow_Effictive_Count;
		}

		if (utemp == 0)
			lowv = 0.0f;
		else
			lowv = (double)(utemp - tempT) / (double)utemp;
		_gcvt_s(double2char, sizeofdouble2str, lowv, 6);
		m_Xbow_Badratio = double2char;
		m_Xbow_Badratio += "\t";
		Badratio.Format("%s: %d (bad: %.3f)", "Xbow: ", utemp,lowv);

		RollAngle = GPS.Mid(1*UNIT, 2*UNIT);
		PitchAngle = GPS.Mid(3*UNIT, 2*UNIT);
		RollRate = GPS.Mid(5 * UNIT, 2 * UNIT);
		PitchRate = GPS.Mid(7 * UNIT, 2 * UNIT);
		YawRate = GPS.Mid(9 * UNIT, 2 * UNIT);
		AccX = GPS.Mid(11 * UNIT, 2 * UNIT);
		AccY = GPS.Mid(13 * UNIT, 2 * UNIT);
		AccZ = GPS.Mid(15 * UNIT, 2 * UNIT);
		Tempature = GPS.Mid(17 * UNIT, 2 * UNIT);
		Time = GPS.Mid(19 * UNIT, 2 * UNIT);

		stemp = (short signed)HexTextToUnsigned(RollAngle);
		lowv = (double)stemp * (180.0f) / pow(2, 15);
		_itoa_s(stemp, double2char, sizeofdouble2str, 10);
		m_Xbow_RollAngle = double2char;
		m_Xbow_RollAngle += "\t";
		RollAngle.Format("%s: %.3f  ","X_Angle",lowv);
		
		stemp = (short signed)HexTextToUnsigned(PitchAngle);
		lowv = (double)stemp * (180.0f) / pow(2, 15);
		_itoa_s(stemp, double2char, sizeofdouble2str, 10);
		m_Xbow_PitchAngle = double2char;
		m_Xbow_PitchAngle += "\t";
		PitchAngle.Format("%s: %.3f", "Y_Angle", lowv);

		RollAngle += PitchAngle;

		stemp = (short signed)HexTextToUnsigned(RollRate);
		lowv = (double)stemp * 200.0f * 1.5f / pow(2, 15);
		_itoa_s(stemp, double2char, sizeofdouble2str, 10);
		m_Xbow_RollRate = double2char;
		m_Xbow_RollRate += "\t";
		RollRate.Format("%s: %.3f  ", "X_Rate", lowv);

		stemp = (short signed)HexTextToUnsigned(PitchRate);
		lowv = (double)stemp * 200.0f * 1.5f / pow(2, 15);
		_itoa_s(stemp, double2char, sizeofdouble2str, 10);
		m_Xbow_PitchRate = double2char;
		m_Xbow_PitchRate += "\t";
		PitchRate.Format("%s: %.3f", "Y_Rate", lowv);

		RollRate += PitchRate;

		stemp = (short signed)HexTextToUnsigned(YawRate);
		lowv = (double)stemp * 200.0f * 1.5f / pow(2, 15);
		_itoa_s(stemp, double2char, sizeofdouble2str, 10);
		m_Xbow_YawRate = double2char;
		m_Xbow_YawRate += "\t";
		YawRate.Format("%s: %.3f  ", "Z_Rate", lowv);

		stemp = (short signed)HexTextToUnsigned(AccX);
		lowv = (double)stemp * 4.0f * 1.5f / pow(2, 15);
		_itoa_s(stemp, double2char, sizeofdouble2str, 10);
		m_Xbow_AccX = double2char;
		m_Xbow_AccX += "\t";
		AccX.Format("%s: %.3f", "AccX", lowv);

		YawRate += AccX;

		stemp = (short signed)HexTextToUnsigned(AccY);
		lowv = (double)stemp * 4.0f * 1.5f / pow(2, 15);
		_itoa_s(stemp, double2char, sizeofdouble2str, 10);
		m_Xbow_AccY = double2char;
		m_Xbow_AccY += "\t";
		AccY.Format("%s: %.3f  ", "AccY", lowv);

		stemp = (short signed)HexTextToUnsigned(AccZ);
		lowv = (double)stemp * 4.0f * 1.5f / pow(2, 15);
		_itoa_s(stemp, double2char, sizeofdouble2str, 10);
		m_Xbow_AccZ = double2char;
		m_Xbow_AccZ += "\t";
		AccZ.Format("%s: %.3f", "AccZ", lowv);

		AccY += AccZ;

		utemp = HexTextToUnsigned(Tempature);
		lowv = (((double)utemp * 5.0f / 4096.0f) - 1.375f) * 44.44f;
		_itoa_s(utemp, double2char, sizeofdouble2str, 10);
		m_Xbow_Tem = double2char;
		m_Xbow_Tem += "\t";
		Tempature.Format("%s: %.3f", "Tempature", lowv);

		utemp = HexTextToUnsigned(Time);
		_itoa_s(utemp, double2char, sizeofdouble2str, 10);
		m_Xbow_Time = double2char;
		m_Xbow_Time += "\t";
		InterlockedExchange(&tempS, m_Xbow_Msg_Sent_Num);
		Time.Format("%s: %d  Sent: %d", "Time", utemp, tempS);

		if (!m_Xbow_Added2Item)
		{
			ADDLVItem_GPS(GPS);
			m_Xbow_Added2Item = true;
		}

		lstMessages_GPS.SetItemText(lstMessages_GPS.GetItemCount() - 2, 0, GPS.Mid(0 * UNIT, 11 * UNIT));
		lstMessages_GPS.SetItemText(lstMessages_GPS.GetItemCount() - 2, 1, RollAngle);
		lstMessages_GPS.SetItemText(lstMessages_GPS.GetItemCount() - 2, 2, YawRate);
		lstMessages_GPS.SetItemText(lstMessages_GPS.GetItemCount() - 2, 3, Badratio);

		lstMessages_GPS.SetItemText(lstMessages_GPS.GetItemCount() - 1, 0, GPS.Mid(11 * UNIT + 1, 11 * UNIT));
		lstMessages_GPS.SetItemText(lstMessages_GPS.GetItemCount() - 1, 1, RollRate);
		lstMessages_GPS.SetItemText(lstMessages_GPS.GetItemCount() - 1, 2, AccY);
		lstMessages_GPS.SetItemText(lstMessages_GPS.GetItemCount() - 1, 3, Time);

		break;
	default:
		break;
	}

	if (WriteEnable)
	{
		if (m_GPS_Num_Count >= GPS_NUM_COUNT)
		{
			m_GPS_Num_Count = 0;

			m_GPS_Recorder += m_GPS_Str_Time + m_GPS_Str_X + m_GPS_Str_Y + m_GPS_Str_Velocity + m_GPS_Str_Heading;
			m_GPS_Recorder += m_GPS_Str_WGS84 + m_GPS_Str_Velocity + m_GPS_Str_TrigDist + m_GPS_Str_LongAcc;
			m_GPS_Recorder += m_GPS_Str_LatACC + m_GPS_Str_Status + m_GPS_Str_TrigTime + m_GPS_Str_TrigV;
			m_GPS_Recorder += m_GPS_Str_Distance + m_GPS_Str_Sats;

			std::vector<std::string>::const_iterator i = m_GPS_Str_RAW.cbegin();
			std::vector<std::string>::const_iterator END = m_GPS_Str_RAW.cend();
			while (i != END)
			{
				m_GPS_Recorder += *i;
				++i;
			}
			m_GPS_Str_RAW.assign(GPS_DATA_COUNT, "");

			if (placeholder)
			{
				unsigned isfirst(0);
				InterlockedExchange(&isfirst, m_GPS_Is_First);
				if (isfirst)
				{
					m_GPS_Begin_Time = placeholder;
					InterlockedExchange(&m_GPS_Is_First, 0);
				}
				else
				{
					if (m_GPS_Begin_Time > placeholder)
						m_GPS_Recorder += "WARNING";
				}
			}

// 			m_GPS_Recorder += "\n";

			return;
		}

		if (GPS.GetLength() == 22 * 2)
		{
			m_Xbow_Recorder += m_Xbow_RollAngle + m_Xbow_PitchAngle + m_Xbow_RollRate + m_Xbow_PitchRate;
			m_Xbow_Recorder += m_Xbow_YawRate + m_Xbow_AccX + m_Xbow_AccY + m_Xbow_AccZ;
			m_Xbow_Recorder += m_Xbow_Tem + m_Xbow_Time;
			m_Xbow_Recorder += m_Xbow_Badratio;
			m_Xbow_Recorder += GPS;

// 			m_Xbow_Recorder += "\n";

			return;
		}
	}
}

CString CPCANBasicExampleDlg::FormatTimeString(unsigned hour, unsigned minute, unsigned seconds, unsigned remainder)
{
	std::stringstream ss;
	ss << std::setfill('0') << std::setw(2) << hour << ":" << std::setfill('0') << std::setw(2) << minute
		<< ":" << std::setfill('0') << std::setw(2) << seconds << "." << std::setfill('0') << std::setw(2) << remainder;
	
	std::string temp;
	ss >> temp;

	CString formatted(temp.c_str());

	return formatted;
}

unsigned CPCANBasicExampleDlg::HexTextToUnsigned(CString ToConvert)
{
	unsigned int x;
	std::stringstream ss;
	ss << std::hex << ToConvert;
	ss >> x;

	return x;
}

CString CPCANBasicExampleDlg::UnsignedToBinString(const std::bitset<8> &bitw)
{
	CString binw("");

	size_t counts = bitw.size();
	unsigned num = 0;
	for (int i = counts - 1; i >= 0 ; i--)
	{
		bitw.test(i) == false ? binw += '0' : binw += '1';
		++num;
		if (num % 4 == 0)
		{
			binw += " ";
		}
	}

	return binw;
}

void CPCANBasicExampleDlg::WriteAccFile()
{
	std::string dir;
	dir = ".\\";
	CT2CA subs(txtFilterSubs);
	CT2CA trials(txtFilterTrials);
	dir += subs;

	boost::filesystem::path p(dir.c_str());
	boost::system::error_code ec;
	bool success = boost::filesystem::create_directory(p, ec);
	if (!success && !boost::filesystem::is_directory(p))
	{
		CString Warn;
		Warn.Format("%s\n", ec.message().c_str());
		::MessageBox(NULL, Warn, "Error!", MB_ICONERROR);

		dir = subs + "_";
		dir += trials + "_";
	}
	else
	{
		dir += "\\";
		dir += trials;
		p = dir.c_str();
		success = boost::filesystem::create_directory(p, ec);
		if (!success && !boost::filesystem::is_directory(p))
		{
			CString Warn;
			Warn.Format("%s\n", ec.message().c_str());
			::MessageBox(NULL, Warn, "Error!", MB_ICONERROR);

			dir = subs + "_";
			dir += trials + "_";
		}
		else
		{
			dir += "\\";
		}

	}

	dir += "Acc.txt";

	std::ofstream myfile;
	myfile.open(dir, std::ofstream::out);
	myfile << m_Xbow_Recorder;
	myfile.close();
}

void CPCANBasicExampleDlg::WriteGPSFile()
{
	std::string dir;
	dir = ".\\";
	CT2CA subs(txtFilterSubs);
	CT2CA trials(txtFilterTrials);
	dir += subs;

	boost::filesystem::path p(dir.c_str());
	boost::system::error_code ec;
	bool success = boost::filesystem::create_directory(p, ec);
	if (!success && !boost::filesystem::is_directory(p))
	{
		CString Warn;
		Warn.Format("%s\n", ec.message().c_str());
		::MessageBox(NULL, Warn, "Error!", MB_ICONERROR);

		dir = subs + "_";
		dir += trials + "_";
	}
	else
	{
		dir += "\\";
		dir += trials;
		p = dir.c_str();
		success = boost::filesystem::create_directory(p, ec);
		if (!success && !boost::filesystem::is_directory(p))
		{
			CString Warn;
			Warn.Format("%s\n", ec.message().c_str());
			::MessageBox(NULL, Warn, "Error!", MB_ICONERROR);

			dir = subs + "_";
			dir += trials + "_";
		}
		else
		{
			dir += "\\";
		}
	}

	dir += "GPS.txt";

	std::ofstream myfile;
	myfile.open(dir, std::ofstream::out);
	myfile << m_GPS_Recorder;
	myfile.close();
}

void CPCANBasicExampleDlg::WriteShutterGlass()
{
	std::vector<std::pair<__int64, std::string>>::const_iterator i = m_Shutter_Time_Rec.cbegin();
	
	char temp[100];
	unsigned sizetemp = sizeof(temp);
	std::string rec;
	while (i != m_Shutter_Time_Rec.cend())
	{
		_i64toa_s((*i).first, temp, sizetemp, 10);
		rec += temp;
		rec += "\t";
		rec += (*i).second;
		rec += "\n";

		++i;
	}

	std::string dir;
	dir = ".\\";
	CT2CA subs(txtFilterSubs);
	CT2CA trials(txtFilterTrials);
	dir += subs;

	boost::filesystem::path p(dir.c_str());
	boost::system::error_code ec;
	bool success = boost::filesystem::create_directory(p, ec);
	if (!success && !boost::filesystem::is_directory(p))
	{
		CString Warn;
		Warn.Format("%s\n", ec.message().c_str());
		::MessageBox(NULL, Warn, "Error!", MB_ICONERROR);

		dir = subs + "_";
		dir += trials + "_";
	}
	else
	{
		
		dir += "\\";
		dir += trials;
		p = dir.c_str();
		success = boost::filesystem::create_directory(p, ec);
		if (!success && !boost::filesystem::is_directory(p))
		{
			CString Warn;
			Warn.Format("%s\n", ec.message().c_str());
			::MessageBox(NULL, Warn, "Error!", MB_ICONERROR);

			dir = subs + "_";
			dir += trials + "_";
		}
		else
		{
			dir += "\\";
		}
	}
	
	dir += "Shutter.txt";

	std::ofstream myfile;
	myfile.open(dir, std::ofstream::out);
	myfile << rec;
	myfile.close();
}

// void CPCANBasicExampleDlg::ComUninitialize()
// {
// 	m_Xbow_Recorder.clear();
// 	m_Xbow_Msg_List.resize(0);
// 	m_Xbow_Msg_List.clear();
// 
// 	btnRefreshCom.EnableWindow(TRUE);
// }

void CPCANBasicExampleDlg::ProcessMessage(TPCANMsgFD theMsg, TPCANTimestampFD itsTimeStamp)
{
	POSITION pos;
	MessageStatus *msg;

    // We search if a message (Same ID and Type) is 
    // already received or if this is a new message
	// (in a protected environment)
	//
	{
		clsCritical locker(m_objpCS);

		pos = m_LastMsgsList->GetHeadPosition();
		for(int i=0; i < m_LastMsgsList->GetCount(); i++)
		{
			msg = (MessageStatus*)m_LastMsgsList->GetNext(pos);
			if((msg->CANMsg.ID == theMsg.ID) && (msg->CANMsg.MSGTYPE == theMsg.MSGTYPE))
			{
				// Modify the message and exit
				//
				CString ID = msg->IdString;
				CString DATA = msg->DataString;

				msg->Update(theMsg, itsTimeStamp);
				m_GPS_Msg_List.push_back(GPS_MK_PAIR(ID, DATA));
				boost::posix_time::ptime now = boost::posix_time::microsec_clock::local_time();
				boost::posix_time::time_duration diff = now - time_t_epoch;
				m_GPS_CPU_Time.push_back(diff.total_milliseconds());

				ID.Truncate(ID.GetLength() - 1);
				int ID_NUM = HexTextToUnsigned(ID);
				if (ID_NUM >= 0x301 && ID_NUM <= 0x305)
				{
					GetGPSXbowInformation(DATA, ID_NUM);
				}
				
				return;
			}
		}
		// Message not found. It will created
		//
		InsertMsgEntry(theMsg, itsTimeStamp);
	}
}

void CPCANBasicExampleDlg::ProcessMessage(TPCANMsg theMsg, TPCANTimestamp itsTimeStamp)
{		
    TPCANMsgFD newMsg;
    TPCANTimestampFD newTimestamp;

	newMsg = TPCANMsgFD();
	newMsg.ID = theMsg.ID;
	newMsg.DLC = theMsg.LEN;
    for (int i = 0; i < ((theMsg.LEN > 8) ? 8 : theMsg.LEN); i++)
        newMsg.DATA[i] = theMsg.DATA[i];
    newMsg.MSGTYPE = theMsg.MSGTYPE;

	newTimestamp = (itsTimeStamp.micros + 1000 * itsTimeStamp.millis + 0x100000000 * 1000 * itsTimeStamp.millis_overflow);
	
	ProcessMessage(newMsg, newTimestamp);
}

TPCANStatus CPCANBasicExampleDlg::ReadMessageFD()
{
    TPCANMsgFD CANMsg;
    TPCANTimestampFD CANTimeStamp;
    TPCANStatus stsResult;

    // We execute the "Read" function of the PCANBasic                
    //
    stsResult = m_objPCANBasic->ReadFD(m_PcanHandle, &CANMsg, &CANTimeStamp);
    if (stsResult == PCAN_ERROR_OK)
        // We process the received message
        //
        ProcessMessage(CANMsg, CANTimeStamp);

    return stsResult;
}

TPCANStatus CPCANBasicExampleDlg::ReadMessage()
{
    TPCANMsg CANMsg;
    TPCANTimestamp CANTimeStamp;
    TPCANStatus stsResult;

    // We execute the "Read" function of the PCANBasic                
    //
    stsResult = m_objPCANBasic->Read(m_PcanHandle, &CANMsg, &CANTimeStamp);
    if (stsResult == PCAN_ERROR_OK)
        // We process the received message
        //
        ProcessMessage(CANMsg, CANTimeStamp);

    return stsResult;
}

void CPCANBasicExampleDlg::ReadMessages()
{
	TPCANStatus stsResult;

	// We read at least one time the queue looking for messages.
	// If a message is found, we look again trying to find more.
	// If the queue is empty or an error occurr, we get out from
	// the dowhile statement.
	//			
	do
	{
		stsResult =  m_IsFD ? ReadMessageFD() : ReadMessage();
        if (stsResult == PCAN_ERROR_ILLOPERATION)
            break;
	} while (btnRelease.IsWindowEnabled() && (!(stsResult & PCAN_ERROR_QRCVEMPTY)));
}

DWORD WINAPI CPCANBasicExampleDlg::CallCANReadThreadFunc(LPVOID lpParam) 
{
	// Cast lpParam argument to PCANBasicExampleDlg*
	//
	CPCANBasicExampleDlg* dialog = (CPCANBasicExampleDlg*)lpParam;
	
	// Call PCANBasicExampleDlg Thread member function
	//
	return dialog->CANReadThreadFunc(NULL);
}

DWORD WINAPI CPCANBasicExampleDlg::CANReadThreadFunc(LPVOID lpParam) 
{
	TPCANStatus stsResult;
	DWORD result, dwTemp;

	m_Terminated = false;

	// Sets the handle of the Receive-Event.
	//
	stsResult = m_objPCANBasic->SetValue(m_PcanHandle, PCAN_RECEIVE_EVENT ,&m_hEvent, sizeof(m_hEvent));

	// If it fails, a error message is shown
	//
	if (stsResult != PCAN_ERROR_OK)
	{
		::MessageBox(NULL, GetFormatedError(stsResult), "Error!",MB_ICONERROR);
		m_Terminated = true;
		return 1;
	}

	// While this mode is selected
	//
	while(!m_Terminated)
	{
		//Wait for CAN Data...
		result = WaitForSingleObject(m_hEvent, 1);

		if (result == WAIT_OBJECT_0)
			ReadMessages();
	}

	// Resets the Event-handle configuration
	//
	dwTemp = 0;
	m_objPCANBasic->SetValue(m_PcanHandle, PCAN_RECEIVE_EVENT ,&dwTemp, sizeof(dwTemp));
	
	return 0;
}

void CPCANBasicExampleDlg::ReadingModeChanged()
{
	if (!btnRelease.IsWindowEnabled())
		return;

	// If active reading mode is By Timer
	//
	if(m_ActiveReadingMode == 0)
	{
		// Terminate Read Thread if it exists
		//
		if(m_hThread != NULL)
		{
			m_Terminated = true;
			WaitForSingleObject(m_hThread,-1);
			m_hThread = NULL;
		}
		// We start to read
		//
		SetTimerRead(true);
	}
	// If active reading mode is By Event
	//
	else if(m_ActiveReadingMode == 1)
	{
		// We stop to read from the CAN queue
		//
		SetTimerRead(false);

		// Create Reading Thread ....
		//
		m_hThread = CreateThread(NULL, NULL, CPCANBasicExampleDlg::CallCANReadThreadFunc, (LPVOID)this, NULL, NULL);

		if(m_hThread == NULL)
			::MessageBox(NULL,"Create CANRead-Thread failed","Error!",MB_ICONERROR);
	}
	else
	{
		// Terminate Read Thread if it exists
		//
		if(m_hThread != NULL)
		{
			m_Terminated = true;
			WaitForSingleObject(m_hThread,-1);
			m_hThread = NULL;
		}
		// We start to read
		//
		SetTimerRead(false);
		btnRead.EnableWindow(btnRelease.IsWindowEnabled() && rdbReadingManual.GetCheck());
	}
}
void CPCANBasicExampleDlg::FillComboBoxData()
{
	// Channels will be check
	//
	OnBnClickedButtonHwrefresh();

    // FD Bitrate: 
    //      Arbitration: 1 Mbit/sec 
    //      Data: 2 Mbit/sec
    //
    txtBitrate = "f_clock_mhz=20, nom_brp=5, nom_tseg1=2, nom_tseg2=1, nom_sjw=1, data_brp=2, data_tseg1=3, data_tseg2=1, data_sjw=1";
	
	// TPCANBaudrate 
	//
	cbbBaudrates.SetCurSel(1); // 500 K
	OnCbnSelchangeCbbbaudrates();

	// Hardware Type for no plugAndplay hardware
	//
	cbbHwsType.SetCurSel(0);
	OnCbnSelchangeCbbhwstype();

	// Interrupt for no plugAndplay hardware
	//
	cbbInterrupt.SetCurSel(0);

	// IO Port for no plugAndplay hardware
	//
	cbbIO.SetCurSel(0);

	// Parameters for GetValue and SetValue function calls
	//
	cbbParameter.SetCurSel(0);	//default:Listen-Only Mode
	OnCbnSelchangeComboparameter();
}

CString CPCANBasicExampleDlg::FormatChannelName(TPCANHandle handle, bool isFD)
{
	CString result;
	BYTE byChannel;

	// Gets the owner device and channel for a 
	// PCAN-Basic handle
	//
	if(handle < 0x100)
		byChannel = (BYTE)(handle) & 0xF;
	else
		byChannel = (BYTE)(handle) & 0xFF;

	// Constructs the PCAN-Basic Channel name and return it
	//
	result.Format(isFD ? "%s:FD %d (%Xh)" : "%s %d (%Xh)", GetTPCANHandleName(handle), byChannel, handle);
	return result;
}

CString CPCANBasicExampleDlg::FormatChannelName(TPCANHandle handle)
{
	return FormatChannelName(handle, false);
}

CString CPCANBasicExampleDlg::GetTPCANHandleName(TPCANHandle handle)
{
	CString result = "PCAN_NONE";
	switch(handle)
	{
	case PCAN_ISABUS1:
	case PCAN_ISABUS2:
	case PCAN_ISABUS3:
	case PCAN_ISABUS4:
	case PCAN_ISABUS5:
	case PCAN_ISABUS6:
	case PCAN_ISABUS7:
	case PCAN_ISABUS8:
		result = "PCAN_ISA";
		break;

	case PCAN_DNGBUS1:
		result = "PCAN_DNG";
		break;

	case PCAN_PCIBUS1:
	case PCAN_PCIBUS2:
	case PCAN_PCIBUS3:
	case PCAN_PCIBUS4:
	case PCAN_PCIBUS5:
	case PCAN_PCIBUS6:
	case PCAN_PCIBUS7:
	case PCAN_PCIBUS8:
	case PCAN_PCIBUS9:
	case PCAN_PCIBUS10:
	case PCAN_PCIBUS11:
	case PCAN_PCIBUS12:
	case PCAN_PCIBUS13:
	case PCAN_PCIBUS14:
	case PCAN_PCIBUS15:
	case PCAN_PCIBUS16:
		result = "PCAN_PCI";
		break;

	case PCAN_USBBUS1:
	case PCAN_USBBUS2:
	case PCAN_USBBUS3:
	case PCAN_USBBUS4:
	case PCAN_USBBUS5:
	case PCAN_USBBUS6:
	case PCAN_USBBUS7:
	case PCAN_USBBUS8:
	case PCAN_USBBUS9:
	case PCAN_USBBUS10:
	case PCAN_USBBUS11:
	case PCAN_USBBUS12:
	case PCAN_USBBUS13:
	case PCAN_USBBUS14:
	case PCAN_USBBUS15:
	case PCAN_USBBUS16:
		result = "PCAN_USB";
		break;

	case PCAN_PCCBUS1:
	case PCAN_PCCBUS2:
		result = "PCAN_PCC";
		break;

	case PCAN_LANBUS1:
	case PCAN_LANBUS2:
	case PCAN_LANBUS3:
	case PCAN_LANBUS4:
	case PCAN_LANBUS5:
	case PCAN_LANBUS6:
	case PCAN_LANBUS7:
	case PCAN_LANBUS8:
	case PCAN_LANBUS9:
	case PCAN_LANBUS10:
	case PCAN_LANBUS11:
	case PCAN_LANBUS12:
	case PCAN_LANBUS13:
	case PCAN_LANBUS14:
	case PCAN_LANBUS15:
	case PCAN_LANBUS16:
		result = "PCAN_LAN";
		break;
	}
	return result;
}


CString CPCANBasicExampleDlg::GetComboBoxSelectedLabel(CComboBox* ccb)
{
	CString strTemp;
	int item = ccb->GetCurSel();
	if(item != CB_ERR)
		ccb->GetLBText(item, strTemp);

	return strTemp;
}


CString CPCANBasicExampleDlg::GetFormatedError(TPCANStatus error)
{
	TPCANStatus status;
	char buffer[256];	
	CString result;

	memset(buffer,'\0',255);
	// Gets the text using the GetErrorText API function
	// If the function success, the translated error is returned. If it fails,
	// a text describing the current error is returned.
	//
	status = m_objPCANBasic->GetErrorText(error, 0, buffer);
	if(status != PCAN_ERROR_OK)
		result.Format("An error ocurred. Error-code's text (%Xh) couldn't be retrieved", error);
	else
		result = buffer;
	return result;
}

void CPCANBasicExampleDlg::SetConnectionStatus(bool bConnected)
{
	// Buttons
	//
	btnInit.EnableWindow(!bConnected);
	btnRead.EnableWindow(bConnected && rdbReadingManual.GetCheck());	
	btnRelease.EnableWindow(bConnected);
	btnFilterApply.EnableWindow(bConnected);
	btnFilterQuery.EnableWindow(bConnected);
	btnVersions.EnableWindow(bConnected);	
	btnRefresh.EnableWindow(!bConnected);
	btnStatus.EnableWindow(bConnected);
	btnReset.EnableWindow(bConnected);

	// ComboBoxs
	//
	cbbChannel.EnableWindow(!bConnected);
	cbbBaudrates.EnableWindow(!bConnected);	
	cbbHwsType.EnableWindow(!bConnected);
	cbbIO.EnableWindow(!bConnected);
	cbbInterrupt.EnableWindow(!bConnected);
    
	// Check-Buttons
    //
    chbCanFD.EnableWindow(!bConnected);

	// Hardware configuration and read mode
	//
	if (!bConnected)
		OnCbnSelchangecbbChannel();
	else
		ReadingModeChanged();

	// Display messages in grid
    //
	SetTimerDisplay(bConnected);
}

bool CPCANBasicExampleDlg::GetFilterStatus(int* status)
{
	TPCANStatus stsResult;

	// Tries to get the stataus of the filter for the current connected hardware
	//
	stsResult = m_objPCANBasic->GetValue(m_PcanHandle, PCAN_MESSAGE_FILTER, (void*)status, sizeof(int));

	// If it fails, a error message is shown
	//
	if (stsResult != PCAN_ERROR_OK)
	{
		::MessageBox(NULL, GetFormatedError(stsResult), "Error!",MB_ICONERROR);
		return false;
	}
	return true;
}

void CPCANBasicExampleDlg::IncludeTextMessage(CString strMsg)
{
	listBoxInfo.AddString(strMsg);
	listBoxInfo.SetCurSel(listBoxInfo.GetCount() - 1);
}

void CPCANBasicExampleDlg::ConfigureLogFile()
{
	int iBuffer;

	// Sets the mask to catch all events
	//
	iBuffer = LOG_FUNCTION_ALL;

	// Configures the log file. 
	// NOTE: The Log capability is to be used with the NONEBUS Handle. Other handle than this will 
	// cause the function fail.
	//
	m_objPCANBasic->SetValue(PCAN_NONEBUS, PCAN_LOG_CONFIGURE, (void*)&iBuffer, sizeof(iBuffer));
}

void CPCANBasicExampleDlg::ConfigureTraceFile()
{
	int iBuffer;
	TPCANStatus stsResult;

    // Configure the maximum size of a trace file to 5 megabytes
    //
	iBuffer = 50;
	stsResult = m_objPCANBasic->SetValue(m_PcanHandle, PCAN_TRACE_SIZE, (void*)&iBuffer, sizeof(iBuffer));
	if (stsResult != PCAN_ERROR_OK)
		IncludeTextMessage(GetFormatedError(stsResult));

    // Configure the way how trace files are created: 
    // * Standard name is used
    // * Existing file is ovewritten, 
    // * Only one file is created.
    // * Recording stopts when the file size reaches 5 megabytes.
    //
	iBuffer = TRACE_FILE_SEGMENTED | TRACE_FILE_DATE | TRACE_FILE_TIME;
	stsResult = m_objPCANBasic->SetValue(m_PcanHandle, PCAN_TRACE_CONFIGURE, (void*)&iBuffer, sizeof(iBuffer));
	if (stsResult != PCAN_ERROR_OK)
		IncludeTextMessage(GetFormatedError(stsResult));
}

void CPCANBasicExampleDlg::OnBnClickedChbfcanfd()
{
	m_IsFD = chbCanFD.GetCheck() > 0;

	cbbBaudrates.ShowWindow(!m_IsFD ? SW_SHOW : SW_HIDE);
	cbbHwsType.ShowWindow(!m_IsFD ? SW_SHOW : SW_HIDE);
	cbbIO.ShowWindow(!m_IsFD ? SW_SHOW : SW_HIDE);
	cbbInterrupt.ShowWindow(!m_IsFD ? SW_SHOW : SW_HIDE);
	GetDlgItem(IDC_LABAUDRATE)->ShowWindow(!m_IsFD ? SW_SHOW : SW_HIDE);
	GetDlgItem(IDC_LAHWTYPE)->ShowWindow(!m_IsFD ? SW_SHOW : SW_HIDE);
	GetDlgItem(IDC_LAIOPORT)->ShowWindow(!m_IsFD ? SW_SHOW : SW_HIDE);
	GetDlgItem(IDC_LAINTERRUPT)->ShowWindow(!m_IsFD ? SW_SHOW : SW_HIDE);

	GetDlgItem(IDC_TXTBITRATE)->ShowWindow(m_IsFD ? SW_SHOW : SW_HIDE);
	GetDlgItem(IDC_LABITRATE)->ShowWindow(m_IsFD ? SW_SHOW : SW_HIDE);
}



void CPCANBasicExampleDlg::OnLbnDblclkListinfo()
{
	OnBnClickedButtoninfoclear();
}

void CPCANBasicExampleDlg::OnBnClickedButtonHwrefresh2()
{
	// TODO: Add your control notification handler code here

	ccbHwXbow.ResetContent();

	EnumComPorts();
}


void CPCANBasicExampleDlg::OnBnClickedButtonreset2()
{
	// TODO: Add your control notification handler code here

	InterlockedExchange(&m_Xbow_Algin, 1);
}


void CPCANBasicExampleDlg::OnBnClickedButtonfilterquery2()
{
	btnSend.EnableWindow(FALSE);
	m_Terminated = false;

	m_Network_hThread = CreateThread(NULL, NULL, CPCANBasicExampleDlg::CallNetworkOutputFunc, (LPVOID)this, NULL, NULL);
}


void CPCANBasicExampleDlg::OnBnClickedButtonglass()
{
	// TODO: Add your control notification handler code here

	txtCtrlGlass.GetWindowTextA(txtGlass);
	std::stringstream ss;
	ss << txtGlass;
	ss >> m_Shutter_Time;
	m_Shutter_Time_Constant = m_Shutter_Time;

	txtCtrlLaneChange.GetWindowTextA(txtLaneChange);
	std::stringstream lc;
	lc << txtLaneChange;
	lc >> m_LaneChange_Time;
	m_LaneChange_Time_Constant = m_LaneChange_Time;

	m_Shutter_CPUTIME_BEGIN = boost::posix_time::microsec_clock::local_time();
	boost::posix_time::time_duration diff = m_Shutter_CPUTIME_BEGIN - time_t_epoch;
	std::pair<__int64, std::string> temp(diff.total_milliseconds(), "Start");
	m_Shutter_Time_Rec.push_back(temp);

	if (!btnInit.IsWindowEnabled())
	{
		txtCtrlGlass.SetReadOnly(TRUE);
		txtCtrlLaneChange.SetReadOnly(TRUE);
		btnShutterApply.EnableWindow(FALSE);
		mNUDOffset.EnableWindow(FALSE);
	}
	else
	{
		txtCtrlGlass.SetReadOnly(FALSE);
		txtCtrlLaneChange.SetReadOnly(FALSE);
		btnShutterApply.EnableWindow(TRUE);
		mNUDOffset.EnableWindow(TRUE);
	}
}

void CPCANBasicExampleDlg::SetShutterGlass(const bool swt /* = true */)
{
	FT_HANDLE ftHandle;
	FT_STATUS ftStatus;

	ftStatus = FT_OpenEx("FTH7PDPZ", FT_OPEN_BY_SERIAL_NUMBER, &ftHandle);
	if (ftStatus == FT_OK)
	{
		if (swt)
			FT_SetRts(ftHandle); // green line low
		else
			FT_ClrRts(ftHandle); // green line high

		ftStatus = FT_Close(ftHandle);
	}

}

void CPCANBasicExampleDlg::OnBnClickedButtonsw()
{
	btnSetGlassOFF.EnableWindow(FALSE);

	SetShutterGlass();

	boost::posix_time::ptime now = boost::posix_time::microsec_clock::local_time();
	boost::posix_time::time_duration diff = now - time_t_epoch;

	std::pair<__int64, std::string> temp(diff.total_milliseconds(), "OFF");

	m_Shutter_Time_Rec.push_back(temp);

	btnSetGlassOFF.EnableWindow(TRUE);
}


void CPCANBasicExampleDlg::OnBnClickedButtonsw2()
{
	btnSetGlassON.EnableWindow(FALSE);

	SetShutterGlass(false);

	boost::posix_time::ptime now = boost::posix_time::microsec_clock::local_time();
	boost::posix_time::time_duration diff = now - time_t_epoch;

	std::pair<__int64, std::string> temp(diff.total_milliseconds(), "ON");

	m_Shutter_Time_Rec.push_back(temp);

	btnSetGlassON.EnableWindow(TRUE);
}

bool CPCANBasicExampleDlg::FT232RCOM_Exist(char SN[])
{
	unsigned long NumDevices = 0;
	char SerialNumber[256];
	FTID_STATUS dStatus;

	dStatus = FTID_GetNumDevices(&NumDevices);

	if ((dStatus == FTID_SUCCESS) && NumDevices) {

		for (int i = 0; i < (int)NumDevices; i++) {

			dStatus = FTID_GetDeviceSerialNumber(i, SerialNumber, 256);
			if (dStatus == FTID_SUCCESS) {
				if(!strcmp(SN, SerialNumber))
					return true;
			}
		}
	}

	return false;
}

void CPCANBasicExampleDlg::OnDeltaposNudfiltersubs(NMHDR *pNMHDR, LRESULT *pResult)
{
	LPNMUPDOWN pNMUpDown = reinterpret_cast<LPNMUPDOWN>(pNMHDR);
	// TODO: Add your control notification handler code here

	int iNewVal;

	//Compute new selected From value
	iNewVal = pNMUpDown->iPos + ((pNMUpDown->iDelta > 0) ? 1 : -1);
	if (iNewVal < 1)
		iNewVal = 1;

	//Update textBox
	txtFilterSubs.Format("%s%02d", "s",iNewVal);
	UpdateData(FALSE);

	SubsExist();

	*pResult = 0;
}


void CPCANBasicExampleDlg::OnDeltaposNudfiltertrials(NMHDR *pNMHDR, LRESULT *pResult)
{
	LPNMUPDOWN pNMUpDown = reinterpret_cast<LPNMUPDOWN>(pNMHDR);
	// TODO: Add your control notification handler code here

	int iNewVal;

	//Compute new selected From value
	iNewVal = pNMUpDown->iPos + ((pNMUpDown->iDelta > 0) ? 1 : -1);
	if (iNewVal < 1)
		iNewVal = 1;

	//Update textBox
	txtFilterTrials.Format("%s%02d", "t", iNewVal);
	UpdateData(FALSE);

	TrialsExist();

	*pResult = 0;
}

bool CPCANBasicExampleDlg::SubsExist()
{
	CT2CA pszConvertedAnsiString(txtFilterSubs);

	std::string dir = ".\\";
	std::string subdir = txtFilterSubs;
	dir += subdir;

	boost::filesystem::path data_dir(dir);
	bool direxists = boost::filesystem::is_directory(data_dir);
	if (direxists)
	{
		CString Warn;
		Warn.Format("%s%s\n", dir.c_str(), " Already Exists!");
		IncludeTextMessage(Warn);

		return true;
	}
	else
	{
		CString Warn;
		Warn.Format("%s%s\n", dir.c_str(), " is available!");
		IncludeTextMessage(Warn);

		return false;
	}
}

bool CPCANBasicExampleDlg::TrialsExist()
{
	CT2CA pszConvertedAnsiString(txtFilterSubs);

	std::string dir = ".\\";
	std::string subdir = txtFilterSubs;
	dir += subdir;

	CT2CA trials(txtFilterTrials);
	subdir = trials;
	dir += "\\";
	dir += subdir;

	boost::filesystem::path trial_dir(dir);
	if (boost::filesystem::is_directory(trial_dir))
	{
		CString Warn;
		Warn.Format("%s%s\n", dir.c_str(), " Already Exists!");
		IncludeTextMessage(Warn);

		return true;
	}
	else
	{
		CString Warn;
		Warn.Format("%s%s\n", dir.c_str(), " is available!");
		IncludeTextMessage(Warn);

		return false;
	}
}

void CPCANBasicExampleDlg::OnBnClickedButtonsub()
{
	// TODO: Add your control notification handler code here
	bool subs = SubsExist();
	bool trials = TrialsExist();

	if (subs && trials)
	{
// 		CString Warn;
// 		Warn.Format("%s\n", "WILL overwrite results!");
// 		IncludeTextMessage(Warn);

		btnApplySub.EnableWindow(TRUE);
		nudFilterSubs.EnableWindow(TRUE);
		nudFilterTrials.EnableWindow(TRUE);
	}
	else
	{
		btnApplySub.EnableWindow(FALSE);
		nudFilterSubs.EnableWindow(FALSE);
		nudFilterTrials.EnableWindow(FALSE);
	}
}


void CPCANBasicExampleDlg::OnBnClickedButtonzero()
{
	// TODO: Add your control notification handler code here

	if (btnInit.IsWindowEnabled())
	{
		btnZero.EnableWindow(FALSE);
		TXTZERO.EnableWindow(FALSE);

		TXTZERO.GetWindowTextA(ValueZero);
		std::stringstream ss;
		ss << ValueZero;
		ss >> m_zerotime;
	}
	else
	{
		IncludeTextMessage("Cannot zero the Gyro after intialization!");
	}
}


void CPCANBasicExampleDlg::OnBnClickedButtonerate()
{
	// TODO: Add your control notification handler code here

	if (btnInit.IsWindowEnabled())
	{
		btnerate.EnableWindow(FALSE);
		TXTERATE.EnableWindow(FALSE);

		TXTERATE.GetWindowTextA(ValueErate);
		std::stringstream ss;
		ss << ValueErate;
		ss >> m_erate;
	}
	else
	{
		IncludeTextMessage("Cannot set e-rate for the Gyro after intialization!");
	}
}


void CPCANBasicExampleDlg::OnDeltaposNudfilteroffset(NMHDR *pNMHDR, LRESULT *pResult)
{
	LPNMUPDOWN pNMUpDown = reinterpret_cast<LPNMUPDOWN>(pNMHDR);
	// TODO: Add your control notification handler code here

	double iNewVal;

	//Compute new selected From value
	iNewVal = pNMUpDown->iPos + ((pNMUpDown->iDelta > 0) ? 1 : -1);
	iNewVal *= 0.1;
	if (iNewVal < 0)
		iNewVal = 0;

	m_Shutter_Offset = iNewVal;

	//Update textBox
	mValueOffset.Format("%-.2f", iNewVal);
	UpdateData(FALSE);

	*pResult = 0;
}
