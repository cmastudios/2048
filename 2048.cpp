// 2048.cpp : Defines the entry point for the application.
//

#include "framework.h"
#include "2048.h"
#include <random>
#include <array>
#include <cstdint>
#include <intrin.h>
#include <algorithm>

#define MAX_LOADSTRING 100

// Global Variables:
HINSTANCE hInst;                                // current instance
WCHAR szTitle[MAX_LOADSTRING];                  // The title bar text
WCHAR szWindowClass[MAX_LOADSTRING];            // the main window class name

static HBRUSH background;


// Forward declarations of functions included in this code module:
ATOM                MyRegisterClass(HINSTANCE hInstance);
BOOL                InitInstance(HINSTANCE, int);
LRESULT CALLBACK    WndProc(HWND, UINT, WPARAM, LPARAM);
INT_PTR CALLBACK    About(HWND, UINT, WPARAM, LPARAM);

int APIENTRY wWinMain(_In_ HINSTANCE hInstance,
	_In_opt_ HINSTANCE hPrevInstance,
	_In_ LPWSTR    lpCmdLine,
	_In_ int       nCmdShow)
{
	UNREFERENCED_PARAMETER(hPrevInstance);
	UNREFERENCED_PARAMETER(lpCmdLine);

	// TODO: Place code here.

	// Initialize global strings
	LoadStringW(hInstance, IDS_APP_TITLE, szTitle, MAX_LOADSTRING);
	LoadStringW(hInstance, IDC_MY2048, szWindowClass, MAX_LOADSTRING);
	MyRegisterClass(hInstance);

	// Perform application initialization:
	if (!InitInstance(hInstance, nCmdShow))
	{
		return FALSE;
	}

	HACCEL hAccelTable = LoadAccelerators(hInstance, MAKEINTRESOURCE(IDC_MY2048));

	MSG msg;

	// Main message loop:
	while (GetMessage(&msg, nullptr, 0, 0))
	{
		if (!TranslateAccelerator(msg.hwnd, hAccelTable, &msg))
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
	}

	if (background != nullptr) DeleteObject(background);

	return (int)msg.wParam;
}


//
//  FUNCTION: MyRegisterClass()
//
//  PURPOSE: Registers the window class.
//
ATOM MyRegisterClass(HINSTANCE hInstance)
{
	WNDCLASSEXW wcex;

	background = CreateSolidBrush(RGB(0xbb, 0xad, 0xa0));

	wcex.cbSize = sizeof(WNDCLASSEX);

	wcex.style = CS_HREDRAW | CS_VREDRAW;
	wcex.lpfnWndProc = WndProc;
	wcex.cbClsExtra = 0;
	wcex.cbWndExtra = 0;
	wcex.hInstance = hInstance;
	wcex.hIcon = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_MY2048));
	wcex.hCursor = LoadCursor(nullptr, IDC_ARROW);
	wcex.hbrBackground = background;
	wcex.lpszMenuName = MAKEINTRESOURCEW(IDC_MY2048);
	wcex.lpszClassName = szWindowClass;
	wcex.hIconSm = LoadIcon(wcex.hInstance, MAKEINTRESOURCE(IDI_SMALL));

	return RegisterClassExW(&wcex);
}

//
//   FUNCTION: InitInstance(HINSTANCE, int)
//
//   PURPOSE: Saves instance handle and creates main window
//
//   COMMENTS:
//
//        In this function, we save the instance handle in a global variable and
//        create and display the main program window.
//
BOOL InitInstance(HINSTANCE hInstance, int nCmdShow)
{
	hInst = hInstance; // Store instance handle in our global variable

	HWND hWnd = CreateWindowW(szWindowClass, szTitle, WS_OVERLAPPEDWINDOW,
		CW_USEDEFAULT, 0, 600, 600, nullptr, nullptr, hInstance, nullptr);

	if (!hWnd)
	{
		return FALSE;
	}

	ShowWindow(hWnd, nCmdShow);
	UpdateWindow(hWnd);

	return TRUE;
}

#define tile2textcolor RGB(0x77, 0x6e, 0x65)
#define tile8textcolor RGB(0xf9, 0xf6, 0xf2)

#define BOARD_WIDTH 4
#define BOARD_SQUARES 16

static inline unsigned int log2u(const unsigned int x) {
	return 31U - min(__lzcnt(x), 31U);
}

typedef struct { unsigned int* row[4]; } slice_S;

class GameBoard
{
public:
	GameBoard() : board{ 0 }
	{
		place();
	}

	bool place()
	{
		// Find open squares
		bool any_space = false;
		std::array<int, 16> weights = { 0 };
		for (int i = 0; i < 16; ++i)
		{
			if (board[i / 4][i % 4] == 0U)
			{
				weights[i] = 1;
				any_space = true;
			}
		}
		if (any_space)
		{
			std::discrete_distribution<int> dist(weights.begin(), weights.end());
			int i = dist(rng);
			board[i / 4][i % 4] = 2U;
		}
		return any_space;

	}

	bool slide(WPARAM wParam)
	{
		bool stuff_moved = false;
		// say we're squashing down, iterate each column separately
		for (int c = 0; c < 4; ++c)
		{
			slice_S slice = slices(wParam, c);
			// then go up each column and move pieces down if there's a hole
			for (int r = 0; r < 4; r++)
			{
				stuff_moved |= fall(slice, r);
			}
		}
		return stuff_moved;
	}

	bool merge(WPARAM wParam)
	{
		bool merged = false;
		for (int c = 0; c < 4; ++c)
		{
			slice_S slice = slices(wParam, c);
			// then go up each column and move pieces down if there's a hole
			for (int r = 0; r < 3; r++)
			{
				if ((*slice.row[r] != 0U) && (*slice.row[r] == *slice.row[r + 1]))
				{
					*slice.row[r + 1] = 0;
					*slice.row[r] *= 2;
					merged = true;
				}
			}
		}
		return merged;
	}

	bool is_game_over(void)
	{
		GameBoard clone(*this);
		const bool game_over =
			!clone.slide(VK_LEFT) && !clone.slide(VK_RIGHT) && !clone.slide(VK_UP) && !clone.slide(VK_DOWN) &&
			!clone.merge(VK_LEFT) && !clone.merge(VK_RIGHT) && !clone.merge(VK_UP) && !clone.merge(VK_DOWN);
		return game_over;
	}

	void reset(void)
	{
		memset(board, 0, sizeof(board));
		place();
	}

	unsigned int operator()(int x, int y) const
	{
		return board[y][x];
	}
private:
	unsigned int board[4][4];

	bool fall(slice_S s, int i)
	{
		bool stuff_moved = false;
		// fall towards s.row[0]
		if (*s.row[i] != 0)
		{
			for (int r = i; r > 0; --r)
			{
				if (*s.row[r - 1] == 0)
				{
					*s.row[r - 1] = *s.row[r];
					*s.row[r] = 0;
					stuff_moved = true;
				}
			}
		}
		return stuff_moved;
	}

	slice_S slices(WPARAM wParam, int i)
	{
		switch (wParam)
		{
		case VK_LEFT:	return { &board[i][0], &board[i][1], &board[i][2], &board[i][3], };
		case VK_RIGHT:  return { &board[i][3], &board[i][2], &board[i][1], &board[i][0], };
		case VK_UP:     return { &board[0][i], &board[1][i], &board[2][i], &board[3][i], };
		case VK_DOWN:   return { &board[3][i], &board[2][i], &board[1][i], &board[0][i], };
		}
		throw std::logic_error("unk dir");
	}

	std::default_random_engine rng;
};

class GameBoardView
{
public:
	GameBoardView(HWND hWnd)
	{
		HDC hdc = GetDC(hWnd);
		int cHeight = (int)(GetDeviceCaps(hdc, LOGPIXELSY) * 0.4);
		font = CreateFont(cHeight, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Clear Sans");

		tileBackground[0] = CreateSolidBrush(RGB(0xcd, 0xc1, 0xb4));
		tileBackground[log2u(2U)] = CreateSolidBrush(RGB(0xee, 0xe4, 0xda));
		tileBackground[log2u(4U)] = CreateSolidBrush(RGB(0xee, 0xe1, 0xc9));
		tileBackground[log2u(8U)] = CreateSolidBrush(RGB(0xf3, 0xb2, 0x7a)); // text #f9f6f2
		tileBackground[log2u(16U)] = CreateSolidBrush(RGB(0xf6, 0x96, 0x64));
		tileBackground[log2u(32U)] = CreateSolidBrush(RGB(0xf7, 0x7c, 0x5f));
		tileBackground[log2u(64U)] = CreateSolidBrush(RGB(0xf7, 0x5f, 0x3b));
		tileBackground[log2u(128U)] = CreateSolidBrush(RGB(0xed, 0xd0, 0x73));
		tileBackground[log2u(256U)] = CreateSolidBrush(RGB(0xed, 0xcc, 0x62));
		tileBackground[log2u(512U)] = CreateSolidBrush(RGB(0xed, 0xc9, 0x50));
		tileBackground[log2u(1024U)] = CreateSolidBrush(RGB(0xed, 0xc5, 0x3f));
		tileBackground[log2u(2048U)] = CreateSolidBrush(RGB(0x00, 0x00, 0x00));
		ReleaseDC(hWnd, hdc);
	}
	~GameBoardView()
	{
		for (HBRUSH brush : tileBackground)
		{
			if (brush != nullptr)
			{
				DeleteObject(brush);
			}
		}
		if (font != nullptr)
		{
			DeleteObject(font);
		}
	}
	void paintGrid(HDC hdc, RECT rect)
	{
		// Common text settings
		SelectObject(hdc, (HGDIOBJ)font);
		SetBkMode(hdc, TRANSPARENT);
		SetTextAlign(hdc, TA_CENTER);

		TEXTMETRIC tm;
		GetTextMetrics(hdc, &tm);

		SelectObject(hdc, GetStockObject(NULL_PEN));


		const int bordersize = 15;
		const int winsizew = rect.right - rect.left;
		const int winsizeh = rect.bottom - rect.top;
		const int tilesizew = (winsizew - 5 * bordersize) / 4;
		const int tilesizeh = (winsizeh - 5 * bordersize) / 4;
		for (int y = 0; y < 4; ++y)
		{
			for (int x = 0; x < 4; ++x)
			{
				SelectObject(hdc, backgroundBrushForPosition(x, y));
				RoundRect(hdc,
					(x + 1) * bordersize + x * tilesizew,
					(y + 1) * bordersize + y * tilesizeh,
					(x + 1) * bordersize + (x + 1) * tilesizew,
					(y + 1) * bordersize + (y + 1) * tilesizeh,
					5, 5);
				if (board(x, y) > 0)
				{
					SetTextColor(hdc, textColorForPosition(x, y));
					TCHAR buf[10];
					int len = std::swprintf(buf, sizeof(buf) / sizeof(TCHAR), L"%u", board(x, y));
					TextOut(hdc,
						(x + 1) * bordersize + x * tilesizew + tilesizew / 2,
						(y + 1) * bordersize + y * tilesizeh + tilesizeh / 2 - tm.tmAscent / 2,
						buf, len);

				}
			}
		}
	}
	void move(HWND hWnd, WPARAM wParam)
	{
		bool moved = false;
		moved |= board.slide(wParam);
		moved |= board.merge(wParam);
		moved |= board.slide(wParam);
		if (moved)
		{
			board.place();
			InvalidateRect(hWnd, 0, TRUE);
		}
		else if (board.is_game_over())
		{
			auto ret = MessageBox(hWnd, L"You have lost", L"Rip", MB_RETRYCANCEL);
			if (ret == IDRETRY)
			{
				board.reset();
				InvalidateRect(hWnd, 0, TRUE);
			}
		}
	}
	void fnew(HWND hWnd)
	{
		board.reset();
		InvalidateRect(hWnd, 0, TRUE);
	}
private:

	GameBoard board;

	HFONT font;
	std::array<HBRUSH, 12> tileBackground;

	HBRUSH backgroundBrushForPosition(int i, int j)
	{
		unsigned int num = board(i, j);
		unsigned int l2 = min(log2u(num), log2u(2048));
		return tileBackground.at(l2);
	}
	COLORREF textColorForPosition(int i, int j)
	{
		if (board(i, j) >= 8)
			return tile8textcolor;
		else
			return tile2textcolor;
	}
};

static GameBoardView* view;

//
//  FUNCTION: WndProc(HWND, UINT, WPARAM, LPARAM)
//
//  PURPOSE: Processes messages for the main window.
//
//  WM_COMMAND  - process the application menu
//  WM_PAINT    - Paint the main window
//  WM_DESTROY  - post a quit message and return
//
//
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	switch (message)
	{
	case WM_COMMAND:
	{
		int wmId = LOWORD(wParam);
		// Parse the menu selections:
		switch (wmId)
		{
		case IDM_ABOUT:
			DialogBox(hInst, MAKEINTRESOURCE(IDD_ABOUTBOX), hWnd, About);
			break;
		case IDM_EXIT:
			DestroyWindow(hWnd);
			break;
		case ID_FILE_NEW:
			view->fnew(hWnd);
			break;
		default:
			return DefWindowProc(hWnd, message, wParam, lParam);
		}
	}
	break;
	case WM_CREATE:
	{
		view = new GameBoardView(hWnd);
	}
	break;
	case WM_PAINT:
	{
		PAINTSTRUCT ps;
		HDC hdc = BeginPaint(hWnd, &ps);
		// TODO: Add any drawing code that uses hdc here...
		RECT rect;
		GetClientRect(hWnd, &rect);

		view->paintGrid(hdc, rect);

		EndPaint(hWnd, &ps);
	}
	break;
	case WM_KEYDOWN:
		switch (wParam)
		{
		case VK_LEFT:
		case VK_RIGHT:
		case VK_UP:
		case VK_DOWN:
		{
			view->move(hWnd, wParam);
		}
		break;
		}
		break;
	case WM_DESTROY:
		if (view != nullptr)
		{
			delete view;
			view = nullptr;
		}
		PostQuitMessage(0);
		break;
	default:
		return DefWindowProc(hWnd, message, wParam, lParam);
	}
	return 0;
}

// Message handler for about box.
INT_PTR CALLBACK About(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
	UNREFERENCED_PARAMETER(lParam);
	switch (message)
	{
	case WM_INITDIALOG:
		return (INT_PTR)TRUE;

	case WM_COMMAND:
		if (LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL)
		{
			EndDialog(hDlg, LOWORD(wParam));
			return (INT_PTR)TRUE;
		}
		break;
	}
	return (INT_PTR)FALSE;
}
