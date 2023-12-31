
// ServerDlg.cpp : implementation file
//

#include "stdafx.h"
#include "Server.h"
#include "ServerDlg.h"
#include "afxdialogex.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

#include <winsock2.h>

#define PORT 5150			// Порт по умолчанию
#define DATA_BUFSIZE 8192 	// Размер буфера по умолчанию

int  iPort = PORT; 	 // Порт для прослушивания подключений
bool bPrint = false; // Выводить ли сообщения клиентов

typedef struct _SOCKET_INFORMATION {
	CHAR Buffer[DATA_BUFSIZE];
	WSABUF DataBuf;
	SOCKET Socket;
	DWORD BytesSEND;
	DWORD BytesRECV;
} SOCKET_INFORMATION, *LPSOCKET_INFORMATION;

BOOL CreateSocketInformation(SOCKET s, char *Str,
	CListBox *pLB);
void FreeSocketInformation(DWORD Event, char *Str,
	CListBox *pLB);

DWORD EventTotal = 0;
WSAEVENT EventArray[WSA_MAXIMUM_WAIT_EVENTS];
LPSOCKET_INFORMATION SocketArray[WSA_MAXIMUM_WAIT_EVENTS];

HWND   hWnd_LB;  // Для вывода в других потоках

UINT ListenThread(PVOID lpParam);

// CServerDlg dialog

CServerDlg::CServerDlg(CWnd* pParent /*=nullptr*/)
	: CDialogEx(IDD_SERVER_DIALOG, pParent)
{
	m_hIcon = AfxGetApp()->LoadIcon(IDR_MAINFRAME);
}

void CServerDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialogEx::DoDataExchange(pDX);
	DDX_Control(pDX, IDC_LISTBOX, m_ListBox);
}

BEGIN_MESSAGE_MAP(CServerDlg, CDialogEx)
	ON_WM_PAINT()
	ON_WM_QUERYDRAGICON()
	ON_BN_CLICKED(IDC_START, &CServerDlg::OnBnClickedStart)
	ON_BN_CLICKED(IDC_PRINT, &CServerDlg::OnBnClickedPrint)
END_MESSAGE_MAP()


// CServerDlg message handlers

BOOL CServerDlg::OnInitDialog()
{
	CDialogEx::OnInitDialog();

	// Set the icon for this dialog.  The framework does this automatically
	//  when the application's main window is not a dialog
	SetIcon(m_hIcon, TRUE);			// Set big icon
	SetIcon(m_hIcon, FALSE);		// Set small icon

	// TODO: Add extra initialization here
	char Str[128];

	sprintf_s(Str, sizeof(Str), "%d", iPort);
	GetDlgItem(IDC_PORT)->SetWindowText(Str);

	if (bPrint)
		((CButton *)GetDlgItem(IDC_PRINT))->SetCheck(1);
	else
		((CButton *)GetDlgItem(IDC_PRINT))->SetCheck(0);

	return TRUE;  // return TRUE  unless you set the focus to a control
}

// If you add a minimize button to your dialog, you will need the code below
//  to draw the icon.  For MFC applications using the document/view model,
//  this is automatically done for you by the framework.

void CServerDlg::OnPaint()
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
		CDialogEx::OnPaint();
	}
}

// The system calls this function to obtain the cursor to display while the user drags
//  the minimized window.
HCURSOR CServerDlg::OnQueryDragIcon()
{
	return static_cast<HCURSOR>(m_hIcon);
}



void CServerDlg::OnBnClickedStart()
{
	// TODO: Add your control notification handler code here
	char Str[81];

	hWnd_LB = m_ListBox.m_hWnd;   // Для ListenThread
	GetDlgItem(IDC_PORT)->GetWindowText(Str, sizeof(Str));
	iPort = atoi(Str);
	if (iPort <= 0 || iPort >= 0x10000)
	{
		AfxMessageBox("Incorrect Port number");
		return;
	}

	AfxBeginThread(ListenThread, NULL);

	GetDlgItem(IDC_START)->EnableWindow(false);
}

UINT ListenThread(PVOID lpParam)
{
	SOCKET Listen;
	SOCKET Accept;
	SOCKADDR_IN InternetAddr;
	DWORD Event;
	WSANETWORKEVENTS NetworkEvents;
	WSADATA wsaData;
	DWORD Ret;
	DWORD Flags;
	DWORD RecvBytes;
	DWORD SendBytes;
	char  Str[200];
	CListBox  *pLB =
		(CListBox *)(CListBox::FromHandle(hWnd_LB));

	if ((Ret = WSAStartup(0x0202, &wsaData)) != 0)
	{
		sprintf_s(Str, sizeof(Str),
			"WSAStartup() failed with error %d", Ret);
		pLB->AddString(Str);
		return 1;
	}

	if ((Listen = socket(AF_INET, SOCK_STREAM, 0)) ==
		INVALID_SOCKET)
	{
		sprintf_s(Str, sizeof(Str),
			"socket() failed with error %d",
			WSAGetLastError());
		pLB->AddString(Str);
		return 1;
	}

	CreateSocketInformation(Listen, Str, pLB);

	if (WSAEventSelect(Listen, EventArray[EventTotal - 1],
		FD_ACCEPT | FD_CLOSE) == SOCKET_ERROR)
	{
		sprintf_s(Str, sizeof(Str),
			"WSAEventSelect() failed with error %d",
			WSAGetLastError());
		pLB->AddString(Str);
		return 1;
	}

	InternetAddr.sin_family = AF_INET;
	InternetAddr.sin_addr.s_addr = htonl(INADDR_ANY);
	InternetAddr.sin_port = htons(iPort);

	if (bind(Listen, (PSOCKADDR)&InternetAddr,
		sizeof(InternetAddr)) == SOCKET_ERROR)
	{
		sprintf_s(Str, sizeof(Str),
			"bind() failed with error %d",
			WSAGetLastError());
		pLB->AddString(Str);
		return 1;
	}

	if (listen(Listen, 5))
	{
		sprintf_s(Str, sizeof(Str),
			"listen() failed with error %d",
			WSAGetLastError());
		pLB->AddString(Str);
		return 1;
	}

	while (TRUE)
	{
		// Дожидаемся уведомления о событии на любом сокете
		if ((Event = WSAWaitForMultipleEvents(EventTotal,
			EventArray, FALSE, WSA_INFINITE, FALSE)) ==
			WSA_WAIT_FAILED)
		{
			sprintf_s(Str, sizeof(Str),
				"WSAWaitForMultipleEvents failed"
				" with error %d", WSAGetLastError());
			pLB->AddString(Str);
			return 1;
		}

		if (WSAEnumNetworkEvents(
			SocketArray[Event - WSA_WAIT_EVENT_0]->Socket,
			EventArray[Event - WSA_WAIT_EVENT_0],
			&NetworkEvents) == SOCKET_ERROR)
		{
			sprintf_s(Str, sizeof(Str),
				"WSAEnumNetworkEvents failed"
				" with error %d", WSAGetLastError());
			pLB->AddString(Str);
			return 1;
		}

		if (NetworkEvents.lNetworkEvents & FD_ACCEPT)
		{
			if (NetworkEvents.iErrorCode[FD_ACCEPT_BIT] != 0)
			{
				sprintf_s(Str, sizeof(Str),
					"FD_ACCEPT failed with error %d",
					NetworkEvents.iErrorCode[FD_ACCEPT_BIT]);
				pLB->AddString(Str);
				break;
			}

			// Прием нового соединения и добавление его
			// в списки сокетов и событий
			if ((Accept = accept(
				SocketArray[Event - WSA_WAIT_EVENT_0]->Socket,
				NULL, NULL)) == INVALID_SOCKET)
			{
				sprintf_s(Str, sizeof(Str),
					"accept() failed with error %d",
					WSAGetLastError());
				pLB->AddString(Str);
				break;
			}

			// Слишком много сокетов. Закрываем соединение.
			if (EventTotal > WSA_MAXIMUM_WAIT_EVENTS)
			{
				sprintf_s(Str, sizeof(Str),
					"Too many connections - closing socket.");
				pLB->AddString(Str);
				closesocket(Accept);
				break;
			}

			CreateSocketInformation(Accept, Str, pLB);

			if (WSAEventSelect(Accept,
				EventArray[EventTotal - 1],
				FD_READ | FD_WRITE | FD_CLOSE) ==
				SOCKET_ERROR)
			{
				sprintf_s(Str, sizeof(Str),
					"WSAEventSelect()failed"
					" with error %d", WSAGetLastError());
				pLB->AddString(Str);
				return 1;
			}

			sprintf_s(Str, sizeof(Str),
				"Socket %d connected", Accept);
			pLB->AddString(Str);
		}

		// Пытаемся читать или писать данные, 
		// если произошло соответствующее событие

		if (NetworkEvents.lNetworkEvents & FD_READ ||
			NetworkEvents.lNetworkEvents & FD_WRITE)
		{
			if (NetworkEvents.lNetworkEvents & FD_READ &&
				NetworkEvents.iErrorCode[FD_READ_BIT] != 0)
			{
				sprintf_s(Str, sizeof(Str),
					"FD_READ failed with error %d",
					NetworkEvents.iErrorCode[FD_READ_BIT]);
				pLB->AddString(Str);
				break;
			}

			if (NetworkEvents.lNetworkEvents & FD_WRITE &&
				NetworkEvents.iErrorCode[FD_WRITE_BIT] != 0)
			{
				sprintf_s(Str, sizeof(Str),
					"FD_WRITE failed with error %d",
					NetworkEvents.iErrorCode[FD_WRITE_BIT]);
				pLB->AddString(Str);
				break;
			}

			LPSOCKET_INFORMATION SocketInfo =
				SocketArray[Event - WSA_WAIT_EVENT_0];

			// Читаем данные только если приемный буфер пуст

			if (SocketInfo->BytesRECV == 0)
			{
				SocketInfo->DataBuf.buf = SocketInfo->Buffer;
				SocketInfo->DataBuf.len = DATA_BUFSIZE;

				Flags = 0;
				if (WSARecv(SocketInfo->Socket,
					&(SocketInfo->DataBuf), 1, &RecvBytes,
					&Flags, NULL, NULL) == SOCKET_ERROR)
				{
					if (WSAGetLastError() != WSAEWOULDBLOCK)
					{
						sprintf_s(Str, sizeof(Str),
							"WSARecv()failed with "
							" error %d", WSAGetLastError());
						pLB->AddString(Str);
						FreeSocketInformation(
							Event - WSA_WAIT_EVENT_0, Str, pLB);
						return 1;
					}
				}
				else
				{
					SocketInfo->BytesRECV = RecvBytes;
					// Вывод сообщения, если требуется
					if (bPrint)
					{
						unsigned l = sizeof(Str) - 1;
						if (l > RecvBytes) l = RecvBytes;
						strncpy_s(Str, SocketInfo->Buffer, l);
						Str[l] = 0;
						pLB->AddString(Str);
					}
				}
			}

			// Отправка данных, если это возможно

			if (SocketInfo->BytesRECV > SocketInfo->BytesSEND)
			{
				SocketInfo->DataBuf.buf =
					SocketInfo->Buffer + SocketInfo->BytesSEND;
				SocketInfo->DataBuf.len =
					SocketInfo->BytesRECV - SocketInfo->BytesSEND;

				if (WSASend(SocketInfo->Socket,
					&(SocketInfo->DataBuf), 1,
					&SendBytes, 0, NULL, NULL) ==
					SOCKET_ERROR)
				{
					if (WSAGetLastError() != WSAEWOULDBLOCK)
					{
						sprintf_s(Str, sizeof(Str),
							"WSASend() failed with "
							"error %d", WSAGetLastError());
						pLB->AddString(Str);
						FreeSocketInformation(
							Event - WSA_WAIT_EVENT_0, Str, pLB);
						return 1;
					}

					// Произошла ошибка WSAEWOULDBLOCK. 
					// Событие FD_WRITE будет отправлено, когда
					// в буфере будет больше свободного места
				}
				else
				{
					SocketInfo->BytesSEND += SendBytes;

					if (SocketInfo->BytesSEND ==
						SocketInfo->BytesRECV)
					{
						SocketInfo->BytesSEND = 0;
						SocketInfo->BytesRECV = 0;
					}
				}
			}
		}

		if (NetworkEvents.lNetworkEvents & FD_CLOSE)
		{
			if (NetworkEvents.iErrorCode[FD_CLOSE_BIT] != 0)
			{
				sprintf_s(Str, sizeof(Str),
					"FD_CLOSE failed with error %d",
					NetworkEvents.iErrorCode[FD_CLOSE_BIT]);
				pLB->AddString(Str);
			}

			sprintf_s(Str, sizeof(Str),
				"Closing socket information %d",
				SocketArray[Event - WSA_WAIT_EVENT_0]->Socket);
			pLB->AddString(Str);

			FreeSocketInformation(Event - WSA_WAIT_EVENT_0,
				Str, pLB);
		}
	} // while
	return 0;
}


BOOL CreateSocketInformation(SOCKET s, char *Str,
	CListBox *pLB)
{
	LPSOCKET_INFORMATION SI;

	if ((EventArray[EventTotal] = WSACreateEvent()) ==
		WSA_INVALID_EVENT)
	{
		sprintf_s(Str, sizeof(Str),
			"WSACreateEvent() failed with error %d",
			WSAGetLastError());
		pLB->AddString(Str);
		return FALSE;
	}

	if ((SI = (LPSOCKET_INFORMATION)GlobalAlloc(GPTR,
		sizeof(SOCKET_INFORMATION))) == NULL)
	{
		sprintf_s(Str, sizeof(Str),
			"GlobalAlloc() failed with error %d",
			GetLastError());
		pLB->AddString(Str);
		return FALSE;
	}

	// Подготовка структуры SocketInfo для использования.
	SI->Socket = s;
	SI->BytesSEND = 0;
	SI->BytesRECV = 0;

	SocketArray[EventTotal] = SI;
	EventTotal++;
	return(TRUE);
}

void FreeSocketInformation(DWORD Event, char *Str,
	CListBox *pLB)
{
	LPSOCKET_INFORMATION SI = SocketArray[Event];
	DWORD i;

	closesocket(SI->Socket);
	GlobalFree(SI);
	WSACloseEvent(EventArray[Event]);

	// Сжатие массивов сокетов и событий

	for (i = Event; i < EventTotal; i++)
	{
		EventArray[i] = EventArray[i + 1];
		SocketArray[i] = SocketArray[i + 1];
	}

	EventTotal--;
}


void CServerDlg::OnBnClickedPrint()
{
	// TODO: Add your control notification handler code here
	if (((CButton *)GetDlgItem(IDC_PRINT))->GetCheck() == 1)
		bPrint = true;
	else
		bPrint = false;
}
