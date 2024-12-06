#pragma once

#include <d3d11_1.h>
#include "HackerContext.h"
#include "nvapi.h"

// {2AEE5B3A-68ED-44E9-AA4D-9EAA6315D72B}
DEFINE_GUID(IID_FrameAnalysisContext,
0x2aee5b3a, 0x68ed, 0x44e9, 0xaa, 0x4d, 0x9e, 0xaa, 0x63, 0x15, 0xd7, 0x2b);

// {4EAF92BD-5552-44C7-804C-4CE1014C1B17}
DEFINE_GUID(InputLayoutDescGuid,
0x4eaf92bd, 0x5552, 0x44c7, 0x80, 0x4c, 0x4c, 0xe1, 0x1, 0x4c, 0x1b, 0x17);

struct FrameAnalysisDeferredDumpBufferArgs {
	FrameAnalysisOptions analyse_options;

	// Using a ComPtr here because the vector that this class is placed in
	// likes to make copies of this class, and I don't particularly want
	// to override the default copy and/or move constructors and operator=
	// just to properly handle the refcounting on a raw COM pointer.
	Microsoft::WRL::ComPtr<ID3D11Buffer> staging;
	Microsoft::WRL::ComPtr<ID3DBlob> layout;
	Microsoft::WRL::ComPtr<ID3D11Buffer> staged_ib_for_vb;

	D3D11_BUFFER_DESC orig_desc;
	wstring filename;
	FrameAnalysisOptions buf_type_mask;
	int idx;
	DXGI_FORMAT ib_fmt;
	UINT stride;
	UINT offset;
	UINT first;
	UINT count;
	DrawCallInfo call_info;
	UINT ib_off_for_vb;
	D3D11_PRIMITIVE_TOPOLOGY topology;

	FrameAnalysisDeferredDumpBufferArgs(FrameAnalysisOptions analyse_options, ID3D11Buffer *staging,
			D3D11_BUFFER_DESC *orig_desc, wchar_t *filename, FrameAnalysisOptions buf_type_mask, int idx,
			DXGI_FORMAT ib_fmt, UINT stride, UINT offset, UINT first, UINT count, ID3DBlob *layout,
			D3D11_PRIMITIVE_TOPOLOGY topology, DrawCallInfo *call_info,
			ID3D11Buffer *staged_ib_for_vb, UINT ib_off_for_vb) :
		analyse_options(analyse_options), staging(staging),
		orig_desc(*orig_desc), filename(filename),
		buf_type_mask(buf_type_mask), idx(idx), ib_fmt(ib_fmt),
		stride(stride), offset(offset), first(first), count(count),
		layout(layout), topology(topology),
		call_info(call_info ? *call_info : DrawCallInfo()),
		staged_ib_for_vb(staged_ib_for_vb), ib_off_for_vb(ib_off_for_vb)
	{}
};
struct FrameAnalysisDeferredDumpTex2DArgs {
	FrameAnalysisOptions analyse_options;

	// Using a ComPtr here because the vector that this class is placed in
	// likes to makes copies of this class, and I don't particularly want
	// to override the default copy and/or move constructors and operator=
	// just to properly handle the refcounting on a raw COM pointer.
	Microsoft::WRL::ComPtr<ID3D11Texture2D> staging;

	wstring filename;
	bool stereo;
	D3D11_TEXTURE2D_DESC orig_desc;
	DXGI_FORMAT format;

	FrameAnalysisDeferredDumpTex2DArgs(FrameAnalysisOptions analyse_options,
			ID3D11Texture2D *staging, wchar_t *filename,
			bool stereo, D3D11_TEXTURE2D_DESC *orig_desc, DXGI_FORMAT format) :
		analyse_options(analyse_options), staging(staging),
		filename(filename), stereo(stereo), orig_desc(*orig_desc), format(format)
	{}
};

// The deferred resource lists are unique pointers that start in the deferred
// context and are moved to the global lookup map when the command list is
// finished, then moved into the immediate context when the command list is
// executed before finally being garbage collected. They move around, but they
// are only ever have one owner at a time.
typedef vector<FrameAnalysisDeferredDumpBufferArgs> FrameAnalysisDeferredBuffers;
typedef std::unique_ptr<FrameAnalysisDeferredBuffers> FrameAnalysisDeferredBuffersPtr;
typedef vector<FrameAnalysisDeferredDumpTex2DArgs> FrameAnalysisDeferredTex2D;
typedef std::unique_ptr<FrameAnalysisDeferredTex2D> FrameAnalysisDeferredTex2DPtr;

// We make the frame analysis context directly implement ID3D11DeviceContext1 -
// no funky implementation inheritance or alternate versions here, just a
// straight forward object implementing an interface. Accessing it as
// ID3D11DeviceContext will work just as well thanks to interface inheritance.

class FrameAnalysisContext : public HackerContext {
	template <class ID3D11Shader>
	void FrameAnalysisLogShaderHash(ID3D11Shader *shader);
	void FrameAnalysisLogResourceHash(const ID3D11Resource *resource);
	void FrameAnalysisLogResource(int slot, char *slot_name, ID3D11Resource *resource);
	void FrameAnalysisLogResourceArray(UINT start, UINT len, ID3D11Resource *const *ppResources);
	void FrameAnalysisLogView(int slot, char *slot_name, ID3D11View *view);
	void FrameAnalysisLogViewArray(UINT start, UINT len, ID3D11View *const *ppViews);
	void FrameAnalysisLogMiscArray(UINT start, UINT len, void *const *array) const;
	void FrameAnalysisLogAsyncQuery(ID3D11Asynchronous *async);
	void FrameAnalysisLogData(void *buf, UINT size);
	FILE *frame_analysis_log;
	unsigned draw_call;
	unsigned non_draw_call_dump_counter;

	FrameAnalysisDeferredBuffersPtr deferred_buffers;
	FrameAnalysisDeferredTex2DPtr deferred_tex2d;

	ID3D11DeviceContext* GetDumpingContext();

	void Dump2DResource(ID3D11Texture2D *resource, wchar_t *filename,
			bool stereo, D3D11_TEXTURE2D_DESC *orig_desc, DXGI_FORMAT format);
	bool DeferDump2DResource(ID3D11Texture2D *staging, wchar_t *filename,
			bool stereo, D3D11_TEXTURE2D_DESC *orig_desc, DXGI_FORMAT format);
	void Dump2DResourceImmediateCtx(ID3D11Texture2D *staging, wstring filename,
			bool stereo, D3D11_TEXTURE2D_DESC *orig_desc, DXGI_FORMAT format);

	HRESULT ResolveMSAA(ID3D11Texture2D *src, D3D11_TEXTURE2D_DESC *srcDesc,
			ID3D11Texture2D **resolved, DXGI_FORMAT format);
	HRESULT StageResource(ID3D11Texture2D *src,
			D3D11_TEXTURE2D_DESC *srcDesc, ID3D11Texture2D **dst, DXGI_FORMAT format);
	HRESULT CreateStagingResource(ID3D11Texture2D **resource,
		D3D11_TEXTURE2D_DESC desc, bool stereo, bool msaa, DXGI_FORMAT format);

	void DumpStereoResource(ID3D11Texture2D *resource, wchar_t *filename, DXGI_FORMAT format);
	void DumpBufferTxt(wchar_t *filename, D3D11_MAPPED_SUBRESOURCE *map,
			UINT size, char type, int idx, UINT stride, UINT offset);
	void DumpVBTxt(wchar_t *filename, D3D11_MAPPED_SUBRESOURCE *map,
			UINT size, int idx, UINT stride, UINT offset,
			UINT first, UINT count, ID3DBlob *layout,
			D3D11_PRIMITIVE_TOPOLOGY topology, DrawCallInfo *call_info);
	void DumpIBTxt(wchar_t *filename, D3D11_MAPPED_SUBRESOURCE *map,
			UINT size, DXGI_FORMAT ib_fmt, UINT offset,
			UINT first, UINT count, D3D11_PRIMITIVE_TOPOLOGY topology);

	void DumpBuffer(ID3D11Buffer *buffer, wchar_t *filename,
			FrameAnalysisOptions buf_type_mask, int idx, DXGI_FORMAT ib_fmt,
			UINT stride, UINT offset, UINT first, UINT count, ID3DBlob *layout,
			D3D11_PRIMITIVE_TOPOLOGY topology, DrawCallInfo *call_info,
			ID3D11Buffer **staged_ib_ret, ID3D11Buffer *staged_ib_for_vb, UINT ib_off_for_vb);
	bool DeferDumpBuffer(ID3D11Buffer *staging,
			D3D11_BUFFER_DESC *orig_desc, wchar_t *filename,
			FrameAnalysisOptions buf_type_mask, int idx, DXGI_FORMAT ib_fmt,
			UINT stride, UINT offset, UINT first, UINT count, ID3DBlob *layout,
			D3D11_PRIMITIVE_TOPOLOGY topology, DrawCallInfo *call_info,
			ID3D11Buffer *staged_ib_for_vb, UINT ib_off_for_vb);
	void DumpBufferImmediateCtx(ID3D11Buffer *staging, D3D11_BUFFER_DESC *orig_desc,
			wstring filename, FrameAnalysisOptions buf_type_mask,
			int idx, DXGI_FORMAT ib_fmt, UINT stride, UINT offset,
			UINT first, UINT count, ID3DBlob *layout,
			D3D11_PRIMITIVE_TOPOLOGY topology, DrawCallInfo *call_info,
			ID3D11Buffer *staged_ib_for_vb, UINT ib_off_for_vb);

	void DumpResource(ID3D11Resource *resource, wchar_t *filename,
			FrameAnalysisOptions buf_type_mask, int idx, DXGI_FORMAT format,
			UINT stride, UINT offset);
	void _DumpCBs(char shader_type, bool compute,
		ID3D11Buffer *buffers[D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT]);
	void _DumpTextures(char shader_type, bool compute,
		ID3D11ShaderResourceView *views[D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT]);
	void DumpCBs(bool compute);
	void DumpVBs(DrawCallInfo *call_info, ID3D11Buffer *staged_ib, DXGI_FORMAT ib_fmt, UINT ib_off);
	void DumpIB(DrawCallInfo *call_info, ID3D11Buffer **staged_ib, DXGI_FORMAT *format, UINT *offset);
	void DumpMesh(DrawCallInfo *call_info);
	void DumpTextures(bool compute);
	void DumpRenderTargets();
	void DumpDepthStencilTargets();
	void DumpUAVs(bool compute);
	template <typename DescType>
	void DumpDesc(DescType *desc, const wchar_t *filename);

	void dump_deferred_resources(ID3D11CommandList *command_list);
	void finish_deferred_resources(ID3D11CommandList *command_list);

	HRESULT FrameAnalysisFilename(wchar_t *filename, size_t size, bool compute,
			wchar_t *reg, char shader_type, int idx, ID3D11Resource *handle);
	HRESULT FrameAnalysisFilenameResource(wchar_t *filename, size_t size, const wchar_t *type,
			ID3D11Resource *handle, bool force_filename_handle);
	const wchar_t* dedupe_tex2d_filename(ID3D11Texture2D *resource,
			const D3D11_TEXTURE2D_DESC *desc, wchar_t *dedupe_filename,
			size_t size, const wchar_t *traditional_filename, DXGI_FORMAT format);
	void dedupe_buf_filename_txt(const wchar_t *bin_filename,
			wchar_t *txt_filename, size_t size, char type, int idx,
			UINT stride, UINT offset);
	void dedupe_buf_filename_vb_txt(const wchar_t *bin_filename,
			wchar_t *txt_filename, size_t size, int idx,
			UINT stride, UINT offset, UINT first, UINT count, ID3DBlob *layout,
			D3D11_PRIMITIVE_TOPOLOGY topology, DrawCallInfo *call_info);
	void dedupe_buf_filename_ib_txt(const wchar_t *bin_filename,
			wchar_t *txt_filename, size_t size, DXGI_FORMAT ib_fmt,
			UINT offset, UINT first, UINT count, D3D11_PRIMITIVE_TOPOLOGY topology);
	void link_deduplicated_files(const wchar_t *filename, const wchar_t *dedupe_filename);
	void rotate_when_nearing_hard_link_limit(const wchar_t *dedupe_filename);
	void rotate_deduped_file(const wchar_t *dedupe_filename);
	void get_deduped_dir(wchar_t *path, size_t size);

	void determine_vb_count(UINT *count, ID3D11Buffer *staged_ib_for_vb,
			DrawCallInfo *call_info, UINT ib_off_for_vb, DXGI_FORMAT ib_fmt);

	void FrameAnalysisClearRT(ID3D11RenderTargetView *target);
	void FrameAnalysisClearUAV(ID3D11UnorderedAccessView *uav);
	void update_per_draw_analyse_options();
	void FrameAnalysisAfterDraw(bool compute, DrawCallInfo *call_info);
	void _FrameAnalysisAfterUpdate(ID3D11Resource *pResource,
			FrameAnalysisOptions type_mask, wchar_t *type);
	void FrameAnalysisAfterUnmap(ID3D11Resource *pResource);
	void FrameAnalysisAfterUpdate(ID3D11Resource *pResource);
	void update_stereo_dumping_mode();
	void set_default_dump_formats(bool draw);

	FrameAnalysisOptions analyse_options;
	FrameAnalysisOptions oneshot_analyse_options;
	bool oneshot_valid;
public:

	FrameAnalysisContext(ID3D11Device1 *pDevice, ID3D11DeviceContext1 *pContext);
	~FrameAnalysisContext();

	// public to allow CommandList access
	void FrameAnalysisLog(const char *fmt, ...) override;
	void vFrameAnalysisLog(const char *fmt, va_list ap);
	// An alias for the above function that we use to denote that omitting
	// the newline was done intentionally. For now this is just for our
	// reference, but later we might actually make the default function
	// insert a newline:
#define FrameAnalysisLogNoNL FrameAnalysisLog

	void FrameAnalysisTrigger(FrameAnalysisOptions new_options) override;
	void FrameAnalysisDump(ID3D11Resource *resource, FrameAnalysisOptions options,
		const wchar_t *target, DXGI_FORMAT format, UINT stride, UINT offset) override;

	/*** IUnknown methods ***/

	HRESULT STDMETHODCALLTYPE QueryInterface(
		/* [in] */ REFIID riid,
		/* [iid_is][out] */ _COM_Outptr_ void __RPC_FAR *__RPC_FAR *ppvObject);

	ULONG STDMETHODCALLTYPE AddRef(void);

	ULONG STDMETHODCALLTYPE Release(void);

	// ******************* ID3D11DeviceChild interface

	STDMETHOD_(void, GetDevice)(THIS_ _Out_  ID3D11Device **ppDevice);

	STDMETHOD(GetPrivateData)(THIS_
		_In_  REFGUID guid,
		__inout  UINT *pDataSize,
		SAL__out_bcount_opt(*pDataSize)  void *pData);

	STDMETHOD(SetPrivateData)(THIS_
		_In_  REFGUID guid,
		_In_  UINT DataSize,
		SAL__in_bcount_opt(DataSize)  const void *pData);

	STDMETHOD(SetPrivateDataInterface)(THIS_
		_In_  REFGUID guid,
		__in_opt  const IUnknown *pData);

	// ******************* ID3D11DeviceContext interface

	STDMETHOD_(void, VSSetConstantBuffers)(THIS_
		_In_range_(0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT - 1)  UINT StartSlot,
		_In_range_(0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT - StartSlot)  UINT NumBuffers,
		__in_ecount(NumBuffers) ID3D11Buffer *const *ppConstantBuffers);

	STDMETHOD_(void, PSSetShaderResources)(THIS_
		_In_range_(0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT - 1)  UINT StartSlot,
		_In_range_(0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT - StartSlot)  UINT NumViews,
		__in_ecount(NumViews) ID3D11ShaderResourceView *const *ppShaderResourceViews);

	STDMETHOD_(void, PSSetShader)(THIS_
		__in_opt ID3D11PixelShader *pPixelShader,
		SAL__in_ecount_opt(NumClassInstances) ID3D11ClassInstance *const *ppClassInstances,
		UINT NumClassInstances);

	STDMETHOD_(void, PSSetSamplers)(THIS_
		_In_range_(0, D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT - 1)  UINT StartSlot,
		_In_range_(0, D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT - StartSlot)  UINT NumSamplers,
		__in_ecount(NumSamplers) ID3D11SamplerState *const *ppSamplers);

	STDMETHOD_(void, VSSetShader)(THIS_
		__in_opt ID3D11VertexShader *pVertexShader,
		SAL__in_ecount_opt(NumClassInstances) ID3D11ClassInstance *const *ppClassInstances,
		UINT NumClassInstances);

	STDMETHOD_(void, DrawIndexed)(THIS_
		_In_  UINT IndexCount,
		_In_  UINT StartIndexLocation,
		_In_  INT BaseVertexLocation);

	STDMETHOD_(void, Draw)(THIS_
		_In_  UINT VertexCount,
		_In_  UINT StartVertexLocation);

	STDMETHOD(Map)(THIS_
		_In_  ID3D11Resource *pResource,
		_In_  UINT Subresource,
		_In_  D3D11_MAP MapType,
		_In_  UINT MapFlags,
		_Out_ D3D11_MAPPED_SUBRESOURCE *pMappedResource);

	STDMETHOD_(void, Unmap)(THIS_
		_In_ ID3D11Resource *pResource,
		_In_  UINT Subresource);

	STDMETHOD_(void, PSSetConstantBuffers)(THIS_
		_In_range_(0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT - 1)  UINT StartSlot,
		_In_range_(0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT - StartSlot)  UINT NumBuffers,
		__in_ecount(NumBuffers) ID3D11Buffer *const *ppConstantBuffers);

	STDMETHOD_(void, IASetInputLayout)(THIS_
		__in_opt ID3D11InputLayout *pInputLayout);

	STDMETHOD_(void, IASetVertexBuffers)(THIS_
		_In_range_(0, D3D11_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT - 1)  UINT StartSlot,
		_In_range_(0, D3D11_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT - StartSlot)  UINT NumBuffers,
		__in_ecount(NumBuffers)  ID3D11Buffer *const *ppVertexBuffers,
		__in_ecount(NumBuffers)  const UINT *pStrides,
		__in_ecount(NumBuffers)  const UINT *pOffsets);

	STDMETHOD_(void, IASetIndexBuffer)(THIS_
		__in_opt ID3D11Buffer *pIndexBuffer,
		_In_ DXGI_FORMAT Format,
		_In_  UINT Offset);

	STDMETHOD_(void, DrawIndexedInstanced)(THIS_
		_In_  UINT IndexCountPerInstance,
		_In_  UINT InstanceCount,
		_In_  UINT StartIndexLocation,
		_In_  INT BaseVertexLocation,
		_In_  UINT StartInstanceLocation);

	STDMETHOD_(void, DrawInstanced)(THIS_
		_In_  UINT VertexCountPerInstance,
		_In_  UINT InstanceCount,
		_In_  UINT StartVertexLocation,
		_In_  UINT StartInstanceLocation);

	STDMETHOD_(void, GSSetConstantBuffers)(THIS_
		_In_range_(0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT - 1)  UINT StartSlot,
		_In_range_(0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT - StartSlot)  UINT NumBuffers,
		__in_ecount(NumBuffers) ID3D11Buffer *const *ppConstantBuffers);

	STDMETHOD_(void, GSSetShader)(THIS_
		__in_opt ID3D11GeometryShader *pShader,
		SAL__in_ecount_opt(NumClassInstances) ID3D11ClassInstance *const *ppClassInstances,
		UINT NumClassInstances);

	STDMETHOD_(void, IASetPrimitiveTopology)(THIS_
		_In_ D3D11_PRIMITIVE_TOPOLOGY Topology);

	STDMETHOD_(void, VSSetShaderResources)(THIS_
		_In_range_(0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT - 1)  UINT StartSlot,
		_In_range_(0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT - StartSlot)  UINT NumViews,
		__in_ecount(NumViews) ID3D11ShaderResourceView *const *ppShaderResourceViews);

	STDMETHOD_(void, VSSetSamplers)(THIS_
		_In_range_(0, D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT - 1)  UINT StartSlot,
		_In_range_(0, D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT - StartSlot)  UINT NumSamplers,
		__in_ecount(NumSamplers) ID3D11SamplerState *const *ppSamplers);

	STDMETHOD_(void, Begin)(THIS_
		_In_  ID3D11Asynchronous *pAsync);

	STDMETHOD_(void, End)(THIS_
		_In_  ID3D11Asynchronous *pAsync);

	STDMETHOD(GetData)(THIS_
		_In_  ID3D11Asynchronous *pAsync,
		SAL__out_bcount_opt(DataSize)  void *pData,
		_In_  UINT DataSize,
		_In_  UINT GetDataFlags);

	STDMETHOD_(void, SetPredication)(THIS_
		__in_opt ID3D11Predicate *pPredicate,
		_In_  BOOL PredicateValue);

	STDMETHOD_(void, GSSetShaderResources)(THIS_
		_In_range_(0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT - 1)  UINT StartSlot,
		_In_range_(0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT - StartSlot)  UINT NumViews,
		__in_ecount(NumViews) ID3D11ShaderResourceView *const *ppShaderResourceViews);

	STDMETHOD_(void, GSSetSamplers)(THIS_
		_In_range_(0, D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT - 1)  UINT StartSlot,
		_In_range_(0, D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT - StartSlot)  UINT NumSamplers,
		__in_ecount(NumSamplers) ID3D11SamplerState *const *ppSamplers);

	STDMETHOD_(void, OMSetRenderTargets)(THIS_
		_In_range_(0, D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT)  UINT NumViews,
		SAL__in_ecount_opt(NumViews) ID3D11RenderTargetView *const *ppRenderTargetViews,
		__in_opt ID3D11DepthStencilView *pDepthStencilView);

	STDMETHOD_(void, OMSetRenderTargetsAndUnorderedAccessViews)(THIS_
		_In_  UINT NumRTVs,
		SAL__in_ecount_opt(NumRTVs) ID3D11RenderTargetView *const *ppRenderTargetViews,
		__in_opt ID3D11DepthStencilView *pDepthStencilView,
		_In_range_(0, D3D11_PS_CS_UAV_REGISTER_COUNT - 1)  UINT UAVStartSlot,
		_In_  UINT NumUAVs,
		SAL__in_ecount_opt(NumUAVs) ID3D11UnorderedAccessView *const *ppUnorderedAccessViews,
		SAL__in_ecount_opt(NumUAVs)  const UINT *pUAVInitialCounts);

	STDMETHOD_(void, OMSetBlendState)(THIS_
		__in_opt  ID3D11BlendState *pBlendState,
		__in_opt  const FLOAT BlendFactor[4],
		_In_  UINT SampleMask);

	STDMETHOD_(void, OMSetDepthStencilState)(THIS_
		__in_opt  ID3D11DepthStencilState *pDepthStencilState,
		_In_  UINT StencilRef);

	STDMETHOD_(void, SOSetTargets)(THIS_
		_In_range_(0, D3D11_SO_BUFFER_SLOT_COUNT)  UINT NumBuffers,
		SAL__in_ecount_opt(NumBuffers)  ID3D11Buffer *const *ppSOTargets,
		SAL__in_ecount_opt(NumBuffers)  const UINT *pOffsets);

	STDMETHOD_(void, DrawAuto)(THIS);

	STDMETHOD_(void, DrawIndexedInstancedIndirect)(THIS_
		_In_  ID3D11Buffer *pBufferForArgs,
		_In_  UINT AlignedByteOffsetForArgs);

	STDMETHOD_(void, DrawInstancedIndirect)(THIS_
		_In_  ID3D11Buffer *pBufferForArgs,
		_In_  UINT AlignedByteOffsetForArgs);

	STDMETHOD_(void, Dispatch)(THIS_
		_In_  UINT ThreadGroupCountX,
		_In_  UINT ThreadGroupCountY,
		_In_  UINT ThreadGroupCountZ);

	STDMETHOD_(void, DispatchIndirect)(THIS_
		_In_  ID3D11Buffer *pBufferForArgs,
		_In_  UINT AlignedByteOffsetForArgs);

	STDMETHOD_(void, RSSetState)(THIS_
		__in_opt  ID3D11RasterizerState *pRasterizerState);

	STDMETHOD_(void, RSSetViewports)(THIS_
		_In_range_(0, D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE)  UINT NumViewports,
		SAL__in_ecount_opt(NumViewports)  const D3D11_VIEWPORT *pViewports);

	STDMETHOD_(void, RSSetScissorRects)(THIS_
		_In_range_(0, D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE)  UINT NumRects,
		SAL__in_ecount_opt(NumRects)  const D3D11_RECT *pRects);

	STDMETHOD_(void, CopySubresourceRegion)(THIS_
		_In_  ID3D11Resource *pDstResource,
		_In_  UINT DstSubresource,
		_In_  UINT DstX,
		_In_  UINT DstY,
		_In_  UINT DstZ,
		_In_  ID3D11Resource *pSrcResource,
		_In_  UINT SrcSubresource,
		__in_opt  const D3D11_BOX *pSrcBox);

	STDMETHOD_(void, CopyResource)(THIS_
		_In_  ID3D11Resource *pDstResource,
		_In_  ID3D11Resource *pSrcResource);

	STDMETHOD_(void, UpdateSubresource)(THIS_
		_In_  ID3D11Resource *pDstResource,
		_In_  UINT DstSubresource,
		__in_opt  const D3D11_BOX *pDstBox,
		_In_  const void *pSrcData,
		_In_  UINT SrcRowPitch,
		_In_  UINT SrcDepthPitch);

	STDMETHOD_(void, CopyStructureCount)(THIS_
		_In_  ID3D11Buffer *pDstBuffer,
		_In_  UINT DstAlignedByteOffset,
		_In_  ID3D11UnorderedAccessView *pSrcView);

	STDMETHOD_(void, ClearRenderTargetView)(THIS_
		_In_  ID3D11RenderTargetView *pRenderTargetView,
		_In_  const FLOAT ColorRGBA[4]);

	STDMETHOD_(void, ClearUnorderedAccessViewUint)(THIS_
		_In_  ID3D11UnorderedAccessView *pUnorderedAccessView,
		_In_  const UINT Values[4]);

	STDMETHOD_(void, ClearUnorderedAccessViewFloat)(THIS_
		_In_  ID3D11UnorderedAccessView *pUnorderedAccessView,
		_In_  const FLOAT Values[4]);

	STDMETHOD_(void, ClearDepthStencilView)(THIS_
		_In_  ID3D11DepthStencilView *pDepthStencilView,
		_In_  UINT ClearFlags,
		_In_  FLOAT Depth,
		_In_  UINT8 Stencil);

	STDMETHOD_(void, GenerateMips)(THIS_
		_In_  ID3D11ShaderResourceView *pShaderResourceView);

	STDMETHOD_(void, SetResourceMinLOD)(THIS_
		_In_  ID3D11Resource *pResource,
		FLOAT MinLOD);

	STDMETHOD_(FLOAT, GetResourceMinLOD)(THIS_
		_In_  ID3D11Resource *pResource);

	STDMETHOD_(void, ResolveSubresource)(THIS_
		_In_  ID3D11Resource *pDstResource,
		_In_  UINT DstSubresource,
		_In_  ID3D11Resource *pSrcResource,
		_In_  UINT SrcSubresource,
		_In_  DXGI_FORMAT Format);

	STDMETHOD_(void, ExecuteCommandList)(THIS_
		_In_  ID3D11CommandList *pCommandList,
		BOOL RestoreContextState);

	STDMETHOD_(void, HSSetShaderResources)(THIS_
		_In_range_(0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT - 1)  UINT StartSlot,
		_In_range_(0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT - StartSlot)  UINT NumViews,
		__in_ecount(NumViews)  ID3D11ShaderResourceView *const *ppShaderResourceViews);

	STDMETHOD_(void, HSSetShader)(THIS_
		__in_opt  ID3D11HullShader *pHullShader,
		SAL__in_ecount_opt(NumClassInstances)  ID3D11ClassInstance *const *ppClassInstances,
		UINT NumClassInstances);

	STDMETHOD_(void, HSSetSamplers)(THIS_
		_In_range_(0, D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT - 1)  UINT StartSlot,
		_In_range_(0, D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT - StartSlot)  UINT NumSamplers,
		__in_ecount(NumSamplers)  ID3D11SamplerState *const *ppSamplers);

	STDMETHOD_(void, HSSetConstantBuffers)(THIS_
		_In_range_(0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT - 1)  UINT StartSlot,
		_In_range_(0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT - StartSlot)  UINT NumBuffers,
		__in_ecount(NumBuffers)  ID3D11Buffer *const *ppConstantBuffers);

	STDMETHOD_(void, DSSetShaderResources)(THIS_
		_In_range_(0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT - 1)  UINT StartSlot,
		_In_range_(0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT - StartSlot)  UINT NumViews,
		__in_ecount(NumViews)  ID3D11ShaderResourceView *const *ppShaderResourceViews);

	STDMETHOD_(void, DSSetShader)(THIS_
		__in_opt  ID3D11DomainShader *pDomainShader,
		SAL__in_ecount_opt(NumClassInstances)  ID3D11ClassInstance *const *ppClassInstances,
		UINT NumClassInstances);

	STDMETHOD_(void, DSSetSamplers)(THIS_
		_In_range_(0, D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT - 1)  UINT StartSlot,
		_In_range_(0, D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT - StartSlot)  UINT NumSamplers,
		__in_ecount(NumSamplers)  ID3D11SamplerState *const *ppSamplers);

	STDMETHOD_(void, DSSetConstantBuffers)(THIS_
		_In_range_(0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT - 1)  UINT StartSlot,
		_In_range_(0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT - StartSlot)  UINT NumBuffers,
		__in_ecount(NumBuffers)  ID3D11Buffer *const *ppConstantBuffers);

	STDMETHOD_(void, CSSetShaderResources)(THIS_
		_In_range_(0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT - 1)  UINT StartSlot,
		_In_range_(0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT - StartSlot)  UINT NumViews,
		__in_ecount(NumViews)  ID3D11ShaderResourceView *const *ppShaderResourceViews);

	STDMETHOD_(void, CSSetUnorderedAccessViews)(THIS_
		_In_range_(0, D3D11_PS_CS_UAV_REGISTER_COUNT - 1)  UINT StartSlot,
		_In_range_(0, D3D11_PS_CS_UAV_REGISTER_COUNT - StartSlot)  UINT NumUAVs,
		__in_ecount(NumUAVs)  ID3D11UnorderedAccessView *const *ppUnorderedAccessViews,
		__in_ecount(NumUAVs)  const UINT *pUAVInitialCounts);

	STDMETHOD_(void, CSSetShader)(THIS_
		__in_opt  ID3D11ComputeShader *pComputeShader,
		SAL__in_ecount_opt(NumClassInstances)  ID3D11ClassInstance *const *ppClassInstances,
		UINT NumClassInstances);

	STDMETHOD_(void, CSSetSamplers)(THIS_
		_In_range_(0, D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT - 1)  UINT StartSlot,
		_In_range_(0, D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT - StartSlot)  UINT NumSamplers,
		__in_ecount(NumSamplers)  ID3D11SamplerState *const *ppSamplers);

	STDMETHOD_(void, CSSetConstantBuffers)(THIS_
		_In_range_(0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT - 1)  UINT StartSlot,
		_In_range_(0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT - StartSlot)  UINT NumBuffers,
		__in_ecount(NumBuffers)  ID3D11Buffer *const *ppConstantBuffers);

	STDMETHOD_(void, VSGetConstantBuffers)(THIS_
		_In_range_(0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT - 1)  UINT StartSlot,
		_In_range_(0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT - StartSlot)  UINT NumBuffers,
		__out_ecount(NumBuffers)  ID3D11Buffer **ppConstantBuffers);

	STDMETHOD_(void, PSGetShaderResources)(THIS_
		_In_range_(0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT - 1)  UINT StartSlot,
		_In_range_(0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT - StartSlot)  UINT NumViews,
		__out_ecount(NumViews)  ID3D11ShaderResourceView **ppShaderResourceViews);

	STDMETHOD_(void, PSGetShader)(THIS_
		_Out_  ID3D11PixelShader **ppPixelShader,
		SAL__out_ecount_opt(*pNumClassInstances)  ID3D11ClassInstance **ppClassInstances,
		__inout_opt  UINT *pNumClassInstances);

	STDMETHOD_(void, PSGetSamplers)(THIS_
		_In_range_(0, D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT - 1)  UINT StartSlot,
		_In_range_(0, D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT - StartSlot)  UINT NumSamplers,
		__out_ecount(NumSamplers)  ID3D11SamplerState **ppSamplers);

	STDMETHOD_(void, VSGetShader)(THIS_
		_Out_  ID3D11VertexShader **ppVertexShader,
		SAL__out_ecount_opt(*pNumClassInstances)  ID3D11ClassInstance **ppClassInstances,
		__inout_opt  UINT *pNumClassInstances);

	STDMETHOD_(void, PSGetConstantBuffers)(THIS_
		_In_range_(0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT - 1)  UINT StartSlot,
		_In_range_(0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT - StartSlot)  UINT NumBuffers,
		__out_ecount(NumBuffers)  ID3D11Buffer **ppConstantBuffers);

	STDMETHOD_(void, IAGetInputLayout)(THIS_
		_Out_  ID3D11InputLayout **ppInputLayout);

	STDMETHOD_(void, IAGetVertexBuffers)(THIS_
		_In_range_(0, D3D11_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT - 1)  UINT StartSlot,
		_In_range_(0, D3D11_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT - StartSlot)  UINT NumBuffers,
		SAL__out_ecount_opt(NumBuffers)  ID3D11Buffer **ppVertexBuffers,
		SAL__out_ecount_opt(NumBuffers)  UINT *pStrides,
		SAL__out_ecount_opt(NumBuffers)  UINT *pOffsets);

	STDMETHOD_(void, IAGetIndexBuffer)(THIS_
		__out_opt  ID3D11Buffer **pIndexBuffer,
		__out_opt  DXGI_FORMAT *Format,
		__out_opt  UINT *Offset);

	STDMETHOD_(void, GSGetConstantBuffers)(THIS_
		_In_range_(0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT - 1)  UINT StartSlot,
		_In_range_(0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT - StartSlot)  UINT NumBuffers,
		__out_ecount(NumBuffers)  ID3D11Buffer **ppConstantBuffers);

	STDMETHOD_(void, GSGetShader)(THIS_
		_Out_  ID3D11GeometryShader **ppGeometryShader,
		SAL__out_ecount_opt(*pNumClassInstances)  ID3D11ClassInstance **ppClassInstances,
		__inout_opt  UINT *pNumClassInstances);

	STDMETHOD_(void, IAGetPrimitiveTopology)(THIS_
		_Out_  D3D11_PRIMITIVE_TOPOLOGY *pTopology);

	STDMETHOD_(void, VSGetShaderResources)(THIS_
		_In_range_(0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT - 1)  UINT StartSlot,
		_In_range_(0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT - StartSlot)  UINT NumViews,
		__out_ecount(NumViews)  ID3D11ShaderResourceView **ppShaderResourceViews);

	STDMETHOD_(void, VSGetSamplers)(THIS_
		_In_range_(0, D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT - 1)  UINT StartSlot,
		_In_range_(0, D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT - StartSlot)  UINT NumSamplers,
		__out_ecount(NumSamplers)  ID3D11SamplerState **ppSamplers);

	STDMETHOD_(void, GetPredication)(THIS_
		__out_opt  ID3D11Predicate **ppPredicate,
		__out_opt  BOOL *pPredicateValue);

	STDMETHOD_(void, GSGetShaderResources)(THIS_
		_In_range_(0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT - 1)  UINT StartSlot,
		_In_range_(0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT - StartSlot)  UINT NumViews,
		__out_ecount(NumViews)  ID3D11ShaderResourceView **ppShaderResourceViews);

	STDMETHOD_(void, GSGetSamplers)(THIS_
		_In_range_(0, D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT - 1)  UINT StartSlot,
		_In_range_(0, D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT - StartSlot)  UINT NumSamplers,
		__out_ecount(NumSamplers)  ID3D11SamplerState **ppSamplers);

	STDMETHOD_(void, OMGetRenderTargets)(THIS_
		_In_range_(0, D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT)  UINT NumViews,
		SAL__out_ecount_opt(NumViews)  ID3D11RenderTargetView **ppRenderTargetViews,
		__out_opt  ID3D11DepthStencilView **ppDepthStencilView);

	STDMETHOD_(void, OMGetRenderTargetsAndUnorderedAccessViews)(THIS_
		_In_range_(0, D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT)  UINT NumRTVs,
		SAL__out_ecount_opt(NumRTVs)  ID3D11RenderTargetView **ppRenderTargetViews,
		__out_opt  ID3D11DepthStencilView **ppDepthStencilView,
		_In_range_(0, D3D11_PS_CS_UAV_REGISTER_COUNT - 1)  UINT UAVStartSlot,
		_In_range_(0, D3D11_PS_CS_UAV_REGISTER_COUNT - UAVStartSlot)  UINT NumUAVs,
		SAL__out_ecount_opt(NumUAVs)  ID3D11UnorderedAccessView **ppUnorderedAccessViews);

	STDMETHOD_(void, OMGetBlendState)(THIS_
		__out_opt  ID3D11BlendState **ppBlendState,
		__out_opt  FLOAT BlendFactor[4],
		__out_opt  UINT *pSampleMask);

	STDMETHOD_(void, OMGetDepthStencilState)(THIS_
		__out_opt  ID3D11DepthStencilState **ppDepthStencilState,
		__out_opt  UINT *pStencilRef);

	STDMETHOD_(void, SOGetTargets)(THIS_
		_In_range_(0, D3D11_SO_BUFFER_SLOT_COUNT)  UINT NumBuffers,
		__out_ecount(NumBuffers)  ID3D11Buffer **ppSOTargets);

	STDMETHOD_(void, RSGetState)(THIS_
		_Out_  ID3D11RasterizerState **ppRasterizerState);

	STDMETHOD_(void, RSGetViewports)(THIS_
		__inout /*_range(0, D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE )*/   UINT *pNumViewports,
		SAL__out_ecount_opt(*pNumViewports)  D3D11_VIEWPORT *pViewports);

	STDMETHOD_(void, RSGetScissorRects)(THIS_
		__inout /*_range(0, D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE )*/   UINT *pNumRects,
		SAL__out_ecount_opt(*pNumRects)  D3D11_RECT *pRects);

	STDMETHOD_(void, HSGetShaderResources)(THIS_
		_In_range_(0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT - 1)  UINT StartSlot,
		_In_range_(0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT - StartSlot)  UINT NumViews,
		__out_ecount(NumViews)  ID3D11ShaderResourceView **ppShaderResourceViews);

	STDMETHOD_(void, HSGetShader)(THIS_
		_Out_  ID3D11HullShader **ppHullShader,
		SAL__out_ecount_opt(*pNumClassInstances)  ID3D11ClassInstance **ppClassInstances,
		__inout_opt  UINT *pNumClassInstances);

	STDMETHOD_(void, HSGetSamplers)(THIS_
		_In_range_(0, D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT - 1)  UINT StartSlot,
		_In_range_(0, D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT - StartSlot)  UINT NumSamplers,
		__out_ecount(NumSamplers)  ID3D11SamplerState **ppSamplers);

	STDMETHOD_(void, HSGetConstantBuffers)(THIS_
		_In_range_(0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT - 1)  UINT StartSlot,
		_In_range_(0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT - StartSlot)  UINT NumBuffers,
		__out_ecount(NumBuffers)  ID3D11Buffer **ppConstantBuffers);

	STDMETHOD_(void, DSGetShaderResources)(THIS_
		_In_range_(0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT - 1)  UINT StartSlot,
		_In_range_(0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT - StartSlot)  UINT NumViews,
		__out_ecount(NumViews)  ID3D11ShaderResourceView **ppShaderResourceViews);

	STDMETHOD_(void, DSGetShader)(THIS_
		_Out_  ID3D11DomainShader **ppDomainShader,
		SAL__out_ecount_opt(*pNumClassInstances)  ID3D11ClassInstance **ppClassInstances,
		__inout_opt  UINT *pNumClassInstances);

	STDMETHOD_(void, DSGetSamplers)(THIS_
		_In_range_(0, D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT - 1)  UINT StartSlot,
		_In_range_(0, D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT - StartSlot)  UINT NumSamplers,
		__out_ecount(NumSamplers)  ID3D11SamplerState **ppSamplers);

	STDMETHOD_(void, DSGetConstantBuffers)(THIS_
		_In_range_(0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT - 1)  UINT StartSlot,
		_In_range_(0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT - StartSlot)  UINT NumBuffers,
		__out_ecount(NumBuffers)  ID3D11Buffer **ppConstantBuffers);

	STDMETHOD_(void, CSGetShaderResources)(THIS_
		_In_range_(0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT - 1)  UINT StartSlot,
		_In_range_(0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT - StartSlot)  UINT NumViews,
		__out_ecount(NumViews)  ID3D11ShaderResourceView **ppShaderResourceViews);

	STDMETHOD_(void, CSGetUnorderedAccessViews)(THIS_
		_In_range_(0, D3D11_PS_CS_UAV_REGISTER_COUNT - 1)  UINT StartSlot,
		_In_range_(0, D3D11_PS_CS_UAV_REGISTER_COUNT - StartSlot)  UINT NumUAVs,
		__out_ecount(NumUAVs)  ID3D11UnorderedAccessView **ppUnorderedAccessViews);

	STDMETHOD_(void, CSGetShader)(THIS_
		_Out_  ID3D11ComputeShader **ppComputeShader,
		SAL__out_ecount_opt(*pNumClassInstances)  ID3D11ClassInstance **ppClassInstances,
		__inout_opt  UINT *pNumClassInstances);

	STDMETHOD_(void, CSGetSamplers)(THIS_
		_In_range_(0, D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT - 1)  UINT StartSlot,
		_In_range_(0, D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT - StartSlot)  UINT NumSamplers,
		__out_ecount(NumSamplers)  ID3D11SamplerState **ppSamplers);

	STDMETHOD_(void, CSGetConstantBuffers)(THIS_
		_In_range_(0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT - 1)  UINT StartSlot,
		_In_range_(0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT - StartSlot)  UINT NumBuffers,
		__out_ecount(NumBuffers)  ID3D11Buffer **ppConstantBuffers);

	STDMETHOD_(void, ClearState)(THIS);

	STDMETHOD_(void, Flush)(THIS);

	STDMETHOD_(D3D11_DEVICE_CONTEXT_TYPE, GetType)(THIS);

	STDMETHOD_(UINT, GetContextFlags)(THIS);

	STDMETHOD(FinishCommandList)(THIS_
		BOOL RestoreDeferredContextState,
		__out_opt  ID3D11CommandList **ppCommandList);

	// ******************* ID3D11DeviceContext1 interface

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
