#include "DX12Helper.h"

#include "WICTextureLoader.h"
#include "ResourceUploadBatch.h"

using namespace DirectX;

//Singleton requirement
DX12Helper* DX12Helper::instance;

//Not much to do since we use ComPtr objects
DX12Helper::~DX12Helper(){}

void DX12Helper::Initialize(Microsoft::WRL::ComPtr<ID3D12Device> device, Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> commandList, Microsoft::WRL::ComPtr<ID3D12CommandQueue> commandQueue, Microsoft::WRL::ComPtr<ID3D12CommandAllocator> commandAllocator)
{
	// Save objects
	this->device = device;
	this->commandList = commandList;
	this->commandQueue = commandQueue;
	this->commandAllocator = commandAllocator;

	// Create the fence for basic synchronization
	device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(waitFence.GetAddressOf()));
	waitFenceEvent = CreateEventEx(0, 0, 0, EVENT_ALL_ACCESS);
	waitFenceCounter = 0;

	// Create the constant buffer upload heap
	CreateConstantBufferUploadHeap();
	CreateCBVSRVDescriptorHeap();
}

//Load a texture using DirectX12 toolkit and makes 
//non-shader visible descriptor heap with it's own 
//handle (returned to material) so it can be copied over later.

D3D12_CPU_DESCRIPTOR_HANDLE DX12Helper::LoadTexture(const wchar_t* file, bool generateMips)
{
	//Helper function from toolkit that uploads a resource to appropriate GPU memory
	ResourceUploadBatch upload(device.Get());
	upload.Begin();

	// Attempt to create the texture
	Microsoft::WRL::ComPtr<ID3D12Resource> texture;
	CreateWICTextureFromFile(device.Get(), upload, file, texture.GetAddressOf(), generateMips);

	//Make sure upload is finished before we return texture.
	auto finish = upload.End(commandQueue.Get());
	finish.wait();

	//Add texture to our list and then make a CPU-side descriptor heap for this texture's SRV
	//Possible Improvement: Find a way to reallocate all descriptors into the same heap after all SRVs are loaded.
	textures.push_back(texture);

	//Descriptor heap definition (CPU-SIDE)
	D3D12_DESCRIPTOR_HEAP_DESC dhDesc = {};
	dhDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE; //Non-shader visible for CPU-side-only descriptor heap (useful where?)
	dhDesc.NodeMask = 0;
	dhDesc.NumDescriptors = 1;
	dhDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;

	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> descHeap;
	device->CreateDescriptorHeap(&dhDesc, IID_PPV_ARGS(descHeap.GetAddressOf()));
	cpuSideTextureDescriptorHeaps.push_back(descHeap);

	// Create the SRV on this descriptor heap
	// Note: Using a null description results in the "default" SRV (same format, all mips, all array slices, etc.)
	D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle = descHeap->GetCPUDescriptorHandleForHeapStart();
	device->CreateShaderResourceView(texture.Get(), 0, cpuHandle);

	// Return the CPU descriptor handle, which can be used to
	// copy the descriptor to a shader-visible heap later
	return cpuHandle;
}


Microsoft::WRL::ComPtr<ID3D12Resource> DX12Helper::CreateStaticBuffer(unsigned int dataStride, unsigned int dataCount, void* data)
{
	// Buffer we are creating
	Microsoft::WRL::ComPtr<ID3D12Resource> buffer;

	// We are making the final heap where the resource will live
	D3D12_HEAP_PROPERTIES props = {};
	props.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
	props.CreationNodeMask = 1;
	props.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
	props.Type = D3D12_HEAP_TYPE_DEFAULT;
	props.VisibleNodeMask = 1;

	D3D12_RESOURCE_DESC desc = {};
	desc.Alignment = 0;
	desc.DepthOrArraySize = 1;
	desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	desc.Flags = D3D12_RESOURCE_FLAG_NONE;
	desc.Format = DXGI_FORMAT_UNKNOWN;
	desc.Height = 1; // Assuming this is a regular buffer, not a texture
	desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
	desc.MipLevels = 1;
	desc.SampleDesc.Count = 1;
	desc.SampleDesc.Quality = 0;
	desc.Width = dataStride * dataCount; // Size of the buffer (Size of one vertex * numVertices) or (Size of one index * numIndices)

	device->CreateCommittedResource(
		&props,
		D3D12_HEAP_FLAG_NONE,
		&desc,
		D3D12_RESOURCE_STATE_COPY_DEST, // Will eventually be "common", but copying to it first!
		0,
		IID_PPV_ARGS(buffer.GetAddressOf()));

	// Now create an intermediate upload heap for copying initial data
	D3D12_HEAP_PROPERTIES uploadProps = {};
	uploadProps.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
	uploadProps.CreationNodeMask = 1;
	uploadProps.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
	uploadProps.Type = D3D12_HEAP_TYPE_UPLOAD; // Can only ever be Generic_Read state
	uploadProps.VisibleNodeMask = 1;

	Microsoft::WRL::ComPtr<ID3D12Resource> uploadHeap;
	device->CreateCommittedResource(
		&uploadProps,
		D3D12_HEAP_FLAG_NONE,
		&desc,
		D3D12_RESOURCE_STATE_GENERIC_READ,
		0,
		IID_PPV_ARGS(uploadHeap.GetAddressOf()));

	// Do a straight map/memcpy/unmap
	void* gpuAddress = 0;
	uploadHeap->Map(0, 0, &gpuAddress);
	memcpy(gpuAddress, data, dataStride * dataCount);
	uploadHeap->Unmap(0, 0);

	// Copy the whole buffer from uploadheap to vert buffer
	commandList->CopyResource(buffer.Get(), uploadHeap.Get());

	// Transition the buffer to generic read for the rest of the app lifetime (presumable)
	// Allows us to change how our resource is being used after it is set up and good to go. 
	D3D12_RESOURCE_BARRIER rb = {};
	rb.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	rb.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
	rb.Transition.pResource = buffer.Get();
	rb.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
	rb.Transition.StateAfter = D3D12_RESOURCE_STATE_GENERIC_READ;
	rb.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
	commandList->ResourceBarrier(1, &rb);

	// Execute the command list and report success
	CloseExecuteAndResetCommandList(); //Causes us to wait again.
	return buffer;
}

//Return CBV heap for drawing.
Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> DX12Helper::GetConstantBufferDescriptorHeap()
{
	return cbvSrvDescriptorHeap;
}

//Copies data into CBV upload heap.
//Prioritizes next unused spot and then will wrap (ring buffer).
//Returns GPU descriptor handle when this operation is finished
//
// data - The current data to copy to the GPU
// dataSizeInBytes - The byte size of the data to copy
D3D12_GPU_DESCRIPTOR_HANDLE DX12Helper::FillNextConstantBufferAndGetGPUDescriptorHandle(void* data, unsigned int dataSizeInBytes)
{
	D3D12_GPU_VIRTUAL_ADDRESS virtualGPUAddress = cbUploadHeap->GetGPUVirtualAddress() + cbUploadHeapOffsetInBytes;

	//CBVs point to a memory chunk of multiple of 256
	//Calculate and reserve this amount
	SIZE_T reservationSize = (SIZE_T)dataSizeInBytes;
	reservationSize = (reservationSize + 255); //Adds 255 to drop the last few bits
	reservationSize = (reservationSize & ~255); //Flip it so it can be used to mask

	if(cbUploadHeapOffsetInBytes + reservationSize>= cbUploadHeapSizeInBytes)
	{ cbUploadHeapOffsetInBytes = 0; }

	// === Copy data to the upload heap ===
	{
		//Calculate address we mapped to buffer (actual upload address)
		//Different from virtual address for the GPU
		void* uploadAddress = reinterpret_cast<void*>((SIZE_T)cbUploadHeapStartAddress + cbUploadHeapOffsetInBytes);

		//New data put into this part of the heap
		memcpy(uploadAddress, data, dataSizeInBytes);

		//Use offset and treat upload heap like a ring buffer.
		cbUploadHeapOffsetInBytes += reservationSize;
		if (cbUploadHeapOffsetInBytes >= cbUploadHeapSizeInBytes)
			cbUploadHeapOffsetInBytes = 0;
	}

	// Create a CBV for this section of the heap
	{
		D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle = cbvSrvDescriptorHeap->GetCPUDescriptorHandleForHeapStart();
		D3D12_GPU_DESCRIPTOR_HANDLE gpuHandle = cbvSrvDescriptorHeap->GetGPUDescriptorHandleForHeapStart();

		cpuHandle.ptr += (SIZE_T)cbvDescriptorOffset * cbvSrvDescriptorHeapIncrementSize;
		gpuHandle.ptr += (SIZE_T)cbvDescriptorOffset * cbvSrvDescriptorHeapIncrementSize;
	
		D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc = {};
		cbvDesc.BufferLocation = virtualGPUAddress;
		cbvDesc.SizeInBytes = (UINT)reservationSize;

		device->CreateConstantBufferView(&cbvDesc, cpuHandle);

		cbvDescriptorOffset++;
		if (cbvDescriptorOffset >= maxConstantBuffers)
			cbvDescriptorOffset = 0; 

		return gpuHandle; 
	}
}

D3D12_GPU_DESCRIPTOR_HANDLE DX12Helper::CopySRVsToDescriptorHeapAndGetGPUDescriptorHandle(D3D12_CPU_DESCRIPTOR_HANDLE firstDescriptorToCopy, unsigned int numDescriptorsToCopy)
{
	// Grab the actual heap start on both sides and offset to the next open SRV portion
	D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle = cbvSrvDescriptorHeap->GetCPUDescriptorHandleForHeapStart();
	D3D12_GPU_DESCRIPTOR_HANDLE gpuHandle = cbvSrvDescriptorHeap->GetGPUDescriptorHandleForHeapStart();

	cpuHandle.ptr += (SIZE_T)srvDescriptorOffset * cbvSrvDescriptorHeapIncrementSize;
	gpuHandle.ptr += (SIZE_T)srvDescriptorOffset * cbvSrvDescriptorHeapIncrementSize;

	// We know where to copy these descriptors, so copy all of them and remember the new offset
	device->CopyDescriptorsSimple(numDescriptorsToCopy, cpuHandle, firstDescriptorToCopy, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	srvDescriptorOffset += numDescriptorsToCopy;

	// Pass back the GPU handle to the start of this section
	// in the final CBV/SRV heap so the caller can use it later
	return gpuHandle;
}

void DX12Helper::CloseExecuteAndResetCommandList()
{
	// Close the current list and execute it as our only list
	commandList->Close();
	ID3D12CommandList* lists[] = { commandList.Get() };
	commandQueue->ExecuteCommandLists(1, lists); //Set it up to be executed now.

	// Always wait before reseting command allocator, as it should not
	// be reset while the GPU is processing a command list
	WaitForGPU();
	commandAllocator->Reset(); //Don't reset until GPU has caught up. It'd be more desirable to have multiple allocators to be able to que up commandLists for additional frames. 
	commandList->Reset(commandAllocator.Get(), 0); //Once allocator is rest, then reset commandList.
}

void DX12Helper::WaitForGPU()
{
	//Create value for ongoing fence (index of "stop sign")
	//and pass to the GPU's command queue
	waitFenceCounter++;
	commandQueue->Signal(waitFence.Get(), waitFenceCounter);

	// Check to see if the most recently completed fence value
	// is less than the one we just set.
	if (waitFence->GetCompletedValue() < waitFenceCounter)
	{
		// Tell the fence to let us know when it's hit, and then
		// sit an wait until that fence is hit.
		waitFence->SetEventOnCompletion(waitFenceCounter, waitFenceEvent);
		WaitForSingleObject(waitFenceEvent, INFINITE);
	}
}


//Creates a single constant buffer which will store
//all the constant buffer data for the entire program. 
//Allows for re-use of memory via a ring buffer. 
void DX12Helper::CreateConstantBufferUploadHeap()
{
	//Must be a heap size of 256 bytes
	//Support up to max number of CBs or fewer if larger heaps.
	cbUploadHeapSizeInBytes = maxConstantBuffers * 256;

	//Beginning offset, will change as we use more CBs
	//and eventually wraps around 
	cbUploadHeapOffsetInBytes = 0;

	//Creating upload heap for our constant buffer
	D3D12_HEAP_PROPERTIES heapProps = {};
	heapProps.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
	heapProps.CreationNodeMask = 1;
	heapProps.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
	heapProps.Type = D3D12_HEAP_TYPE_UPLOAD; //Upload heap since we'll be copying often
	heapProps.VisibleNodeMask = 1;

	//Fill out description
	D3D12_RESOURCE_DESC resDesc = {};
	resDesc.Alignment = 0;
	resDesc.DepthOrArraySize = 1;
	resDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	resDesc.Flags = D3D12_RESOURCE_FLAG_NONE;
	resDesc.Format = DXGI_FORMAT_UNKNOWN;
	resDesc.Height = 1;
	resDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
	resDesc.MipLevels = 1;
	resDesc.SampleDesc.Count = 1;
	resDesc.SampleDesc.Quality = 0;
	resDesc.Width = cbUploadHeapSizeInBytes; // Must be 256 byte aligned!

	//Create a constant buffer resource heap
	device->CreateCommittedResource(
		&heapProps,
		D3D12_HEAP_FLAG_NONE,
		&resDesc,
		D3D12_RESOURCE_STATE_GENERIC_READ,
		0,
		IID_PPV_ARGS(cbUploadHeap.GetAddressOf()));

	// Keep mapped!
	D3D12_RANGE range{ 0, 0 };
	cbUploadHeap->Map(0, &range, &cbUploadHeapStartAddress);
}

//Create a single CBV descriptor heap which holds all 
//CBVs for the entire program and allows re-use of memory.
void DX12Helper::CreateCBVSRVDescriptorHeap()
{
	// Ask the device for the increment size for CBV descriptor heaps
	// This can vary by GPU so we need to query for it
	cbvSrvDescriptorHeapIncrementSize = (SIZE_T)device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

	// Describe the descriptor heap we want to make
	D3D12_DESCRIPTOR_HEAP_DESC dhDesc = {};
	dhDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE; // Shaders can see these!
	dhDesc.NodeMask = 0; // Node here means physical GPU - we only have 1 so its index is 0
	dhDesc.NumDescriptors = maxConstantBuffers; // How many descriptors will we need?
	dhDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV; // This heap can store CBVs, SRVs and UAVs
	device->CreateDescriptorHeap(&dhDesc, IID_PPV_ARGS(cbvSrvDescriptorHeap.GetAddressOf()));

	// Assume the first CBV will be at the beginning of the heap
	// This will increase as we use more CBVs and will wrap back to 0
	cbvDescriptorOffset = 0;

}