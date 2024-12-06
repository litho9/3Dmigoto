#pragma once

#include <d3d11_1.h>
#include <INITGUID.h>

#include "DrawCallInfo.h"

#include "CommandList.h"

#include "HackerDevice.h"
//#include "ResourceHash.h"
#include "Globals.h"

// {A3046B1E-336B-4D90-9FD6-234BC09B8687}
DEFINE_GUID(IID_HackerContext,
0xa3046b1e, 0x336b, 0x4d90, 0x9f, 0xd6, 0x23, 0x4b, 0xc0, 0x9b, 0x86, 0x87);


// Self forward reference for the factory interface.
class HackerContext;

HackerContext* HackerContextFactory(ID3D11Device1 *pDevice1, ID3D11DeviceContext1 *pContext1);

// Forward declaration to allow circular reference between HackerContext and HackerDevice. 
// We need this to allow each to reference the other as needed.

class HackerDevice;

enum class FrameAnalysisOptions;
struct ShaderOverride;


struct DrawContext
{
	float oldSeparation;
	ID3D11PixelShader *oldPixelShader;
	ID3D11VertexShader *oldVertexShader;
	CommandList *post_commands[5];
	DrawCallInfo call_info;

	DrawContext(DrawCall type,
			UINT VertexCount, UINT IndexCount, UINT InstanceCount,
			UINT FirstVertex, UINT FirstIndex, UINT FirstInstance,
			ID3D11Buffer **indirect_buffer, UINT args_offset) :
		oldSeparation(FLT_MAX),
		oldVertexShader(NULL),
		oldPixelShader(NULL),
		call_info(type, VertexCount, IndexCount, InstanceCount, FirstVertex, FirstIndex, FirstInstance,
				indirect_buffer, args_offset)
	{
		memset(post_commands, 0, sizeof(post_commands));
	}
};

struct DispatchContext
{
	CommandList *post_commands;
	DrawCallInfo call_info;

	DispatchContext(UINT ThreadGroupCountX, UINT ThreadGroupCountY, UINT ThreadGroupCountZ) :
		post_commands(NULL),
		call_info(DrawCall::Dispatch, 0, 0, 0, 0, 0, 0, NULL, 0, ThreadGroupCountX, ThreadGroupCountY, ThreadGroupCountZ)
	{}

	DispatchContext(ID3D11Buffer **indirect_buffer, UINT args_offset) :
		post_commands(NULL),
		call_info(DrawCall::DispatchIndirect, 0, 0, 0, 0, 0, 0, indirect_buffer, args_offset)
	{}
};



// These are per-context so we shouldn't need locks
struct MappedResourceInfo {
	D3D11_MAPPED_SUBRESOURCE map;
	bool mapped_writable;
	void *orig_pData;
	size_t size;

	MappedResourceInfo() :
		orig_pData(NULL),
		size(0),
		mapped_writable(false)
	{}
};


// 1-6-18:  Current approach will be to only create one level of wrapping,
// specifically HackerDevice and HackerContext, based on the ID3D11Device1,
// and ID3D11DeviceContext1.  ID3D11Device1/ID3D11DeviceContext1 is supported
// on Win7+platform_update, and thus is a superset of what we need.  By
// using the highest level object supported, we can kill off a lot of conditional
// code that just complicates things. 
//
// The ID3D11DeviceContext1 will be supported on all OS except Win7 minus the 
// platform_update.  In that scenario, we will save a reference to the 
// ID3D11DeviceContext object instead, but store it and wrap it in HackerContext.
// 
// Specifically decided to not name everything *1, because frankly that is 
// was an awful choice on Microsoft's part to begin with.  Meaningless number
// completely unrelated to version/revision or functionality.  Bad.
// We will use the *1 notation for object names that are specific types,
// like the mOrigContext1 to avoid misleading types.
//
// Any HackerDevice will be the superset object ID3D11DeviceContext1 in all cases
// except for Win7 missing the evil platform_update.

// Hierarchy:
//  HackerContext <- ID3D11DeviceContext1 <- ID3D11DeviceContext <- ID3D11DeviceChild <- IUnknown

class HackerContext : public ID3D11DeviceContext1 {
	ID3D11Device1 *mOrigDevice1;
	ID3D11DeviceContext1 *mOrigContext1;
	ID3D11DeviceContext1 *mRealOrigContext1;
	HackerDevice *mHackerDevice;

	// These are per-context, moved from globals.h:
	uint32_t mCurrentVertexBuffers[D3D11_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT];
	uint32_t mCurrentIndexBuffer; // Only valid while hunting=1
	std::vector<ID3D11Resource *> mCurrentRenderTargets;
	ID3D11Resource *mCurrentDepthTarget;
	UINT mCurrentPSUAVStartSlot;
	UINT mCurrentPSNumUAVs;

	// Used for deny_cpu_read, track_texture_updates and constant buffer matching
	typedef std::unordered_map<ID3D11Resource*, MappedResourceInfo> MappedResources;
	MappedResources mMappedResources;

	// These private methods are utility routines for HackerContext.
	void BeforeDraw(DrawContext &data);
	void AfterDraw(DrawContext &data);
	bool BeforeDispatch(DispatchContext *context);
	void AfterDispatch(DispatchContext *context);
	template <class ID3D11Shader,
		void (__stdcall ID3D11DeviceContext::*GetShaderVS2013BUGWORKAROUND)(ID3D11Shader**, ID3D11ClassInstance**, UINT*),
		void (__stdcall ID3D11DeviceContext::*SetShaderVS2013BUGWORKAROUND)(ID3D11Shader*, ID3D11ClassInstance*const*, UINT),
		HRESULT (__stdcall ID3D11Device::*CreateShader)(const void*, SIZE_T, ID3D11ClassLinkage*, ID3D11Shader**)
	>
	void DeferredShaderReplacement(ID3D11DeviceChild *shader, UINT64 hash, wchar_t *shader_type);
	void DeferredShaderReplacementBeforeDraw();
	void DeferredShaderReplacementBeforeDispatch();
	bool ExpandRegionCopy(ID3D11Resource *pDstResource, UINT DstX,
		UINT DstY, ID3D11Resource *pSrcResource, const D3D11_BOX *pSrcBox,
		UINT *replaceDstX, D3D11_BOX *replaceBox);
	bool MapDenyCPURead(ID3D11Resource *pResource, UINT Subresource,
		D3D11_MAP MapType, UINT MapFlags,
		D3D11_MAPPED_SUBRESOURCE *pMappedResource);
	void TrackAndDivertMap(HRESULT map_hr, ID3D11Resource *pResource,
		UINT Subresource, D3D11_MAP MapType, UINT MapFlags,
		D3D11_MAPPED_SUBRESOURCE *pMappedResource);
	void TrackAndDivertUnmap(ID3D11Resource *pResource, UINT Subresource);
	void ProcessShaderOverride(ShaderOverride *shaderOverride, bool isPixelShader, DrawContext *data);
	ID3D11PixelShader* SwitchPSShader(ID3D11PixelShader *shader);
	ID3D11VertexShader* SwitchVSShader(ID3D11VertexShader *shader);
	void RecordDepthStencil(ID3D11DepthStencilView *target);
	template <void (__stdcall ID3D11DeviceContext::*GetShaderResources)(THIS_
		UINT StartSlot,
		UINT NumViews,
		ID3D11ShaderResourceView **ppShaderResourceViews)>
	void RecordShaderResourceUsage(std::map<UINT64, ShaderInfoData> &ShaderInfo, UINT64 currentShader);
	void _RecordShaderResourceUsage(ShaderInfoData *shader_info,
			ID3D11ShaderResourceView *views[D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT]);
	void RecordGraphicsShaderStats();
	void RecordComputeShaderStats();
	void RecordPeerShaders(std::set<UINT64> *PeerShaders, UINT64 this_shader_hash);
	void RecordRenderTargetInfo(ID3D11RenderTargetView *target, UINT view_num);
	ID3D11Resource* RecordResourceViewStats(ID3D11View *view, std::set<uint32_t> *resource_info);

	// Templates to reduce duplicated code:
	template <class ID3D11Shader,
		 void (__stdcall ID3D11DeviceContext::*OrigSetShader)(THIS_
				 ID3D11Shader *pShader,
				 ID3D11ClassInstance *const *ppClassInstances,
				 UINT NumClassInstances)
		 >
	STDMETHODIMP_(void) SetShader(THIS_
		__in_opt ID3D11Shader *pShader,
		__in_ecount_opt(NumClassInstances) ID3D11ClassInstance *const *ppClassInstances,
		UINT NumClassInstances,
		std::set<UINT64> *visitedShaders,
		UINT64 selectedShader,
		UINT64 *currentShaderHash,
		ID3D11Shader **currentShaderHandle);
	template <void (__stdcall ID3D11DeviceContext::*OrigSetShaderResources)(THIS_
			UINT StartSlot,
			UINT NumViews,
			ID3D11ShaderResourceView *const *ppShaderResourceViews)>
	void BindStereoResources();
	template <void (__stdcall ID3D11DeviceContext::*OrigSetShaderResources)(THIS_
			UINT StartSlot,
			UINT NumViews,
			ID3D11ShaderResourceView *const *ppShaderResourceViews)>
	void SetShaderResources(UINT StartSlot, UINT NumViews, ID3D11ShaderResourceView *const *ppShaderResourceViews);

protected:
	// Allow FrameAnalysisContext access to these as an interim measure
	// until it has been further decoupled from HackerContext. Be wary of
	// relying on these - they will be zero in release mode with no
	// ShaderOverrides / ShaderRegex:
	UINT64 mCurrentVertexShader;
	UINT64 mCurrentHullShader;
	UINT64 mCurrentDomainShader;
	UINT64 mCurrentGeometryShader;
	UINT64 mCurrentPixelShader;
	UINT64 mCurrentComputeShader;

public:
	HackerContext(ID3D11Device1 *pDevice1, ID3D11DeviceContext1 *pContext1);

	void SetHackerDevice(HackerDevice *pDevice);
	HackerDevice* GetHackerDevice();
	void Bind3DMigotoResources();
	void InitIniParams();
	ID3D11DeviceContext1* GetPossiblyHookedOrigContext1();
	ID3D11DeviceContext1* GetPassThroughOrigContext1();
	void HookContext();

	// public to allow CommandList access
	virtual void FrameAnalysisLog(const char *fmt, ...) {};
	virtual void FrameAnalysisTrigger(FrameAnalysisOptions new_options) {};
	virtual void FrameAnalysisDump(ID3D11Resource *resource, FrameAnalysisOptions options,
		const wchar_t *target, DXGI_FORMAT format, UINT stride, UINT offset) {};

	// These are the shaders the game has set, which may be different from
	// the ones we have bound to the pipeline:
	ID3D11VertexShader *mCurrentVertexShaderHandle;
	ID3D11PixelShader *mCurrentPixelShaderHandle;
	ID3D11ComputeShader *mCurrentComputeShaderHandle;
	ID3D11GeometryShader *mCurrentGeometryShaderHandle;
	ID3D11DomainShader *mCurrentDomainShaderHandle;
	ID3D11HullShader *mCurrentHullShaderHandle;

	/*** IUnknown methods ***/

	HRESULT STDMETHODCALLTYPE QueryInterface(
		/* [in] */ REFIID riid,
		/* [iid_is][out] */ _COM_Outptr_ void __RPC_FAR *__RPC_FAR *ppvObject);

	ULONG STDMETHODCALLTYPE AddRef(void);

	ULONG STDMETHODCALLTYPE Release(void);


	/** ID3D11DeviceChild **/

	void STDMETHODCALLTYPE GetDevice(
		_Out_  ID3D11Device **ppDevice);

	HRESULT STDMETHODCALLTYPE GetPrivateData(
		_In_  REFGUID guid,
		_Inout_  UINT *pDataSize,
		_Out_writes_bytes_opt_(*pDataSize)  void *pData);

	HRESULT STDMETHODCALLTYPE SetPrivateData(
		_In_  REFGUID guid,
		_In_  UINT DataSize,
		_In_reads_bytes_opt_(DataSize)  const void *pData);

	HRESULT STDMETHODCALLTYPE SetPrivateDataInterface(
		_In_  REFGUID guid,
		_In_opt_  const IUnknown *pData);


	/** ID3D11DeviceContext **/

	void STDMETHODCALLTYPE VSSetConstantBuffers(
		_In_range_(0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT - 1)  UINT StartSlot,
		_In_range_(0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT - StartSlot)  UINT NumBuffers,
		_In_reads_opt_(NumBuffers)  ID3D11Buffer *const *ppConstantBuffers);

	void STDMETHODCALLTYPE PSSetShaderResources(
		_In_range_(0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT - 1)  UINT StartSlot,
		_In_range_(0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT - StartSlot)  UINT NumViews,
		_In_reads_opt_(NumViews)  ID3D11ShaderResourceView *const *ppShaderResourceViews);

	void STDMETHODCALLTYPE PSSetShader(
		_In_opt_  ID3D11PixelShader *pPixelShader,
		_In_reads_opt_(NumClassInstances)  ID3D11ClassInstance *const *ppClassInstances,
		UINT NumClassInstances);

	void STDMETHODCALLTYPE PSSetSamplers(
		_In_range_(0, D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT - 1)  UINT StartSlot,
		_In_range_(0, D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT - StartSlot)  UINT NumSamplers,
		_In_reads_opt_(NumSamplers)  ID3D11SamplerState *const *ppSamplers);

	void STDMETHODCALLTYPE VSSetShader(
		_In_opt_  ID3D11VertexShader *pVertexShader,
		_In_reads_opt_(NumClassInstances)  ID3D11ClassInstance *const *ppClassInstances,
		UINT NumClassInstances);

	void STDMETHODCALLTYPE DrawIndexed(
		_In_  UINT IndexCount,
		_In_  UINT StartIndexLocation,
		_In_  INT BaseVertexLocation);

	void STDMETHODCALLTYPE Draw(
		_In_  UINT VertexCount,
		_In_  UINT StartVertexLocation);

	HRESULT STDMETHODCALLTYPE Map(
		_In_  ID3D11Resource *pResource,
		_In_  UINT Subresource,
		_In_  D3D11_MAP MapType,
		_In_  UINT MapFlags,
		_Out_  D3D11_MAPPED_SUBRESOURCE *pMappedResource);

	void STDMETHODCALLTYPE Unmap(
		_In_  ID3D11Resource *pResource,
		_In_  UINT Subresource);

	void STDMETHODCALLTYPE PSSetConstantBuffers(
		_In_range_(0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT - 1)  UINT StartSlot,
		_In_range_(0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT - StartSlot)  UINT NumBuffers,
		_In_reads_opt_(NumBuffers)  ID3D11Buffer *const *ppConstantBuffers);

	void STDMETHODCALLTYPE IASetInputLayout(
		_In_opt_  ID3D11InputLayout *pInputLayout);

	void STDMETHODCALLTYPE IASetVertexBuffers(
		_In_range_(0, D3D11_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT - 1)  UINT StartSlot,
		_In_range_(0, D3D11_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT - StartSlot)  UINT NumBuffers,
		_In_reads_opt_(NumBuffers)  ID3D11Buffer *const *ppVertexBuffers,
		_In_reads_opt_(NumBuffers)  const UINT *pStrides,
		_In_reads_opt_(NumBuffers)  const UINT *pOffsets);

	void STDMETHODCALLTYPE IASetIndexBuffer(
		_In_opt_  ID3D11Buffer *pIndexBuffer,
		_In_  DXGI_FORMAT Format,
		_In_  UINT Offset);

	void STDMETHODCALLTYPE DrawIndexedInstanced(
		_In_  UINT IndexCountPerInstance,
		_In_  UINT InstanceCount,
		_In_  UINT StartIndexLocation,
		_In_  INT BaseVertexLocation,
		_In_  UINT StartInstanceLocation);

	void STDMETHODCALLTYPE DrawInstanced(
		_In_  UINT VertexCountPerInstance,
		_In_  UINT InstanceCount,
		_In_  UINT StartVertexLocation,
		_In_  UINT StartInstanceLocation);

	void STDMETHODCALLTYPE GSSetConstantBuffers(
		_In_range_(0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT - 1)  UINT StartSlot,
		_In_range_(0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT - StartSlot)  UINT NumBuffers,
		_In_reads_opt_(NumBuffers)  ID3D11Buffer *const *ppConstantBuffers);

	void STDMETHODCALLTYPE GSSetShader(
		_In_opt_  ID3D11GeometryShader *pShader,
		_In_reads_opt_(NumClassInstances)  ID3D11ClassInstance *const *ppClassInstances,
		UINT NumClassInstances);

	void STDMETHODCALLTYPE IASetPrimitiveTopology(
		_In_  D3D11_PRIMITIVE_TOPOLOGY Topology);

	void STDMETHODCALLTYPE VSSetShaderResources(
		_In_range_(0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT - 1)  UINT StartSlot,
		_In_range_(0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT - StartSlot)  UINT NumViews,
		_In_reads_opt_(NumViews)  ID3D11ShaderResourceView *const *ppShaderResourceViews);

	void STDMETHODCALLTYPE VSSetSamplers(
		_In_range_(0, D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT - 1)  UINT StartSlot,
		_In_range_(0, D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT - StartSlot)  UINT NumSamplers,
		_In_reads_opt_(NumSamplers)  ID3D11SamplerState *const *ppSamplers);

	void STDMETHODCALLTYPE Begin(
		_In_  ID3D11Asynchronous *pAsync);

	void STDMETHODCALLTYPE End(
		_In_  ID3D11Asynchronous *pAsync);

	HRESULT STDMETHODCALLTYPE GetData(
		_In_  ID3D11Asynchronous *pAsync,
		_Out_writes_bytes_opt_(DataSize)  void *pData,
		_In_  UINT DataSize,
		_In_  UINT GetDataFlags);

	void STDMETHODCALLTYPE SetPredication(
		_In_opt_  ID3D11Predicate *pPredicate,
		_In_  BOOL PredicateValue);

	void STDMETHODCALLTYPE GSSetShaderResources(
		_In_range_(0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT - 1)  UINT StartSlot,
		_In_range_(0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT - StartSlot)  UINT NumViews,
		_In_reads_opt_(NumViews)  ID3D11ShaderResourceView *const *ppShaderResourceViews);

	void STDMETHODCALLTYPE GSSetSamplers(
		_In_range_(0, D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT - 1)  UINT StartSlot,
		_In_range_(0, D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT - StartSlot)  UINT NumSamplers,
		_In_reads_opt_(NumSamplers)  ID3D11SamplerState *const *ppSamplers);

	void STDMETHODCALLTYPE OMSetRenderTargets(
		_In_range_(0, D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT)  UINT NumViews,
		_In_reads_opt_(NumViews)  ID3D11RenderTargetView *const *ppRenderTargetViews,
		_In_opt_  ID3D11DepthStencilView *pDepthStencilView);

	void STDMETHODCALLTYPE OMSetRenderTargetsAndUnorderedAccessViews(
		_In_  UINT NumRTVs,
		_In_reads_opt_(NumRTVs)  ID3D11RenderTargetView *const *ppRenderTargetViews,
		_In_opt_  ID3D11DepthStencilView *pDepthStencilView,
		_In_range_(0, D3D11_1_UAV_SLOT_COUNT - 1)  UINT UAVStartSlot,
		_In_  UINT NumUAVs,
		_In_reads_opt_(NumUAVs)  ID3D11UnorderedAccessView *const *ppUnorderedAccessViews,
		_In_reads_opt_(NumUAVs)  const UINT *pUAVInitialCounts);

	void STDMETHODCALLTYPE OMSetBlendState(
		_In_opt_  ID3D11BlendState *pBlendState,
		_In_opt_  const FLOAT BlendFactor[4],
		_In_  UINT SampleMask);

	void STDMETHODCALLTYPE OMSetDepthStencilState(
		_In_opt_  ID3D11DepthStencilState *pDepthStencilState,
		_In_  UINT StencilRef);

	void STDMETHODCALLTYPE SOSetTargets(
		_In_range_(0, D3D11_SO_BUFFER_SLOT_COUNT)  UINT NumBuffers,
		_In_reads_opt_(NumBuffers)  ID3D11Buffer *const *ppSOTargets,
		_In_reads_opt_(NumBuffers)  const UINT *pOffsets);

	void STDMETHODCALLTYPE DrawAuto(void);

	void STDMETHODCALLTYPE DrawIndexedInstancedIndirect(
		_In_  ID3D11Buffer *pBufferForArgs,
		_In_  UINT AlignedByteOffsetForArgs);

	void STDMETHODCALLTYPE DrawInstancedIndirect(
		_In_  ID3D11Buffer *pBufferForArgs,
		_In_  UINT AlignedByteOffsetForArgs);

	void STDMETHODCALLTYPE Dispatch(
		_In_  UINT ThreadGroupCountX,
		_In_  UINT ThreadGroupCountY,
		_In_  UINT ThreadGroupCountZ);

	void STDMETHODCALLTYPE DispatchIndirect(
		_In_  ID3D11Buffer *pBufferForArgs,
		_In_  UINT AlignedByteOffsetForArgs);

	void STDMETHODCALLTYPE RSSetState(
		_In_opt_  ID3D11RasterizerState *pRasterizerState);

	void STDMETHODCALLTYPE RSSetViewports(
		_In_range_(0, D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE)  UINT NumViewports,
		_In_reads_opt_(NumViewports)  const D3D11_VIEWPORT *pViewports);

	void STDMETHODCALLTYPE RSSetScissorRects(
		_In_range_(0, D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE)  UINT NumRects,
		_In_reads_opt_(NumRects)  const D3D11_RECT *pRects);

	void STDMETHODCALLTYPE CopySubresourceRegion(
		_In_  ID3D11Resource *pDstResource,
		_In_  UINT DstSubresource,
		_In_  UINT DstX,
		_In_  UINT DstY,
		_In_  UINT DstZ,
		_In_  ID3D11Resource *pSrcResource,
		_In_  UINT SrcSubresource,
		_In_opt_  const D3D11_BOX *pSrcBox);

	void STDMETHODCALLTYPE CopyResource(
		_In_  ID3D11Resource *pDstResource,
		_In_  ID3D11Resource *pSrcResource);

	void STDMETHODCALLTYPE UpdateSubresource(
		_In_  ID3D11Resource *pDstResource,
		_In_  UINT DstSubresource,
		_In_opt_  const D3D11_BOX *pDstBox,
		_In_  const void *pSrcData,
		_In_  UINT SrcRowPitch,
		_In_  UINT SrcDepthPitch);

	void STDMETHODCALLTYPE CopyStructureCount(
		_In_  ID3D11Buffer *pDstBuffer,
		_In_  UINT DstAlignedByteOffset,
		_In_  ID3D11UnorderedAccessView *pSrcView);

	void STDMETHODCALLTYPE ClearRenderTargetView(
		_In_  ID3D11RenderTargetView *pRenderTargetView,
		_In_  const FLOAT ColorRGBA[4]);

	void STDMETHODCALLTYPE ClearUnorderedAccessViewUint(
		_In_  ID3D11UnorderedAccessView *pUnorderedAccessView,
		_In_  const UINT Values[4]);

	void STDMETHODCALLTYPE ClearUnorderedAccessViewFloat(
		_In_  ID3D11UnorderedAccessView *pUnorderedAccessView,
		_In_  const FLOAT Values[4]);

	void STDMETHODCALLTYPE ClearDepthStencilView(
		_In_  ID3D11DepthStencilView *pDepthStencilView,
		_In_  UINT ClearFlags,
		_In_  FLOAT Depth,
		_In_  UINT8 Stencil);

	void STDMETHODCALLTYPE GenerateMips(
		_In_  ID3D11ShaderResourceView *pShaderResourceView);

	void STDMETHODCALLTYPE SetResourceMinLOD(
		_In_  ID3D11Resource *pResource,
		FLOAT MinLOD);

	FLOAT STDMETHODCALLTYPE GetResourceMinLOD(
		_In_  ID3D11Resource *pResource);

	void STDMETHODCALLTYPE ResolveSubresource(
		_In_  ID3D11Resource *pDstResource,
		_In_  UINT DstSubresource,
		_In_  ID3D11Resource *pSrcResource,
		_In_  UINT SrcSubresource,
		_In_  DXGI_FORMAT Format);

	void STDMETHODCALLTYPE ExecuteCommandList(
		_In_  ID3D11CommandList *pCommandList,
		BOOL RestoreContextState);

	void STDMETHODCALLTYPE HSSetShaderResources(
		_In_range_(0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT - 1)  UINT StartSlot,
		_In_range_(0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT - StartSlot)  UINT NumViews,
		_In_reads_opt_(NumViews)  ID3D11ShaderResourceView *const *ppShaderResourceViews);

	void STDMETHODCALLTYPE HSSetShader(
		_In_opt_  ID3D11HullShader *pHullShader,
		_In_reads_opt_(NumClassInstances)  ID3D11ClassInstance *const *ppClassInstances,
		UINT NumClassInstances);

	void STDMETHODCALLTYPE HSSetSamplers(
		_In_range_(0, D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT - 1)  UINT StartSlot,
		_In_range_(0, D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT - StartSlot)  UINT NumSamplers,
		_In_reads_opt_(NumSamplers)  ID3D11SamplerState *const *ppSamplers);

	void STDMETHODCALLTYPE HSSetConstantBuffers(
		_In_range_(0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT - 1)  UINT StartSlot,
		_In_range_(0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT - StartSlot)  UINT NumBuffers,
		_In_reads_opt_(NumBuffers)  ID3D11Buffer *const *ppConstantBuffers);

	void STDMETHODCALLTYPE DSSetShaderResources(
		_In_range_(0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT - 1)  UINT StartSlot,
		_In_range_(0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT - StartSlot)  UINT NumViews,
		_In_reads_opt_(NumViews)  ID3D11ShaderResourceView *const *ppShaderResourceViews);

	void STDMETHODCALLTYPE DSSetShader(
		_In_opt_  ID3D11DomainShader *pDomainShader,
		_In_reads_opt_(NumClassInstances)  ID3D11ClassInstance *const *ppClassInstances,
		UINT NumClassInstances);

	void STDMETHODCALLTYPE DSSetSamplers(
		_In_range_(0, D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT - 1)  UINT StartSlot,
		_In_range_(0, D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT - StartSlot)  UINT NumSamplers,
		_In_reads_opt_(NumSamplers)  ID3D11SamplerState *const *ppSamplers);

	void STDMETHODCALLTYPE DSSetConstantBuffers(
		_In_range_(0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT - 1)  UINT StartSlot,
		_In_range_(0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT - StartSlot)  UINT NumBuffers,
		_In_reads_opt_(NumBuffers)  ID3D11Buffer *const *ppConstantBuffers);

	void STDMETHODCALLTYPE CSSetShaderResources(
		_In_range_(0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT - 1)  UINT StartSlot,
		_In_range_(0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT - StartSlot)  UINT NumViews,
		_In_reads_opt_(NumViews)  ID3D11ShaderResourceView *const *ppShaderResourceViews);

	void STDMETHODCALLTYPE CSSetUnorderedAccessViews(
		_In_range_(0, D3D11_1_UAV_SLOT_COUNT - 1)  UINT StartSlot,
		_In_range_(0, D3D11_1_UAV_SLOT_COUNT - StartSlot)  UINT NumUAVs,
		_In_reads_opt_(NumUAVs)  ID3D11UnorderedAccessView *const *ppUnorderedAccessViews,
		_In_reads_opt_(NumUAVs)  const UINT *pUAVInitialCounts);

	void STDMETHODCALLTYPE CSSetShader(
		_In_opt_  ID3D11ComputeShader *pComputeShader,
		_In_reads_opt_(NumClassInstances)  ID3D11ClassInstance *const *ppClassInstances,
		UINT NumClassInstances);

	void STDMETHODCALLTYPE CSSetSamplers(
		_In_range_(0, D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT - 1)  UINT StartSlot,
		_In_range_(0, D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT - StartSlot)  UINT NumSamplers,
		_In_reads_opt_(NumSamplers)  ID3D11SamplerState *const *ppSamplers);

	void STDMETHODCALLTYPE CSSetConstantBuffers(
		_In_range_(0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT - 1)  UINT StartSlot,
		_In_range_(0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT - StartSlot)  UINT NumBuffers,
		_In_reads_opt_(NumBuffers)  ID3D11Buffer *const *ppConstantBuffers);

	void STDMETHODCALLTYPE VSGetConstantBuffers(
		_In_range_(0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT - 1)  UINT StartSlot,
		_In_range_(0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT - StartSlot)  UINT NumBuffers,
		_Out_writes_opt_(NumBuffers)  ID3D11Buffer **ppConstantBuffers);

	void STDMETHODCALLTYPE PSGetShaderResources(
		_In_range_(0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT - 1)  UINT StartSlot,
		_In_range_(0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT - StartSlot)  UINT NumViews,
		_Out_writes_opt_(NumViews)  ID3D11ShaderResourceView **ppShaderResourceViews);

	void STDMETHODCALLTYPE PSGetShader(
		_Out_  ID3D11PixelShader **ppPixelShader,
		_Out_writes_opt_(*pNumClassInstances)  ID3D11ClassInstance **ppClassInstances,
		_Inout_opt_  UINT *pNumClassInstances);

	void STDMETHODCALLTYPE PSGetSamplers(
		_In_range_(0, D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT - 1)  UINT StartSlot,
		_In_range_(0, D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT - StartSlot)  UINT NumSamplers,
		_Out_writes_opt_(NumSamplers)  ID3D11SamplerState **ppSamplers);

	void STDMETHODCALLTYPE VSGetShader(
		_Out_  ID3D11VertexShader **ppVertexShader,
		_Out_writes_opt_(*pNumClassInstances)  ID3D11ClassInstance **ppClassInstances,
		_Inout_opt_  UINT *pNumClassInstances);

	void STDMETHODCALLTYPE PSGetConstantBuffers(
		_In_range_(0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT - 1)  UINT StartSlot,
		_In_range_(0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT - StartSlot)  UINT NumBuffers,
		_Out_writes_opt_(NumBuffers)  ID3D11Buffer **ppConstantBuffers);

	void STDMETHODCALLTYPE IAGetInputLayout(
		_Out_  ID3D11InputLayout **ppInputLayout);

	void STDMETHODCALLTYPE IAGetVertexBuffers(
		_In_range_(0, D3D11_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT - 1)  UINT StartSlot,
		_In_range_(0, D3D11_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT - StartSlot)  UINT NumBuffers,
		_Out_writes_opt_(NumBuffers)  ID3D11Buffer **ppVertexBuffers,
		_Out_writes_opt_(NumBuffers)  UINT *pStrides,
		_Out_writes_opt_(NumBuffers)  UINT *pOffsets);

	void STDMETHODCALLTYPE IAGetIndexBuffer(
		_Out_opt_  ID3D11Buffer **pIndexBuffer,
		_Out_opt_  DXGI_FORMAT *Format,
		_Out_opt_  UINT *Offset);

	void STDMETHODCALLTYPE GSGetConstantBuffers(
		_In_range_(0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT - 1)  UINT StartSlot,
		_In_range_(0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT - StartSlot)  UINT NumBuffers,
		_Out_writes_opt_(NumBuffers)  ID3D11Buffer **ppConstantBuffers);

	void STDMETHODCALLTYPE GSGetShader(
		_Out_  ID3D11GeometryShader **ppGeometryShader,
		_Out_writes_opt_(*pNumClassInstances)  ID3D11ClassInstance **ppClassInstances,
		_Inout_opt_  UINT *pNumClassInstances);

	void STDMETHODCALLTYPE IAGetPrimitiveTopology(
		_Out_  D3D11_PRIMITIVE_TOPOLOGY *pTopology);

	void STDMETHODCALLTYPE VSGetShaderResources(
		_In_range_(0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT - 1)  UINT StartSlot,
		_In_range_(0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT - StartSlot)  UINT NumViews,
		_Out_writes_opt_(NumViews)  ID3D11ShaderResourceView **ppShaderResourceViews);

	void STDMETHODCALLTYPE VSGetSamplers(
		_In_range_(0, D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT - 1)  UINT StartSlot,
		_In_range_(0, D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT - StartSlot)  UINT NumSamplers,
		_Out_writes_opt_(NumSamplers)  ID3D11SamplerState **ppSamplers);

	void STDMETHODCALLTYPE GetPredication(
		_Out_opt_  ID3D11Predicate **ppPredicate,
		_Out_opt_  BOOL *pPredicateValue);

	void STDMETHODCALLTYPE GSGetShaderResources(
		_In_range_(0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT - 1)  UINT StartSlot,
		_In_range_(0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT - StartSlot)  UINT NumViews,
		_Out_writes_opt_(NumViews)  ID3D11ShaderResourceView **ppShaderResourceViews);

	void STDMETHODCALLTYPE GSGetSamplers(
		_In_range_(0, D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT - 1)  UINT StartSlot,
		_In_range_(0, D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT - StartSlot)  UINT NumSamplers,
		_Out_writes_opt_(NumSamplers)  ID3D11SamplerState **ppSamplers);

	void STDMETHODCALLTYPE OMGetRenderTargets(
		_In_range_(0, D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT)  UINT NumViews,
		_Out_writes_opt_(NumViews)  ID3D11RenderTargetView **ppRenderTargetViews,
		_Out_opt_  ID3D11DepthStencilView **ppDepthStencilView);

	void STDMETHODCALLTYPE OMGetRenderTargetsAndUnorderedAccessViews(
		_In_range_(0, D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT)  UINT NumRTVs,
		_Out_writes_opt_(NumRTVs)  ID3D11RenderTargetView **ppRenderTargetViews,
		_Out_opt_  ID3D11DepthStencilView **ppDepthStencilView,
		_In_range_(0, D3D11_PS_CS_UAV_REGISTER_COUNT - 1)  UINT UAVStartSlot,
		_In_range_(0, D3D11_PS_CS_UAV_REGISTER_COUNT - UAVStartSlot)  UINT NumUAVs,
		_Out_writes_opt_(NumUAVs)  ID3D11UnorderedAccessView **ppUnorderedAccessViews);

	void STDMETHODCALLTYPE OMGetBlendState(
		_Out_opt_  ID3D11BlendState **ppBlendState,
		_Out_opt_  FLOAT BlendFactor[4],
		_Out_opt_  UINT *pSampleMask);

	void STDMETHODCALLTYPE OMGetDepthStencilState(
		_Out_opt_  ID3D11DepthStencilState **ppDepthStencilState,
		_Out_opt_  UINT *pStencilRef);

	void STDMETHODCALLTYPE SOGetTargets(
		_In_range_(0, D3D11_SO_BUFFER_SLOT_COUNT)  UINT NumBuffers,
		_Out_writes_opt_(NumBuffers)  ID3D11Buffer **ppSOTargets);

	void STDMETHODCALLTYPE RSGetState(
		_Out_  ID3D11RasterizerState **ppRasterizerState);

	void STDMETHODCALLTYPE RSGetViewports(
		_Inout_ /*_range(0, D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE )*/   UINT *pNumViewports,
		_Out_writes_opt_(*pNumViewports)  D3D11_VIEWPORT *pViewports);

	void STDMETHODCALLTYPE RSGetScissorRects(
		_Inout_ /*_range(0, D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE )*/   UINT *pNumRects,
		_Out_writes_opt_(*pNumRects)  D3D11_RECT *pRects);

	void STDMETHODCALLTYPE HSGetShaderResources(
		_In_range_(0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT - 1)  UINT StartSlot,
		_In_range_(0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT - StartSlot)  UINT NumViews,
		_Out_writes_opt_(NumViews)  ID3D11ShaderResourceView **ppShaderResourceViews);

	void STDMETHODCALLTYPE HSGetShader(
		_Out_  ID3D11HullShader **ppHullShader,
		_Out_writes_opt_(*pNumClassInstances)  ID3D11ClassInstance **ppClassInstances,
		_Inout_opt_  UINT *pNumClassInstances);

	void STDMETHODCALLTYPE HSGetSamplers(
		_In_range_(0, D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT - 1)  UINT StartSlot,
		_In_range_(0, D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT - StartSlot)  UINT NumSamplers,
		_Out_writes_opt_(NumSamplers)  ID3D11SamplerState **ppSamplers);

	void STDMETHODCALLTYPE HSGetConstantBuffers(
		_In_range_(0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT - 1)  UINT StartSlot,
		_In_range_(0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT - StartSlot)  UINT NumBuffers,
		_Out_writes_opt_(NumBuffers)  ID3D11Buffer **ppConstantBuffers);

	void STDMETHODCALLTYPE DSGetShaderResources(
		_In_range_(0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT - 1)  UINT StartSlot,
		_In_range_(0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT - StartSlot)  UINT NumViews,
		_Out_writes_opt_(NumViews)  ID3D11ShaderResourceView **ppShaderResourceViews);

	void STDMETHODCALLTYPE DSGetShader(
		_Out_  ID3D11DomainShader **ppDomainShader,
		_Out_writes_opt_(*pNumClassInstances)  ID3D11ClassInstance **ppClassInstances,
		_Inout_opt_  UINT *pNumClassInstances);

	void STDMETHODCALLTYPE DSGetSamplers(
		_In_range_(0, D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT - 1)  UINT StartSlot,
		_In_range_(0, D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT - StartSlot)  UINT NumSamplers,
		_Out_writes_opt_(NumSamplers)  ID3D11SamplerState **ppSamplers);

	void STDMETHODCALLTYPE DSGetConstantBuffers(
		_In_range_(0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT - 1)  UINT StartSlot,
		_In_range_(0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT - StartSlot)  UINT NumBuffers,
		_Out_writes_opt_(NumBuffers)  ID3D11Buffer **ppConstantBuffers);

	void STDMETHODCALLTYPE CSGetShaderResources(
		_In_range_(0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT - 1)  UINT StartSlot,
		_In_range_(0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT - StartSlot)  UINT NumViews,
		_Out_writes_opt_(NumViews)  ID3D11ShaderResourceView **ppShaderResourceViews);

	void STDMETHODCALLTYPE CSGetUnorderedAccessViews(
		_In_range_(0, D3D11_PS_CS_UAV_REGISTER_COUNT - 1)  UINT StartSlot,
		_In_range_(0, D3D11_PS_CS_UAV_REGISTER_COUNT - StartSlot)  UINT NumUAVs,
		_Out_writes_opt_(NumUAVs)  ID3D11UnorderedAccessView **ppUnorderedAccessViews);

	void STDMETHODCALLTYPE CSGetShader(
		_Out_  ID3D11ComputeShader **ppComputeShader,
		_Out_writes_opt_(*pNumClassInstances)  ID3D11ClassInstance **ppClassInstances,
		_Inout_opt_  UINT *pNumClassInstances);

	void STDMETHODCALLTYPE CSGetSamplers(
		_In_range_(0, D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT - 1)  UINT StartSlot,
		_In_range_(0, D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT - StartSlot)  UINT NumSamplers,
		_Out_writes_opt_(NumSamplers)  ID3D11SamplerState **ppSamplers);

	void STDMETHODCALLTYPE CSGetConstantBuffers(
		_In_range_(0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT - 1)  UINT StartSlot,
		_In_range_(0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT - StartSlot)  UINT NumBuffers,
		_Out_writes_opt_(NumBuffers)  ID3D11Buffer **ppConstantBuffers);

	void STDMETHODCALLTYPE ClearState(void);

	void STDMETHODCALLTYPE Flush(void);

	D3D11_DEVICE_CONTEXT_TYPE STDMETHODCALLTYPE GetType(void);

	UINT STDMETHODCALLTYPE GetContextFlags(void);

	HRESULT STDMETHODCALLTYPE FinishCommandList(
		BOOL RestoreDeferredContextState,
		_Out_opt_  ID3D11CommandList **ppCommandList);


	/** ID3D11DeviceContext1 **/

	void STDMETHODCALLTYPE CopySubresourceRegion1(
		_In_  ID3D11Resource *pDstResource,
		_In_  UINT DstSubresource,
		_In_  UINT DstX,
		_In_  UINT DstY,
		_In_  UINT DstZ,
		_In_  ID3D11Resource *pSrcResource,
		_In_  UINT SrcSubresource,
		_In_opt_  const D3D11_BOX *pSrcBox,
		_In_  UINT CopyFlags);

	void STDMETHODCALLTYPE UpdateSubresource1(
		_In_  ID3D11Resource *pDstResource,
		_In_  UINT DstSubresource,
		_In_opt_  const D3D11_BOX *pDstBox,
		_In_  const void *pSrcData,
		_In_  UINT SrcRowPitch,
		_In_  UINT SrcDepthPitch,
		_In_  UINT CopyFlags);

	void STDMETHODCALLTYPE DiscardResource(
		_In_  ID3D11Resource *pResource);

	void STDMETHODCALLTYPE DiscardView(
		_In_  ID3D11View *pResourceView);

	void STDMETHODCALLTYPE VSSetConstantBuffers1(
		_In_range_(0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT - 1)  UINT StartSlot,
		_In_range_(0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT - StartSlot)  UINT NumBuffers,
		_In_reads_opt_(NumBuffers)  ID3D11Buffer *const *ppConstantBuffers,
		_In_reads_opt_(NumBuffers)  const UINT *pFirstConstant,
		_In_reads_opt_(NumBuffers)  const UINT *pNumConstants);

	void STDMETHODCALLTYPE HSSetConstantBuffers1(
		_In_range_(0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT - 1)  UINT StartSlot,
		_In_range_(0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT - StartSlot)  UINT NumBuffers,
		_In_reads_opt_(NumBuffers)  ID3D11Buffer *const *ppConstantBuffers,
		_In_reads_opt_(NumBuffers)  const UINT *pFirstConstant,
		_In_reads_opt_(NumBuffers)  const UINT *pNumConstants);

	void STDMETHODCALLTYPE DSSetConstantBuffers1(
		_In_range_(0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT - 1)  UINT StartSlot,
		_In_range_(0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT - StartSlot)  UINT NumBuffers,
		_In_reads_opt_(NumBuffers)  ID3D11Buffer *const *ppConstantBuffers,
		_In_reads_opt_(NumBuffers)  const UINT *pFirstConstant,
		_In_reads_opt_(NumBuffers)  const UINT *pNumConstants);

	void STDMETHODCALLTYPE GSSetConstantBuffers1(
		_In_range_(0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT - 1)  UINT StartSlot,
		_In_range_(0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT - StartSlot)  UINT NumBuffers,
		_In_reads_opt_(NumBuffers)  ID3D11Buffer *const *ppConstantBuffers,
		_In_reads_opt_(NumBuffers)  const UINT *pFirstConstant,
		_In_reads_opt_(NumBuffers)  const UINT *pNumConstants);

	void STDMETHODCALLTYPE PSSetConstantBuffers1(
		_In_range_(0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT - 1)  UINT StartSlot,
		_In_range_(0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT - StartSlot)  UINT NumBuffers,
		_In_reads_opt_(NumBuffers)  ID3D11Buffer *const *ppConstantBuffers,
		_In_reads_opt_(NumBuffers)  const UINT *pFirstConstant,
		_In_reads_opt_(NumBuffers)  const UINT *pNumConstants);

	void STDMETHODCALLTYPE CSSetConstantBuffers1(
		_In_range_(0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT - 1)  UINT StartSlot,
		_In_range_(0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT - StartSlot)  UINT NumBuffers,
		_In_reads_opt_(NumBuffers)  ID3D11Buffer *const *ppConstantBuffers,
		_In_reads_opt_(NumBuffers)  const UINT *pFirstConstant,
		_In_reads_opt_(NumBuffers)  const UINT *pNumConstants);

	void STDMETHODCALLTYPE VSGetConstantBuffers1(
		_In_range_(0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT - 1)  UINT StartSlot,
		_In_range_(0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT - StartSlot)  UINT NumBuffers,
		_Out_writes_opt_(NumBuffers)  ID3D11Buffer **ppConstantBuffers,
		_Out_writes_opt_(NumBuffers)  UINT *pFirstConstant,
		_Out_writes_opt_(NumBuffers)  UINT *pNumConstants);

	void STDMETHODCALLTYPE HSGetConstantBuffers1(
		_In_range_(0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT - 1)  UINT StartSlot,
		_In_range_(0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT - StartSlot)  UINT NumBuffers,
		_Out_writes_opt_(NumBuffers)  ID3D11Buffer **ppConstantBuffers,
		_Out_writes_opt_(NumBuffers)  UINT *pFirstConstant,
		_Out_writes_opt_(NumBuffers)  UINT *pNumConstants);

	void STDMETHODCALLTYPE DSGetConstantBuffers1(
		_In_range_(0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT - 1)  UINT StartSlot,
		_In_range_(0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT - StartSlot)  UINT NumBuffers,
		_Out_writes_opt_(NumBuffers)  ID3D11Buffer **ppConstantBuffers,
		_Out_writes_opt_(NumBuffers)  UINT *pFirstConstant,
		_Out_writes_opt_(NumBuffers)  UINT *pNumConstants);

	void STDMETHODCALLTYPE GSGetConstantBuffers1(
		_In_range_(0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT - 1)  UINT StartSlot,
		_In_range_(0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT - StartSlot)  UINT NumBuffers,
		_Out_writes_opt_(NumBuffers)  ID3D11Buffer **ppConstantBuffers,
		_Out_writes_opt_(NumBuffers)  UINT *pFirstConstant,
		_Out_writes_opt_(NumBuffers)  UINT *pNumConstants);

	void STDMETHODCALLTYPE PSGetConstantBuffers1(
		_In_range_(0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT - 1)  UINT StartSlot,
		_In_range_(0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT - StartSlot)  UINT NumBuffers,
		_Out_writes_opt_(NumBuffers)  ID3D11Buffer **ppConstantBuffers,
		_Out_writes_opt_(NumBuffers)  UINT *pFirstConstant,
		_Out_writes_opt_(NumBuffers)  UINT *pNumConstants);

	void STDMETHODCALLTYPE CSGetConstantBuffers1(
		_In_range_(0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT - 1)  UINT StartSlot,
		_In_range_(0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT - StartSlot)  UINT NumBuffers,
		_Out_writes_opt_(NumBuffers)  ID3D11Buffer **ppConstantBuffers,
		_Out_writes_opt_(NumBuffers)  UINT *pFirstConstant,
		_Out_writes_opt_(NumBuffers)  UINT *pNumConstants);

	void STDMETHODCALLTYPE SwapDeviceContextState(
		_In_  ID3DDeviceContextState *pState,
		_Out_opt_  ID3DDeviceContextState **ppPreviousState);

	void STDMETHODCALLTYPE ClearView(
		_In_  ID3D11View *pView,
		_In_  const FLOAT Color[4],
		_In_reads_opt_(NumRects)  const D3D11_RECT *pRect,
		UINT NumRects);

	void STDMETHODCALLTYPE DiscardView1(
		_In_  ID3D11View *pResourceView,
		_In_reads_opt_(NumRects)  const D3D11_RECT *pRects,
		UINT NumRects);
};
