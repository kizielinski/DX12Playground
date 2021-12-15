#include "DXCore.h"
#include "Input.h"
#include "DX12Helper.h"

#include <WindowsX.h>
#include <sstream>

// Define the static instance variable so our OS-level 
// message handling function below can talk to our object
DXCore* DXCore::DXCoreInstance = 0;

// --------------------------------------------------------
// The global callback function for handling windows OS-level messages.
//
// This needs to be a global function (not part of a class), but we want
// to forward the parameters to our class to properly handle them.
// --------------------------------------------------------
LRESULT DXCore::WindowProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	return DXCoreInstance->ProcessMessage(hWnd, uMsg, wParam, lParam);
}

// --------------------------------------------------------
// Constructor - Set up fields and timer
//
// hInstance	- The application's OS-level handle (unique ID)
// titleBarText - Text for the window's title bar
// windowWidth	- Width of the window's client (internal) area
// windowHeight - Height of the window's client (internal) area
// debugTitleBarStats - Show debug stats in the title bar, like FPS?
// --------------------------------------------------------
DXCore::DXCore(
	HINSTANCE hInstance,		// The application's handle
	const char* titleBarText,	// Text for the window's title bar
	unsigned int windowWidth,	// Width of the window's client area
	unsigned int windowHeight,	// Height of the window's client area
	bool debugTitleBarStats)	// Show extra stats (fps) in title bar?
{
	// Save a static reference to this object.
	//  - Since the OS-level message function must be a non-member (global) function, 
	//    it won't be able to directly interact with our DXCore object otherwise.
	//  - (Yes, a singleton might be a safer choice here).
	DXCoreInstance = this;

	// Save params
	this->hInstance = hInstance;
	this->titleBarText = titleBarText;
	this->width = windowWidth;
	this->height = windowHeight;
	this->titleBarStats = debugTitleBarStats;

	// Initialize fields
	this->hasFocus = true; 
	
	this->fpsFrameCount = 0;
	this->fpsTimeElapsed = 0.0f;
	this->currentTime = 0;
	this->deltaTime = 0;
	this->startTime = 0;
	this->totalTime = 0;

	// Query performance counter for accurate timing information
	__int64 perfFreq;
	QueryPerformanceFrequency((LARGE_INTEGER*)&perfFreq);
	perfCounterSeconds = 1.0 / (double)perfFreq;
}

// --------------------------------------------------------
// Destructor - Clean up (release) all DirectX references
// --------------------------------------------------------
DXCore::~DXCore()
{
	// Note: Since we're using smart pointers (ComPtr),
	// we don't need to explicitly clean up those DirectX objects
	// - If we weren't using smart pointers, we'd need
	//   to call Release() on each DirectX object created in DXCore

	// Delete input manager singleton
	delete& Input::GetInstance();
	delete& DX12Helper::GetInstance();
}

// --------------------------------------------------------
// Created the actual window for our application
// --------------------------------------------------------
HRESULT DXCore::InitWindow()
{
	// Start window creation by filling out the
	// appropriate window class struct
	WNDCLASS wndClass		= {}; // Zero out the memory
	wndClass.style			= CS_HREDRAW | CS_VREDRAW;	// Redraw on horizontal or vertical movement/adjustment
	wndClass.lpfnWndProc	= DXCore::WindowProc;
	wndClass.cbClsExtra		= 0;
	wndClass.cbWndExtra		= 0;
	wndClass.hInstance		= hInstance;						// Our app's handle
	wndClass.hIcon			= LoadIcon(NULL, IDI_APPLICATION);	// Default icon
	wndClass.hCursor		= LoadCursor(NULL, IDC_ARROW);		// Default arrow cursor
	wndClass.hbrBackground	= (HBRUSH)GetStockObject(BLACK_BRUSH);
	wndClass.lpszMenuName	= NULL;
	wndClass.lpszClassName	= "Direct3DWindowClass";

	// Attempt to register the window class we've defined
	if (!RegisterClass(&wndClass))
	{
		// Get the most recent error
		DWORD error = GetLastError();

		// If the class exists, that's actually fine.  Otherwise,
		// we can't proceed with the next step.
		if (error != ERROR_CLASS_ALREADY_EXISTS)
			return HRESULT_FROM_WIN32(error);
	}

	// Adjust the width and height so the "client size" matches
	// the width and height given (the inner-area of the window)
	RECT clientRect;
	SetRect(&clientRect, 0, 0, width, height);
	AdjustWindowRect(
		&clientRect,
		WS_OVERLAPPEDWINDOW,	// Has a title bar, border, min and max buttons, etc.
		false);					// No menu bar

	// Center the window to the screen
	RECT desktopRect;
	GetClientRect(GetDesktopWindow(), &desktopRect);
	int centeredX = (desktopRect.right / 2) - (clientRect.right / 2);
	int centeredY = (desktopRect.bottom / 2) - (clientRect.bottom / 2);

	// Actually ask Windows to create the window itself
	// using our settings so far.  This will return the
	// handle of the window, which we'll keep around for later
	hWnd = CreateWindow(
		wndClass.lpszClassName,
		titleBarText.c_str(),
		WS_OVERLAPPEDWINDOW,
		centeredX,
		centeredY,
		clientRect.right - clientRect.left,	// Calculated width
		clientRect.bottom - clientRect.top,	// Calculated height
		0,			// No parent window
		0,			// No menu
		hInstance,	// The app's handle
		0);			// No other windows in our application

	// Ensure the window was created properly
	if (hWnd == NULL)
	{
		DWORD error = GetLastError();
		return HRESULT_FROM_WIN32(error);
	}

	// The window exists but is not visible yet
	// We need to tell Windows to show it, and how to show it
	ShowWindow(hWnd, SW_SHOW);

	// Initialize the input manager now that we definitely have a window
	Input::GetInstance().Initialize(hWnd);

	// Return an "everything is ok" HRESULT value
	return S_OK;
}


// --------------------------------------------------------
// Initializes DirectX, which requires a window.  This method
// also creates several DirectX objects we'll need to start
// drawing things to the screen.
// --------------------------------------------------------
HRESULT DXCore::InitDirectX()
{
#if defined(DEBUG) || defined(_DEBUG)
	// Enable debug layer for runtime debug errors/warnings in visual studio output (Don't forget to check this)
	ID3D12Debug* debugController;
	D3D12GetDebugInterface(IID_PPV_ARGS(&debugController));
	debugController->EnableDebugLayer();
#endif

	// Result variable for below function calls
	HRESULT hr = S_OK;
	// Create the DX 12 device and check the feature level
	{
		hr = D3D12CreateDevice(
			0, // Not explicitly specifying which adapter (GPU)
			D3D_FEATURE_LEVEL_11_0, // MINIMUM feature level - NOT the level we'll necessarily turn on
			IID_PPV_ARGS(device.GetAddressOf())); // Macro to grab necessary IDs of device
		if (FAILED(hr)) return hr;
		// Now that we have a device, determine the maximum
		// feature level supported by the device
		D3D_FEATURE_LEVEL levelsToCheck[] = {
		D3D_FEATURE_LEVEL_11_0,
		D3D_FEATURE_LEVEL_11_1,
		D3D_FEATURE_LEVEL_12_0,
		D3D_FEATURE_LEVEL_12_1
		};
		D3D12_FEATURE_DATA_FEATURE_LEVELS levels = {};
		levels.pFeatureLevelsRequested = levelsToCheck;
		levels.NumFeatureLevels = ARRAYSIZE(levelsToCheck);
		device->CheckFeatureSupport(
			D3D12_FEATURE_FEATURE_LEVELS,
			&levels,
			sizeof(D3D12_FEATURE_DATA_FEATURE_LEVELS)); //Establishes what the device is capable of.
		dxFeatureLevel = levels.MaxSupportedFeatureLevel;
	}

	// Set up DX12 command allocator / queue / list, 
	// which are necessary pieces for issuing standard API calls
	{
		// Set up allocator
		device->CreateCommandAllocator(
			D3D12_COMMAND_LIST_TYPE_DIRECT,
			IID_PPV_ARGS(commandAllocator.GetAddressOf()));

		// Command queue
		D3D12_COMMAND_QUEUE_DESC qDesc = {};
		qDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
		qDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
		device->CreateCommandQueue(&qDesc, IID_PPV_ARGS(commandQueue.GetAddressOf()));

		// Command list
		device->CreateCommandList(
			0, // Which physical GPU will handle these tasks?  0 for single GPU setup
			D3D12_COMMAND_LIST_TYPE_DIRECT, // Type of command list - direct is for standard API calls
			commandAllocator.Get(), // The allocator for this list
			0, // Initial pipeline state - none for now
			IID_PPV_ARGS(commandList.GetAddressOf()));
	}

	// Now that we have a device and a command list stuff,
	// we can initialize the DX12 helper singleton
	{
		DX12Helper::GetInstance().Initialize(
			device,
			commandList,
			commandQueue,
			commandAllocator);
	}

	// Swap chain creation
	{
		// Create a description of how our swap chain should work
		DXGI_SWAP_CHAIN_DESC swapDesc = {};
		swapDesc.BufferCount = numBackBuffers;
		swapDesc.BufferDesc.Width = width;
		swapDesc.BufferDesc.Height = height;
		swapDesc.BufferDesc.RefreshRate.Numerator = 60;
		//swapDesc.BufferDesc.RefreshRate.Numerator = 144; Ask Chris?
		swapDesc.BufferDesc.RefreshRate.Denominator = 1;
		swapDesc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		swapDesc.BufferDesc.ScanlineOrdering = DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED;
		swapDesc.BufferDesc.Scaling = DXGI_MODE_SCALING_UNSPECIFIED;
		swapDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
		swapDesc.Flags = 0;
		swapDesc.OutputWindow = hWnd;
		swapDesc.SampleDesc.Count = 1;
		swapDesc.SampleDesc.Quality = 0;
		swapDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
		swapDesc.Windowed = true;

		// Create a DXGI factory, which is what we use to create a swap chain
		Microsoft::WRL::ComPtr<IDXGIFactory> dxgiFactory;
		CreateDXGIFactory(IID_PPV_ARGS(dxgiFactory.GetAddressOf()));
		hr = dxgiFactory->CreateSwapChain(commandQueue.Get(), &swapDesc, swapChain.GetAddressOf());
	}

	// Create back buffers
	{
		// What is the increment size between RTV descriptors in a
		// descriptor heap?  This differs per GPU so we need to 
		// get it at applications start up
		rtvDescriptorSize = device -> GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

		// First create a descriptor heap for RTVs
		D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc = {};
		rtvHeapDesc.NumDescriptors = numBackBuffers;
		rtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
		device->CreateDescriptorHeap(&rtvHeapDesc, IID_PPV_ARGS(rtvHeap.GetAddressOf()));

		// Now create the RTV handles for each buffer (buffers were created by the swap chain)
		for (unsigned int i = 0; i < numBackBuffers; i++)
		{
			// Grab this buffer from the swap chain
			swapChain->GetBuffer(i, IID_PPV_ARGS(backBuffers[i].GetAddressOf()));

			// Make a handle for it
			rtvHandles[i] = rtvHeap->GetCPUDescriptorHandleForHeapStart();
			rtvHandles[i].ptr += rtvDescriptorSize * i; //Offset by size of descriptor, ex. 2nd one starts at the end of memory of the first descriptor.

			// Create the render target view
			device->CreateRenderTargetView(backBuffers[i].Get(), 0, rtvHandles[i]);
		}
	}

	// Create depth/stencil buffer
	{
		// Create a descriptor heap for DSV
		D3D12_DESCRIPTOR_HEAP_DESC dsvHeapDesc = {};
		dsvHeapDesc.NumDescriptors = 1;
		dsvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
		device->CreateDescriptorHeap(&dsvHeapDesc, IID_PPV_ARGS(dsvHeap.GetAddressOf()));

		// Describe the depth stencil buffer resource
		D3D12_RESOURCE_DESC depthBufferDesc = {}; //All description structs are now all one type: RESOURCE. Generalizes it, but means we have to fill out more.
		depthBufferDesc.Alignment = 0;
		depthBufferDesc.DepthOrArraySize = 1;
		depthBufferDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
		depthBufferDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;
		depthBufferDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT; //(S8) A stencil buffer is used to mask pixels in an image, we have not implemented this.
		depthBufferDesc.Height = height;
		depthBufferDesc.Width = width;
		depthBufferDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
		depthBufferDesc.MipLevels = 1;
		depthBufferDesc.SampleDesc.Count = 1;
		depthBufferDesc.SampleDesc.Quality = 0;

		// Describe the clear value that will most often be used
		// for this buffer (which optimizes the clearing of the buffer)
		D3D12_CLEAR_VALUE clear = {};
		clear.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
		clear.DepthStencil.Depth = 1.0f;
		clear.DepthStencil.Stencil = 0;

		// Describe the memory heap that will house this resource
		//Optimizations and where things live in memory.
		D3D12_HEAP_PROPERTIES props = {};
		props.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
		props.CreationNodeMask = 1;
		props.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
		props.Type = D3D12_HEAP_TYPE_DEFAULT; //This definition is most important piece, probably the only piece we would change if at all.
		props.VisibleNodeMask = 1;

		// Actually create the resource, and the heap in which it
		// will reside, and map the resource to that heap
		device->CreateCommittedResource(
			&props,
			D3D12_HEAP_FLAG_NONE,
			&depthBufferDesc,
			D3D12_RESOURCE_STATE_DEPTH_WRITE, //Only allows writing of Depth information.
			&clear,
			IID_PPV_ARGS(depthStencilBuffer.GetAddressOf())); //Gets it into GPU memory for usage

		// Get the handle to the Depth Stencil View that we'll
		// be using for the depth buffer.  The DSV is stored in
		// our DSV-specific descriptor Heap.
		dsvHandle = dsvHeap->GetCPUDescriptorHandleForHeapStart(); //Use this to swap on runtime.

		// Actually make the DSV
		device->CreateDepthStencilView(
			depthStencilBuffer.Get(),
			0, // Default view (first mip)
			dsvHandle);
	}

	// Set up the viewport so we render into the correct
	// portion of the render target
	viewport = {};
	viewport.TopLeftX = 0;
	viewport.TopLeftY = 0;
	viewport.Width = (float)width;
	viewport.Height = (float)height;
	viewport.MinDepth = 0.0f;
	viewport.MaxDepth = 1.0f;

	// Define a scissor rectangle that defines a portion of
	// the render target for clipping.  This is different from
	// a viewport in that it is applied after the pixel shader.
	// We need at least one of these, but we're rendering to 
	// the entire window, so it'll be the same size.
	scissorRect = {};
	scissorRect.left = 0;
	scissorRect.top = 0;
	scissorRect.right = width;
	scissorRect.bottom = height;

	// Wait for the GPU to catch up
	DX12Helper::GetInstance().WaitForGPU();

	// Return the "everything is ok" HRESULT value
	return S_OK;
}

// --------------------------------------------------------
// When the window is resized, the underlying 
// buffers (textures) must also be resized to match.
//
// If we don't do this, the window size and our rendering
// resolution won't match up.  This can result in odd
// stretching/skewing.
// --------------------------------------------------------
void DXCore::OnResize()
{
	DX12Helper& dx12Helper = DX12Helper::GetInstance();

	// Wait for the GPU to finish all work, since we'll
	// be destroying and recreating resources
	dx12Helper.WaitForGPU();

	// Release the back buffers using ComPtr's Reset()
	for (unsigned int i = 0; i < numBackBuffers; i++)
		backBuffers[i].Reset();

	// Resize the swap chain (assuming a basic color format here)
	swapChain->ResizeBuffers(numBackBuffers, width, height, DXGI_FORMAT_R8G8B8A8_UNORM, 0);

	// Go through the steps to setup the back buffers again
	// Note: This assumes the descriptor heap already exists
	// and that the rtvDescriptorSize was previously set
	for (unsigned int i = 0; i < numBackBuffers; i++)
	{
		// Grab this buffer from the swap chain
		swapChain->GetBuffer(i, IID_PPV_ARGS(backBuffers[i].GetAddressOf()));

		// Make a handle for it
		rtvHandles[i] = rtvHeap->GetCPUDescriptorHandleForHeapStart();
		rtvHandles[i].ptr += rtvDescriptorSize * i;

		// Create the render target view
		device->CreateRenderTargetView(backBuffers[i].Get(), 0, rtvHandles[i]);
	}

	// Reset back to the first back buffer
	currentSwapBuffer = 0;

	// Reset the depth buffer and create it again
	{
		depthStencilBuffer.Reset();
		// Describe the depth stencil buffer resource
		D3D12_RESOURCE_DESC depthBufferDesc = {};
		depthBufferDesc.Alignment = 0;
		depthBufferDesc.DepthOrArraySize = 1;
		depthBufferDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
		depthBufferDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;
		depthBufferDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
		depthBufferDesc.Height = height;
		depthBufferDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
		depthBufferDesc.MipLevels = 1;
		depthBufferDesc.SampleDesc.Count = 1;
		depthBufferDesc.SampleDesc.Quality = 0;
		depthBufferDesc.Width = width;

		// Describe the clear value that will most often be used
		// for this buffer (which optimizes the clearing of the buffer)
		D3D12_CLEAR_VALUE clear = {};
		clear.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
		clear.DepthStencil.Depth = 1.0f;
		clear.DepthStencil.Stencil = 0;

		// Describe the memory heap that will house this resource
		D3D12_HEAP_PROPERTIES props = {};
		props.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
		props.CreationNodeMask = 1;
		props.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
		props.Type = D3D12_HEAP_TYPE_DEFAULT;
		props.VisibleNodeMask = 1;

		// Actually create the resource, and the heap in which it
		// will reside, and map the resource to that heap
		device->CreateCommittedResource(
			&props,
			D3D12_HEAP_FLAG_NONE,
			&depthBufferDesc,
			D3D12_RESOURCE_STATE_DEPTH_WRITE,
			&clear,
			IID_PPV_ARGS(depthStencilBuffer.GetAddressOf()));

		// Now recreate the depth stencil view
		dsvHandle = dsvHeap->GetCPUDescriptorHandleForHeapStart();
		device->CreateDepthStencilView(
			depthStencilBuffer.Get(),
			0, // Default view (first mip)
			dsvHandle);
	}

	// Recreate the viewport and scissor rects, too,
	// since the window size has changed
	{
		// Set up the viewport so we render into the correct
		// portion of the render target
		viewport = {};
		viewport.TopLeftX = 0;
		viewport.TopLeftY = 0;
		viewport.Width = (float)width;
		viewport.Height = (float)height;
		viewport.MinDepth = 0.0f;
		viewport.MaxDepth = 1.0f;

		// Define a scissor rectangle that defines a portion of
		// the render target for clipping.  This is different from
		// a viewport in that it is applied after the pixel shader.
		// We need at least one of these, but we're rendering to 
		// the entire window, so it'll be the same size.
		scissorRect = {};
		scissorRect.left = 0;
		scissorRect.top = 0;
		scissorRect.right = width;
		scissorRect.bottom = height;
	}

	// Wait for the GPU before we proceed
	dx12Helper.WaitForGPU();
}

//// --------------------------------------------------------
//// Makes our C++ code wait for the GPU to finish its
//// current batch of work before moving on.
//// --------------------------------------------------------
//void DXCore::WaitForGPU()
//{
//	// Update our ongoing fence value (a unique index for each "stop sign")
//	// and then place that value into the GPU's command queue
//	currentFence++;
//	commandQueue->Signal(fence.Get(), currentFence);
//	// Check to see if the most recently completed fence value
//	// is less than the one we just set.
//	if (fence->GetCompletedValue() < currentFence)
//	{
//		// Tell the fence to let us know when it's hit, and then
//		// sit an wait until that fence is hit.
//		fence->SetEventOnCompletion(currentFence, fenceEvent);
//		WaitForSingleObject(fenceEvent, INFINITE);
//	}
//}

//// --------------------------------------------------------
//// Closes the current command list and tells the GPU to
//// start executing those commands.  We also wait for
//// the GPU to finish this work so we can reset the
//// command allocator (which CANNOT be reset while the
//// GPU is using its commands) and the command list itself.
//// --------------------------------------------------------
//void DXCore::CloseExecuteAndResetCommandList()
//{
//	// Close the current list and execute it as our only list
//	commandList->Close();
//	ID3D12CommandList* lists[] = { commandList.Get() };
//	commandQueue->ExecuteCommandLists(1, lists); //Set it up to be executed now.
//
//	// Always wait before reseting command allocator, as it should not
//	// be reset while the GPU is processing a command list
//	WaitForGPU();
//	commandAllocator->Reset(); //Don't reset until GPU has caught up. It'd be more desirable to have multiple allocators to be able to que up commandLists for additional frames. 
//	commandList->Reset(commandAllocator.Get(), 0); //Once allocator is rest, then reset commandList.
//}


// --------------------------------------------------------
// This is the main game loop, handling the following:
//  - OS-level messages coming in from Windows itself
//  - Calling update & draw back and forth, forever
// --------------------------------------------------------
HRESULT DXCore::Run()
{
	// Grab the start time now that
	// the game loop is running
	__int64 now;
	QueryPerformanceCounter((LARGE_INTEGER*)&now);
	startTime = now;
	currentTime = now;
	previousTime = now;

	// Give subclass a chance to initialize
	Init();

	// Our overall game and message loop
	MSG msg = {};
	while (msg.message != WM_QUIT)
	{
		// Determine if there is a message waiting
		if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
		{
			// Translate and dispatch the message
			// to our custom WindowProc function
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
		else
		{
			// Update timer and title bar (if necessary)
			UpdateTimer();
			if(titleBarStats)
				UpdateTitleBarStats();

			// Update the input manager
			Input::GetInstance().Update();

			// The game loop
			Update(deltaTime, totalTime);
			Draw(deltaTime, totalTime);

			// Frame is over, notify the input manager
			Input::GetInstance().EndOfFrame();
		}
	}

	// We'll end up here once we get a WM_QUIT message,
	// which usually comes from the user closing the window
	return (HRESULT)msg.wParam;
}


// --------------------------------------------------------
// Sends an OS-level window close message to our process, which
// will be handled by our message processing function
// --------------------------------------------------------
void DXCore::Quit()
{
	PostMessage(this->hWnd, WM_CLOSE, NULL, NULL);
}


// --------------------------------------------------------
// Uses high resolution time stamps to get very accurate
// timing information, and calculates useful time stats
// --------------------------------------------------------
void DXCore::UpdateTimer()
{
	// Grab the current time
	__int64 now;
	QueryPerformanceCounter((LARGE_INTEGER*)&now);
	currentTime = now;

	// Calculate delta time and clamp to zero
	//  - Could go negative if CPU goes into power save mode 
	//    or the process itself gets moved to another core
	deltaTime = max((float)((currentTime - previousTime) * perfCounterSeconds), 0.0f);

	// Calculate the total time from start to now
	totalTime = (float)((currentTime - startTime) * perfCounterSeconds);

	// Save current time for next frame
	previousTime = currentTime;
}


// --------------------------------------------------------
// Updates the window's title bar with several stats once
// per second, including:
//  - The window's width & height
//  - The current FPS and ms/frame
//  - The version of DirectX actually being used (usually 11)
// --------------------------------------------------------
void DXCore::UpdateTitleBarStats()
{
	fpsFrameCount++;

	// Only calc FPS and update title bar once per second
	float timeDiff = totalTime - fpsTimeElapsed;
	if (timeDiff < 1.0f)
		return;

	// How long did each frame take?  (Approx)
	float mspf = 1000.0f / (float)fpsFrameCount;

	// Quick and dirty title bar text (mostly for debugging)
	std::ostringstream output;
	output.precision(6);
	output << titleBarText <<
		"    Width: "		<< width <<
		"    Height: "		<< height <<
		"    FPS: "			<< fpsFrameCount <<
		"    Frame Time: "	<< mspf << "ms";

	// Append the version of DirectX the app is using
	switch (dxFeatureLevel)
	{
	case D3D_FEATURE_LEVEL_12_1: output << "    DX 12.1"; break;
	case D3D_FEATURE_LEVEL_12_0: output << "    DX 12.0"; break;
	case D3D_FEATURE_LEVEL_11_1: output << "    DX 11.1"; break;
	case D3D_FEATURE_LEVEL_11_0: output << "    DX 11.0"; break;
	case D3D_FEATURE_LEVEL_10_1: output << "    DX 10.1"; break;
	case D3D_FEATURE_LEVEL_10_0: output << "    DX 10.0"; break;
	case D3D_FEATURE_LEVEL_9_3:  output << "    DX 9.3";  break;
	case D3D_FEATURE_LEVEL_9_2:  output << "    DX 9.2";  break;
	case D3D_FEATURE_LEVEL_9_1:  output << "    DX 9.1";  break;
	default:                     output << "    DX ???";  break;
	}

	// Actually update the title bar and reset fps data
	SetWindowText(hWnd, output.str().c_str());
	fpsFrameCount = 0;
	fpsTimeElapsed += 1.0f;
}

// --------------------------------------------------------
// Allocates a console window we can print to for debugging
// 
// bufferLines   - Number of lines in the overall console buffer
// bufferColumns - Numbers of columns in the overall console buffer
// windowLines   - Number of lines visible at once in the window
// windowColumns - Number of columns visible at once in the window
// --------------------------------------------------------
void DXCore::CreateConsoleWindow(int bufferLines, int bufferColumns, int windowLines, int windowColumns)
{
	// Our temp console info struct
	CONSOLE_SCREEN_BUFFER_INFO coninfo;

	// Get the console info and set the number of lines
	AllocConsole();
	GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &coninfo);
	coninfo.dwSize.Y = bufferLines;
	coninfo.dwSize.X = bufferColumns;
	SetConsoleScreenBufferSize(GetStdHandle(STD_OUTPUT_HANDLE), coninfo.dwSize);

	SMALL_RECT rect;
	rect.Left = 0;
	rect.Top = 0;
	rect.Right = windowColumns;
	rect.Bottom = windowLines;
	SetConsoleWindowInfo(GetStdHandle(STD_OUTPUT_HANDLE), TRUE, &rect);

	FILE *stream;
	freopen_s(&stream, "CONIN$", "r", stdin);
	freopen_s(&stream, "CONOUT$", "w", stdout);
	freopen_s(&stream, "CONOUT$", "w", stderr);

	// Prevent accidental console window close
	HWND consoleHandle = GetConsoleWindow();
	HMENU hmenu = GetSystemMenu(consoleHandle, FALSE);
	EnableMenuItem(hmenu, SC_CLOSE, MF_GRAYED);
}

// --------------------------------------------------------------------------
// Gets the actual path to this executable
//
// - As it turns out, the relative path for a program is different when 
//    running through VS and when running the .exe directly, which makes 
//    it a pain to properly load external files (like textures)
//    - Running through VS: Current Dir is the *project folder*
//    - Running from .exe:  Current Dir is the .exe's folder
// - This has nothing to do with DEBUG and RELEASE modes - it's purely a 
//    Visual Studio "thing", and isn't obvious unless you know to look 
//    for it.  In fact, it could be fixed by changing a setting in VS, but
//    the option is stored in a user file (.suo), which is ignored by most
//    version control packages by default.  Meaning: the option must be
//    changed on every PC.  Ugh.  So instead, here's a helper.
// --------------------------------------------------------------------------
std::string DXCore::GetExePath()
{
	// Assume the path is just the "current directory" for now
	std::string path = ".\\";

	// Get the real, full path to this executable
	char currentDir[1024] = {};
	GetModuleFileName(0, currentDir, 1024);

	// Find the location of the last slash charaacter
	char* lastSlash = strrchr(currentDir, '\\');
	if (lastSlash)
	{
		// End the string at the last slash character, essentially
		// chopping off the exe's file name.  Remember, c-strings
		// are null-terminated, so putting a "zero" character in 
		// there simply denotes the end of the string.
		*lastSlash = 0;
		
		// Set the remainder as the path
		path = currentDir;
	}

	// Toss back whatever we've found
	return path;
}


// ---------------------------------------------------
//  Same as GetExePath(), except it returns a wide character
//  string, which most of the Windows API requires.
// ---------------------------------------------------
std::wstring DXCore::GetExePath_Wide()
{
	// Grab the path as a standard string
	std::string path = GetExePath();

	// Convert to a wide string
	wchar_t widePath[1024] = {};
	mbstowcs_s(0, widePath, path.c_str(), 1024);

	// Create a wstring for it and return
	return std::wstring(widePath);
}


// ----------------------------------------------------
//  Gets the full path to a given file.  NOTE: This does 
//  NOT "find" the file, it simply concatenates the given
//  relative file path onto the executable's path
// ----------------------------------------------------
std::string DXCore::GetFullPathTo(std::string relativeFilePath)
{
	return GetExePath() + "\\" + relativeFilePath;
}



// ----------------------------------------------------
//  Same as GetFullPathTo, but with wide char strings.
// 
//  Gets the full path to a given file.  NOTE: This does 
//  NOT "find" the file, it simply concatenates the given
//  relative file path onto the executable's path
// ----------------------------------------------------
std::wstring DXCore::GetFullPathTo_Wide(std::wstring relativeFilePath)
{
	return GetExePath_Wide() + L"\\" + relativeFilePath;
}





// --------------------------------------------------------
// Handles messages that are sent to our window by the
// operating system.  Ignoring these messages would cause
// our program to hang and Windows would think it was
// unresponsive.
// --------------------------------------------------------
LRESULT DXCore::ProcessMessage(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	// Check the incoming message and handle any we care about
	switch (uMsg)
	{
	// This is the message that signifies the window closing
	case WM_DESTROY:
		PostQuitMessage(0); // Send a quit message to our own program
		return 0;

	// Prevent beeping when we "alt-enter" into fullscreen
	case WM_MENUCHAR: 
		return MAKELRESULT(0, MNC_CLOSE);

	// Prevent the overall window from becoming too small
	case WM_GETMINMAXINFO:
		((MINMAXINFO*)lParam)->ptMinTrackSize.x = 200;
		((MINMAXINFO*)lParam)->ptMinTrackSize.y = 200;
		return 0;

	// Sent when the window size changes
	case WM_SIZE:
		// Don't adjust anything when minimizing,
		// since we end up with a width/height of zero
		// and that doesn't play well with the GPU
		if (wParam == SIZE_MINIMIZED)
			return 0;
		
		// Save the new client area dimensions.
		width = LOWORD(lParam);
		height = HIWORD(lParam);

		// If DX is initialized, resize 
		// our required buffers
		if (device) 
			OnResize();

		return 0;

	// Has the mouse wheel been scrolled?
	case WM_MOUSEWHEEL:
		Input::GetInstance().SetWheelDelta(GET_WHEEL_DELTA_WPARAM(wParam) / (float)WHEEL_DELTA);
		return 0;
	
	// Is our focus state changing?
	case WM_SETFOCUS:	hasFocus = true;	return 0;
	case WM_KILLFOCUS:	hasFocus = false;	return 0;
	case WM_ACTIVATE:	hasFocus = (LOWORD(wParam) != WA_INACTIVE); return 0;
	}

	// Let Windows handle any messages we're not touching
	return DefWindowProc(hWnd, uMsg, wParam, lParam);
}

