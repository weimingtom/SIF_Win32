﻿/* 
   Copyright 2013 KLab Inc.

   Licensed under the Apache License, Version 2.0 (the "License");
   you may not use this file except in compliance with the License.
   You may obtain a copy of the License at

       http://www.apache.org/licenses/LICENSE-2.0

   Unless required by applicable law or agreed to in writing, software
   distributed under the License is distributed on an "AS IS" BASIS,
   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
   See the License for the specific language governing permissions and
   limitations under the License.
*/
// GameLibraryWin32.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#include "assert_klb.h"

#include "GameEngine.h"
#include "EngineStdReference.h"
#include "Win32TouchLib.h"
#include "SIF_Win32.h"

#include <map>

#include <assert.h>
#include <float.h>
#include <math.h>
#include <stdio.h>
#include "RenderingFramework.h"
#include <Windows.h>
#include <gl/GL.h>
#include "CPFInterface.h"
#include "CWin32Platform.h"
#include "CWin32PathConv.h"
#include "Win32FileLocation.h"

#include "CKLBLuaEnv.h"
#include "CKLBTouchPad.h"

#define IS_TOUCH ((GetMessageExtraInfo() & 0xFFFFFF00) == 0xFF515700)

// #pragma comment(lib, "GameLibraryWin32.lib")

//
//-----------------------------------------
//  Global Execution Context
//
#include "CKLBDrawTask.h"
#include "CKLBDebugger.h"
#include "CKLBRendering.h"
#include "CKLBAsset.h"

#include <conio.h>

bool SIF_Win32_IS_RELEASE = true;
bool OVERRIDE_IS_RELEASE = false;
bool SIF_Win32_IS_SINGLECORE = false;
bool OVERRIDE_IS_SINGECORE = false;

POINT logical_touch_pos[9] = {
	{16 + 64, 96 + 64},
	{46 + 64, 249+ 64},
	{133+ 64, 378+ 64},
	{262+ 64, 465+ 64},
	{416+ 64, 496+ 64},
	{569+ 64, 465+ 64},
	{698+ 64, 378+ 64},
	{785+ 64, 249+ 64},
	{816+ 64, 96 + 64}
};

// Will be calculated later
POINT physical_touch_pos[9] = {
	{0,0},
	{0,0},
	{0,0},
	{0,0},
	{0,0},
	{0,0},
	{0,0},
	{0,0},
	{0,0}
};

void logical_to_physical(const POINT* logical, POINT* physical)
{
	CKLBDrawResource& dr = CKLBDrawResource::getInstance();

	int x, y = 0;

	dr.toPhisicalPosition(logical->x, logical->y, x, y);

	physical->x = x;
	physical->y = y;
}

//
//-----------------------------------------
//
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);
void EnableOpenGL (HWND hWnd, HDC * hDC, HGLRC * hRC);
void DisableOpenGL(HWND hWnd, HDC hDC, HGLRC hRC);

// Enable OpenGL
void EnableOpenGL(HWND hWnd, HDC * hDC, HGLRC * hRC)
{
	PIXELFORMATDESCRIPTOR pfd;
	int format;
	
	// get the device context (DC)
	*hDC = GetDC( hWnd );
	
	// set the pixel format for the DC
	ZeroMemory( &pfd, sizeof( pfd ) );
	pfd.nSize = sizeof( pfd );
	pfd.nVersion = 1;
	pfd.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
	pfd.iPixelType = PFD_TYPE_RGBA;
	pfd.cColorBits = 24;
	pfd.cDepthBits = 16;
	pfd.iLayerType = PFD_MAIN_PLANE;
	format = ChoosePixelFormat( *hDC, &pfd );
	SetPixelFormat( *hDC, format, &pfd );
	
	// create and enable the render context (RC)
	*hRC = wglCreateContext( *hDC );
	wglMakeCurrent( *hDC, *hRC );
	
}

// Disable OpenGL
void DisableOpenGL(HWND hWnd, HDC hDC, HGLRC hRC)
{
	wglMakeCurrent( NULL, NULL );
	wglDeleteContext( hRC );
	ReleaseDC( hWnd, hDC );
}


const char* gsrc = NULL;

LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{                                                                       
	static int  rot         = 0;
	static int  mouse_stat  = 0;
	static int  lastX, lastY;

	switch (message)
    {
		case WM_CREATE:
			return 0;
		
		case WM_CLOSE:
			{
				if(SIF_Win32::CloseWindowAsBack == false)
					PostQuitMessage( 0 );
				else
					CPFInterface::getInstance().client().inputDeviceKey(IClientRequest::KEY_BACK, IClientRequest::KEYEVENT_CLICK);
				
				break;
			}
		
		case WM_COMMAND:
			{
				CWin32Widget::ControlCommand(hWnd, message, wParam, lParam);
			}
			break;
        case WM_LBUTTONDOWN:
			if(!IS_TOUCH)
			{
				if(mouse_stat) {
					// ボタンを押したまま画面外に出た
					// 最後の座標を使って、無理やり RELEASEを送る
					// If going out of the screen with a pushed button
					// Force sending a RELEASE signal.
					CPFInterface::getInstance().client().inputPoint(63, IClientRequest::I_RELEASE,
							lastX, lastY);
					mouse_stat = 0;
				}
				CPFInterface::getInstance().client().inputPoint(63, IClientRequest::I_CLICK,
						GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
				mouse_stat = 1;
				break;
			}
		case WM_MOUSEMOVE:
			if(!IS_TOUCH)
			{
				if (wParam & MK_LBUTTON) {
					lastX = GET_X_LPARAM(lParam);
					lastY = GET_Y_LPARAM(lParam);
					CPFInterface::getInstance().client().inputPoint(63, IClientRequest::I_DRAG,
							lastX, lastY);
				}
				break;
			}
		case WM_LBUTTONUP:
			if(!IS_TOUCH)
			{
				CPFInterface::getInstance().client().inputPoint(63, IClientRequest::I_RELEASE,
						GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
				mouse_stat = 0;
				break;
			}
		case WM_KEYDOWN:
			switch (wParam) {
			case VK_ESCAPE:
				if(SIF_Win32::CloseWindowAsBack == false)
					PostQuitMessage(0);

				break;
			case VK_BACK:
				{
					if(SIF_Win32::CloseWindowAsBack == false)
						CPFInterface::getInstance().client().inputDeviceKey(IClientRequest::KEY_BACK, IClientRequest::KEYEVENT_CLICK);
				}
				break;
			default:
				if(SIF_Win32::AllowKeyboard)
				{
					for(int i = 0; i < 9; i++)
					{
						IClientRequest* cr = &CPFInterface::getInstance().client();
						if(wParam == SIF_Win32::VirtualKeyIdol[i])
						{
							POINT& physical = physical_touch_pos[i];
							cr->inputPoint(54 + i, IClientRequest::I_CLICK, physical.x, physical.y);
						}
					}
				}
				break;
			}
			break;
		case WM_KEYUP:
			{
				if(SIF_Win32::AllowKeyboard)
				{
					for(int i = 0; i < 9; i++)
					{
						IClientRequest* cr = &CPFInterface::getInstance().client();
						if(wParam == SIF_Win32::VirtualKeyIdol[i])
						{
							POINT& physical = physical_touch_pos[i];
							cr->inputPoint(54 + i, IClientRequest::I_RELEASE, physical.x, physical.y);
						}
					}
				}
			}
			break;
        case WM_DESTROY:
            PostQuitMessage(0);                                             
            break;
		case WM_TOUCH:
			{
				using namespace Win32Touch;

				TouchInputList touchlist = GetTouchList(hWnd, (void*)lParam, LOWORD(wParam));
				IClientRequest& cr = CPFInterface::getInstance().client();

				for(TouchInputList::iterator i = touchlist.begin(); i != touchlist.end(); i++)
				{
					TouchPoint& cur = *i;
					
					//DEBUG_PRINT("TouchID = %d; X = %d; Y = %d; Type = %d", cur.TouchID, cur.X, cur.Y, int(cur.Type));

					switch(cur.Type)
					{
						case TouchDown:
							{
								cr.inputPoint(cur.TouchID, IClientRequest::I_CLICK, cur.X, cur.Y);
								break;
							}
						case TouchUp:
							{
								cr.inputPoint(cur.TouchID, IClientRequest::I_RELEASE, cur.X, cur.Y);
								break;
							}
						case TouchMove:
							{
								cr.inputPoint(cur.TouchID, IClientRequest::I_DRAG, cur.X, cur.Y);
								break;
							}
						default:
							klb_assertAlways("Unknown touch type. TouchID = %d", cur.TouchID);
							break;
					}
				}

				ReleaseTouchHandle((void*)lParam);
			}
			break;
		case WM_SYSCOMMAND:
			{
				IClientRequest& cr = CPFInterface::getInstance().client();

				if(wParam == SC_MINIMIZE)
					cr.pauseGame(true);
				else if(wParam == SC_RESTORE)
					cr.pauseGame(false);
			}
        default:                                                            
            return DefWindowProc(hWnd, message, wParam, lParam);            
    }                                                                   
    return 0;                                                           
}    

void sendEvents() {
	if (gsrc) {
		int c = 0;
		bool exit = false;
		const char* src = gsrc;
		while (!exit && (*src != 0)) {
			int items = sscanf(src,"Event%c", &c);
			if (items == 1) {
				src += 6;
				switch (c) {
				case 'S':
					// Start
					exit = true;	// Point to the next frame.
					break;
				case 'E':
					// End
					break;
				case 'F':
					exit = true;
					break;
				case 'P':
					// Process
					break;
				case '0':
					{
						src++;	// skip :
						CKLBTouchPadQueue& queue = CKLBTouchPadQueue::getInstance();
						int id;
						int type;
						int x;
						int y;
						items = sscanf(src,"%i,%i,%i,%i", &id,&type,&x,&y);
						queue.addQueue(id,(IClientRequest::INPUT_TYPE)type,x,y);
					}
					break;
				case '1':
					// Push while processing, should never happen !
					klb_assertAlways("Should never happend");
					break;
				}
			}

			// Reach until EOL
			while ((*src != 0) && (*src != 0xA) && (*src != 0xD)) {
				src++;
			}

			// Skip EOL
			while ((*src == 0xA) || (*src == 0xD)) {
				src++;
			}
		}

		if (*src == 0) {
			gsrc = NULL; 
		} else {
			gsrc = src;
		}
	}
}

char g_basePath[MAX_PATH];
char g_fileName[MAX_PATH];
static char* g_pathExtern;
static char* g_pathInstall;


char* convertPath(const char* input) {
	int len = strlen(input);
	bool addEnd = false;
	if ((input[len-1] != '\\') && (input[len-1] != '/')) {
		addEnd = true;
	}

	char* buffDest = (char*)malloc(len + 1 + (addEnd ? 1 : 0));
	memcpy(buffDest, input, len);
	for (int n=0; n < len; n++) {
		if (buffDest[n] == '\\') {
			buffDest[n] = '/';
		}
	}
	if (addEnd) {
		buffDest[len] = '/';
		len++;
	}
	buffDest[len] = 0;
	return buffDest;
}

#ifdef _WIN32_WCE
int WINAPI WinMain( HINSTANCE,
                    HINSTANCE,
                    LPTSTR,
                    int)
#else
int GameEngineMain(int argc, _TCHAR* argv[])
// int _main (int argc, const char * const* argv)
#endif
{
	bool bStdModuleExist = EngineStdReference();
	bool is_maximized = false;
	klb_assert(bStdModuleExist, "The links of a system are insufficient.");

	glutInit(&argc, argv);
	glutInitDisplayMode(GLUT_RGB | GLUT_DEPTH);
	glutCreateWindow("GLEW Test");

	GLenum err = glewInit();
	if (GLEW_OK != err)
	{
	  /* Problem: glewInit failed, something is seriously wrong. */
	  fprintf(stderr, "Error: %s\n", glewGetErrorString(err));
	}
	fprintf(stdout, "Status: Using GLEW %s\n", glewGetString(GLEW_VERSION));
	
	int WIDTH = 960;
	int HEIGHT = 640;
	
	int fixedDelta = 0;

	*g_basePath = 0;
	*g_fileName = 0;
	g_pathExtern	= PATH_EXTERN;
	g_pathInstall	= PATH_INSTALL;

	g_fileName[0] = 0;

	bool hasDefaultFont = true;
	bool hasDefaultDB   = false;

	if (argc > 1) {
		int parse	= 1;
		int max		= argc;
		while (parse < max) {
			if(*argv[parse] == '-') {
				if (strcmp("-w",argv[parse]) == 0) {
					sscanf_s(argv[parse+1],"%i",&WIDTH);
				}

				if (strcmp("-h",argv[parse]) == 0) {
					sscanf_s(argv[parse+1],"%i",&HEIGHT);
				}

				if (strcmp("-i",argv[parse]) == 0) {
					g_pathInstall = convertPath(argv[parse+1]);
				}

				if (strcmp("-e",argv[parse]) == 0) {
					g_pathExtern = convertPath(argv[parse+1]);
				}

				if (strcmp("-t",argv[parse]) == 0) {
					fixedDelta = atoi(argv[parse+1]);
				}

				if (strcmp("-enc", argv[parse]) == 0) {
					bool encrypt = false;
					if (stricmp(argv[parse+1],"true") == 0) {
						encrypt = true;
					}

					if (stricmp(argv[parse+1],"1") == 0) {
						encrypt = true;
					}

					CWin32Platform::setEncrypt(encrypt);
				}

				if (strcmp("-maximize", argv[parse]) == 0)
					is_maximized = argv[parse+1][0] == '1';

				if (strcmp("-no", argv[parse]) == 0) {
					if (strcmp("defaultfont", argv[parse+1]) == 0) {
						hasDefaultFont = false;
					}
					else if(strcmp("multicore", argv[parse+1]) == 0)
					{
						SIF_Win32_IS_SINGLECORE = true;
						OVERRIDE_IS_SINGECORE = true;
					}
					else if(strcmp("release", argv[parse+1]) == 0)
					{
						SIF_Win32_IS_RELEASE = false;
						OVERRIDE_IS_RELEASE = true;
					}
				}

				parse += 2;
			} else {
				// Specify the boot file
				const char* file = argv[parse];
				int lenf = strlen(file);
				
				memcpy(g_fileName, file, lenf);
				g_fileName[lenf] = 0;

				// ファイル名そのものは start.lua に相当する起動ファイルとする。
                // File name of the file used as a start.lua
				parse++;
			}
		}
	}

	DWORD Window_Flags = WS_CAPTION | WS_VISIBLE | WS_MINIMIZEBOX | WS_SYSMENU;
	RECT temp = {0, 0, WIDTH, HEIGHT};

	AdjustWindowRect(&temp, Window_Flags, 0);

	int scrW	= temp.right - temp.left;
	int scrH	= temp.bottom - temp.top;

	// Create external folder
	CreateDirectoryA(g_pathExtern, NULL);

	CWin32PathConv& pathconv = CWin32PathConv::getInstance();
	pathconv.setPath(g_pathInstall, g_pathExtern);

	WNDCLASS wc;
	HWND hwnd;
	HDC hDC;
	HGLRC hRC;
	HINSTANCE hInstance = GetModuleHandle(NULL);

	// register window class
	wc.style = CS_OWNDC;
	wc.lpfnWndProc = WndProc;
	wc.cbClsExtra = 0;
	wc.cbWndExtra = 0;
	wc.hInstance = hInstance;
	wc.hIcon = LoadIcon( NULL, IDI_APPLICATION );
	wc.hCursor = LoadCursor( NULL, IDC_ARROW );
	wc.hbrBackground = (HBRUSH)GetStockObject( BLACK_BRUSH );
	wc.lpszMenuName = NULL;
	wc.lpszClassName = "GameEngineGL";
	RegisterClass( &wc );
	
	// create main window
	hwnd = CreateWindowExA(0,
		"GameEngineGL", "Playground", 
		Window_Flags,
		CW_USEDEFAULT, CW_USEDEFAULT, scrW, scrH,
		NULL, NULL, hInstance, NULL );
	
	if (!hwnd)
		return -1;

	// enable OpenGL for the window
	EnableOpenGL( hwnd, &hDC, &hRC );

	// COM Initialization
	CoInitialize(NULL);
	OleInitialize(NULL);
	EnableWindow(hwnd, TRUE);

	CPFInterface& pfif = CPFInterface::getInstance();
	CWin32Platform * pPlatform = new CWin32Platform(hwnd);

	if (!hasDefaultFont) {
		pPlatform->setNoDefaultFont();
	}

	pfif.setPlatformRequest(pPlatform);
	GameSetup();	// client side setup

	// Can only access client AFTER GameSetup.
	pfif.client().setInitParam((hasDefaultDB   ? IClientRequest::ENGINE_USE_DEFAULTDB   : 0)
							|  (hasDefaultFont ? IClientRequest::ENGINE_USE_DEFAULTFONT : 0), NULL); 

	// Set info
	if(OVERRIDE_IS_RELEASE == false)
		SIF_Win32_IS_RELEASE = SIF_Win32::DebugMode == false;
	if(OVERRIDE_IS_SINGECORE == false)
		SIF_Win32_IS_SINGLECORE = SIF_Win32::SingleCore;

	// Check if single core mode is used.
	if(SIF_Win32_IS_SINGLECORE)
		SetProcessAffinityMask(GetCurrentProcess(), 0x1);

	// sound initialize
	SoundSystemInitFor_Win32();
	CWin32AudioMgr::getInstance().init(hwnd);

	/* set as foreground window to give this app focus in case it doesn't have it */
	SetForegroundWindow(hwnd);
	ShowWindow(hwnd, is_maximized ? SW_SHOWMAXIMIZED : SW_SHOWNORMAL);

	glClearColor(1.0f, 0.7f, 0.2039f, 0.0f);
	glDisable( GL_CULL_FACE );

	// set screen size
	if(is_maximized)
	{
		RECT client_area;
		GetClientRect(hwnd, &client_area);

		WIDTH = client_area.right;
		HEIGHT = client_area.bottom;
	}
	pfif.client().setScreenInfo(false, WIDTH, HEIGHT);

	// Register for touch
	if(SIF_Win32::AllowTouchscreen) Win32Touch::RegisterWindowForTouch(hwnd);

	// boot path
	if (strlen(g_fileName)) {
		pfif.client().setFilePath(g_fileName);
	} else {
		pfif.client().setFilePath(NULL);
	}
	if (!pfif.client().initGame()) {
		klb_assertAlways("Could not initialize game, most likely memory error");
	} else {
		static DWORD lastTime = GetTickCount();

		// Calculate touch position
		for(int i = 0; i < 9; i++)
			logical_to_physical(&logical_touch_pos[i], &physical_touch_pos[i]);

		// Main message loop:
		bool quit = false;
		s32 frameTime = pfif.client().getFrameTime();
		IClientRequest& pClient = pfif.client();

		while (!quit)
		{
			/* relay message queue messages to windowproc's */
			MSG msg;
			while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
			{
				TranslateMessage(&msg);

				if (msg.message == WM_QUIT)
				{
					quit = true;
					break;
				}

				DispatchMessage(&msg);
			}

			if (!quit) {
				// This is not the safest or best way to handle timing, but this code
				// is only added to make the triangle rotate at a basically constant
				// rate, independent of the target (Win32) platform
				DWORD newTime   = GetTickCount();
				DWORD delta     = newTime - lastTime;

				// Handle rollover
				if (newTime < lastTime) {
					delta = 0;
				} else {
					if (delta > (DWORD)frameTime) {
						sendEvents();

						lastTime = newTime;
						//dglClear(GL_COLOR_BUFFER_BIT);
	
						//
						// Rendering complete.
						//		
						//testCodeLoop(delta);
						quit = !pClient.frameFlip(fixedDelta ? fixedDelta : delta);

						// pfIF.platform().flipFrame();
						SwapBuffers( hDC );
					}
                    // コントロール(ex. TextBox)が作られている場合、その再描画を行う
					// If a Control (ex TextBox) is done, redraw them.
					CWin32Widget::ReDrawControls();
				}
			}
			Sleep(1); // To cool down the processor
		}
	}

	pfif.client().finishGame();

	SoundSystemExitFor_Win32();

	delete pPlatform;

	// shutdown OpenGL
	DisableOpenGL( hwnd, hDC, hRC );

	CWin32AudioMgr::getInstance().release();

	// End of COM
	OleUninitialize();
	CoUninitialize();

	if(DestroyWindow (hwnd)) {
		printf("DestroyWindow SUCCESS\n");
    }
	return 0;
}
