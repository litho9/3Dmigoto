// This Overlay class is to encapsulate all the on-screen drawing code,
// including creating and using the DirectXTK code.

#include "Overlay.h"

#include <DirectXColors.h>
//#include <StrSafe.h>

#include "SimpleMath.h"
#include "SpriteBatch.h"

#include "log.h"
#include "version.h"
#include "D3D11Wrapper.h"
//#include "nvapi.h"
#include "Globals.h"

#include "HackerDevice.h"
#include "HackerContext.h"



// Side note: Not really stoked with C++ string handling.  There are like 4 or
// 5 different ways to do things, all partly compatible, none a clear winner in
// terms of simplicity and clarity.  Generally speaking we'd want to use C++
// wstring and string, but there are no good output formatters.  Maybe the 
// newer iostream based pieces, but we'd still need to convert.
//
// The philosophy here and in other files, is to use whatever the API that we
// are using wants.  In this case it's a wchar_t* for DrawString, so we'll not
// do a lot of conversions and different formats, we'll just use wchar_t and its
// formatters.
//
// In particular, we also want to avoid 5 different libraries for string handling,
// Microsoft has way too many variants.  We'll use the regular C library from
// the standard c runtime, but use the _s safe versions.

// Max expected on-screen string size, used for buffer safe calls.
const int maxstring = 200;


Overlay::Overlay(HackerDevice *pDevice, HackerContext *pContext, IDXGISwapChain *pSwapChain)
{
	LogInfo("Overlay::Overlay created for %p\n", pSwapChain);
	LogInfo("  on HackerDevice: %p, HackerContext: %p\n", pDevice, pContext);

	// Drawing environment for this swap chain. This is the game environment.
	// These should specifically avoid Hacker* objects, to avoid object 
	// callbacks or other problems. We just want to draw here, nothing tricky.
	// The only exception being that we need the HackerDevice in order to 
	// draw the current stereoparams.
	mHackerDevice = pDevice;
	mOrigSwapChain = pSwapChain;
	mOrigDevice = mHackerDevice->GetOrigDevice1();
	mOrigContext = pContext->GetOrigContext1();
		
	// The courierbold.spritefont is now included as binary resource data attached
	// to the d3d11.dll.  We can fetch that resource and pass it to new SpriteFont
	// to avoid having to drag around the font file.
	HMODULE handle = GetModuleHandle(L"d3d11.dll");
	HRSRC rc = FindResource(handle, MAKEINTRESOURCE(IDR_COURIERBOLD), MAKEINTRESOURCE(SPRITEFONT));
	HGLOBAL rcData = LoadResource(handle, rc);
	DWORD fontSize = SizeofResource(handle, rc);
	uint8_t const* fontBlob = static_cast<const uint8_t*>(LockResource(rcData));

	// We want to use the original device and original context here, because
	// these will be used by DirectXTK to generate VertexShaders and PixelShaders
	// to draw the text, and we don't want to intercept those.
	mFont.reset(new DirectX::SpriteFont(mOrigDevice, fontBlob, fontSize));
	mFont->SetDefaultCharacter(L'?');
	mSpriteBatch.reset(new DirectX::SpriteBatch(mOrigContext));
}

Overlay::~Overlay()
{
}


// -----------------------------------------------------------------------------

using namespace DirectX::SimpleMath;

// Expected to be called at DXGI::Present() to be the last thing drawn.

// Notes:
	//1) Active PS location(probably x / N format)
	//2) Active VS location(x / N format)
	//3) Current convergence and separation. (convergence, a must)
	//4) Error state of reload(syntax errors go red or something)
	//5) Duplicate Mark(maybe yellow text for location, red if Decompile failed)

	//Maybe:
	//5) Other state, like show_original active.
	//6) Active toggle override.


// We need to save off everything that DirectTK will clobber and
// restore it before returning to the application. This is necessary
// to prevent rendering issues in some games like The Long Dark, and
// helps avoid introducing pipeline errors in other games like The
// Witcher 3.
// Only saving and restoring the first RenderTarget, as the only one
// we change for drawing overlay.

void Overlay::SaveState()
{
	memset(&state, 0, sizeof(state));
	
	mOrigContext->OMGetRenderTargets(1, &state.pRenderTargetView, &state.pDepthStencilView);
	state.RSNumViewPorts = D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE;
	mOrigContext->RSGetViewports(&state.RSNumViewPorts, state.pViewPorts);

	mOrigContext->OMGetBlendState(&state.pBlendState, state.BlendFactor, &state.SampleMask);
	mOrigContext->OMGetDepthStencilState(&state.pDepthStencilState, &state.StencilRef);
	mOrigContext->RSGetState(&state.pRasterizerState);
	mOrigContext->PSGetSamplers(0, 1, state.samplers);
	mOrigContext->IAGetPrimitiveTopology(&state.topology);
	mOrigContext->IAGetInputLayout(&state.pInputLayout);
	mOrigContext->VSGetShader(&state.pVertexShader, state.pVSClassInstances, &state.VSNumClassInstances);
	mOrigContext->PSGetShader(&state.pPixelShader, state.pPSClassInstances, &state.PSNumClassInstances);
	mOrigContext->IAGetVertexBuffers(0, 1, state.pVertexBuffers, state.Strides, state.Offsets);
	mOrigContext->IAGetIndexBuffer(&state.IndexBuffer, &state.Format, &state.Offset);
	mOrigContext->VSGetConstantBuffers(0, 1, state.pConstantBuffers);
	mOrigContext->PSGetShaderResources(0, 1, state.pShaderResourceViews);
}

void Overlay::RestoreState()
{
	unsigned i;

	mOrigContext->OMSetRenderTargets(1, &state.pRenderTargetView, state.pDepthStencilView);
	if (state.pRenderTargetView)
		state.pRenderTargetView->Release();
	if (state.pDepthStencilView)
		state.pDepthStencilView->Release();

	mOrigContext->RSSetViewports(state.RSNumViewPorts, state.pViewPorts);
	
	mOrigContext->OMSetBlendState(state.pBlendState, state.BlendFactor, state.SampleMask);
	if (state.pBlendState)
		state.pBlendState->Release();

	mOrigContext->OMSetDepthStencilState(state.pDepthStencilState, state.StencilRef);
	if (state.pDepthStencilState)
		state.pDepthStencilState->Release();

	mOrigContext->RSSetState(state.pRasterizerState);
	if (state.pRasterizerState)
		state.pRasterizerState->Release();

	mOrigContext->PSSetSamplers(0, 1, state.samplers);
	if (state.samplers[0])
		state.samplers[0]->Release();

	mOrigContext->IASetPrimitiveTopology(state.topology);

	mOrigContext->IASetInputLayout(state.pInputLayout);
	if (state.pInputLayout)
		state.pInputLayout->Release();

	mOrigContext->VSSetShader(state.pVertexShader, state.pVSClassInstances, state.VSNumClassInstances);
	if (state.pVertexShader)
		state.pVertexShader->Release();
	for (i = 0; i < state.VSNumClassInstances; i++)
		state.pVSClassInstances[i]->Release();

	mOrigContext->PSSetShader(state.pPixelShader, state.pPSClassInstances, state.PSNumClassInstances);
	if (state.pPixelShader)
		state.pPixelShader->Release();
	for (i = 0; i < state.PSNumClassInstances; i++)
		state.pPSClassInstances[i]->Release();

	mOrigContext->IASetVertexBuffers(0, 1, state.pVertexBuffers, state.Strides, state.Offsets);
	if (state.pVertexBuffers[0])
		state.pVertexBuffers[0]->Release();

	mOrigContext->IASetIndexBuffer(state.IndexBuffer, state.Format, state.Offset);
	if (state.IndexBuffer)
		state.IndexBuffer->Release();

	mOrigContext->VSSetConstantBuffers(0, 1, state.pConstantBuffers);
	if (state.pConstantBuffers[0])
		state.pConstantBuffers[0]->Release();

	mOrigContext->PSSetShaderResources(0, 1, state.pShaderResourceViews);
	if (state.pShaderResourceViews[0])
		state.pShaderResourceViews[0]->Release();
}

// We can't trust the game to have a proper drawing environment for DirectXTK.
//
// For two games we know of (Batman Arkham Knight and Project Cars) we were not
// getting an overlay, because apparently the rendertarget was left in an odd
// state.  This adds an init to be certain that the rendertarget is the backbuffer
// so that the overlay is drawn. 

HRESULT Overlay::InitDrawState()
{
	HRESULT hr;

	ID3D11Texture2D *pBackBuffer;
	hr = mOrigSwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (LPVOID*)&pBackBuffer);
	if (FAILED(hr))
		return hr;

	// By doing this every frame, we are always up to date with correct size,
	// and we need the address of the BackBuffer anyway, so this is low cost.
	D3D11_TEXTURE2D_DESC description;
	pBackBuffer->GetDesc(&description);
	mResolution = DirectX::XMUINT2(description.Width, description.Height);

	// use the back buffer address to create the render target
	ID3D11RenderTargetView *backbuffer;
	hr = mOrigDevice->CreateRenderTargetView(pBackBuffer, NULL, &backbuffer);

	pBackBuffer->Release();

	if (FAILED(hr))
		return hr;

	// set the first render target as the back buffer, with no stencil
	mOrigContext->OMSetRenderTargets(1, &backbuffer, NULL);

	// Holding onto a view of the back buffer can cause a crash on
	// ResizeBuffers, so it is very important we release it here - it will
	// still have a reference so long as it is bound to the pipeline -
	// i.e. until RestoreState() unbinds it. Holding onto this view caused
	// a crash in Mass Effect Andromeda when toggling full screen if the
	// hunting overlay had ever been displayed since launch.
	backbuffer->Release();

	// Make sure there is at least one open viewport for DirectXTK to use.
	D3D11_VIEWPORT openView = CD3D11_VIEWPORT(0.0, 0.0, float(mResolution.x), float(mResolution.y));
	mOrigContext->RSSetViewports(1, &openView);

	return S_OK;
}

// -----------------------------------------------------------------------------

// The active shader will show where we are in each list. / 0 / 0 will mean that we are not 
// actively searching. 

static void AppendShaderText(wchar_t *fullLine, wchar_t *type, int pos, size_t size)
{
	if (size == 0)
		return;

	// The position is zero based, so we'll make it +1 for the humans.
	if (++pos == 0)
		size = 0;

	// Format: "VS:1/15"
	wchar_t append[maxstring];
	swprintf_s(append, maxstring, L"%ls:%d/%Iu ", type, pos, size);

	wcscat_s(fullLine, maxstring, append);
}


// We also want to show the count of active vertex, pixel, compute, geometry, domain, hull
// shaders, that are active in the frame.  Any that have a zero count will be stripped, to
// keep it from being too busy looking.

static void CreateShaderCountString(wchar_t *counts)
{
	wcscpy_s(counts, maxstring, L"");
	// The order here more or less follows how important these are for
	// shaderhacking. VS and PS are the absolute most important, CS is
	// pretty important, GS and DS show up from time to time and HS is not
	// important at all since we have never needed to fix one.
	AppendShaderText(counts, L"VS", G->mSelectedVertexShaderPos, G->mVisitedVertexShaders.size());
	AppendShaderText(counts, L"PS", G->mSelectedPixelShaderPos, G->mVisitedPixelShaders.size());
	AppendShaderText(counts, L"CS", G->mSelectedComputeShaderPos, G->mVisitedComputeShaders.size());
	AppendShaderText(counts, L"GS", G->mSelectedGeometryShaderPos, G->mVisitedGeometryShaders.size());
	AppendShaderText(counts, L"DS", G->mSelectedDomainShaderPos, G->mVisitedDomainShaders.size());
	AppendShaderText(counts, L"HS", G->mSelectedHullShaderPos, G->mVisitedHullShaders.size());
	if (G->mSelectedIndexBuffer != -1)
		AppendShaderText(counts, L"IB", G->mSelectedIndexBufferPos, G->mVisitedIndexBuffers.size());
	if (G->mSelectedRenderTarget != (ID3D11Resource *)-1)
		AppendShaderText(counts, L"RT", G->mSelectedRenderTargetPos, G->mVisitedRenderTargets.size());
}


// Need to convert from the current selection, mSelectedVertexShader as hash, and
// find the OriginalShaderInfo that matches.  This is a linear search instead of a
// hash lookup, because we don't have the ID3D11DeviceChild*.

static bool FindInfoText(wchar_t *info, UINT64 selectedShader)
{
	for each (pair<ID3D11DeviceChild *, OriginalShaderInfo> loaded in G->mReloadedShaders)
	{
		if ((loaded.second.hash == selectedShader) && !loaded.second.infoText.empty())
		{
			// We now use wcsncat_s instead of wcscat_s here,
			// because the later will terminate us if the resulting
			// string would overflow the destination buffer (or
			// fail with EINVAL if we change the parameter
			// validation so it doesn't terminate us). wcsncat_s
			// has a _TRUNCATE option that tells it to fill up as
			// much of the buffer as possible without overflowing
			// and will still NULL terminate the resulting string,
			// which will work fine for this case since that will
			// be more than we can fit on the screen anyway.
			// wcsncat would also work, but its count field is
			// silly (maxstring-strlen(info)-1) and VS complains.
			//
			// Skip past first two characters, which will always be //
			wcsncat_s(info, maxstring, loaded.second.infoText.c_str() + 2, _TRUNCATE);
			return true;
		}
	}
	return false;
}


// This is for a line of text as info about the currently selected shader.
// The line is pulled out of the header of the HLSL text file, and can be
// anything. Since there can be multiple shaders selected, VS and PS and HS for
// example, we'll show one line for each, but only those that are present
// in ShaderFixes and have something other than a blank line at the top.

void Overlay::DrawShaderInfoLine(char *type, UINT64 selectedShader, int *y, bool shader)
{
	wchar_t osdString[maxstring];
	Vector2 strSize;
	Vector2 textPosition;
	float x = 0;

	if (shader) {
		if (selectedShader == -1)
			return;

		if (G->verbose_overlay)
			swprintf_s(osdString, maxstring, L"%S %016llx:", type, selectedShader);
		else
			swprintf_s(osdString, maxstring, L"%S:", type);

		if (!FindInfoText(osdString, selectedShader) && !G->verbose_overlay)
			return;
	} else {
		if (selectedShader == 0xffffffff || !G->verbose_overlay)
			return;

		swprintf_s(osdString, maxstring, L"%S %08llx", type, selectedShader);
	}

	strSize = mFont->MeasureString(osdString);

	if (!G->verbose_overlay)
		x = max(float(mResolution.x - strSize.x) / 2, 0);

	textPosition = Vector2(x, 10 + ((*y)++ * strSize.y));
	mFont->DrawString(mSpriteBatch.get(), osdString, textPosition, DirectX::Colors::LimeGreen);
}

void Overlay::DrawShaderInfoLines()
{
	int y = 1;

	// Order is the same as the pipeline... Not quite the same as the count
	// summary line, which is sorted by "the order in which we added them"
	// (which to be fair, is pretty much their order of importance for our
	// purposes). Since these only show up while hunting, it is better to
	// have them reflect the actual order that they are run in. The summary
	// line can stay in order of importance since it is always shown.
	DrawShaderInfoLine("IB", G->mSelectedIndexBuffer, &y, false);
	DrawShaderInfoLine("VS", G->mSelectedVertexShader, &y, true);
	DrawShaderInfoLine("HS", G->mSelectedHullShader, &y, true);
	DrawShaderInfoLine("DS", G->mSelectedDomainShader, &y, true);
	DrawShaderInfoLine("GS", G->mSelectedGeometryShader, &y, true);
	DrawShaderInfoLine("PS", G->mSelectedPixelShader, &y, true);
	DrawShaderInfoLine("CS", G->mSelectedComputeShader, &y, true);
	// FIXME? This one is stored as a handle, not a hash:
	if (G->mSelectedRenderTarget != (ID3D11Resource *)-1)
		DrawShaderInfoLine("RT", GetOrigResourceHash(G->mSelectedRenderTarget), &y, false);
}


// Create a string for display on the bottom edge of the screen, that contains the current
// stereo info of separation and convergence. 
// Desired format: "Sep:85  Conv:4.5"

static void CreateStereoInfoString(StereoHandle stereoHandle, wchar_t *info)
{
	// Rather than draw graphic bars, this will just be numeric.  Because
	// convergence is essentially an arbitrary number.

	float separation, convergence;
	NvU8 stereo = false;
	NvAPIOverride();
	NvAPI_Stereo_IsEnabled(&stereo);
	if (stereo)
	{
		NvAPI_Stereo_IsActivated(stereoHandle, &stereo);
		if (stereo)
		{
			NvAPI_Stereo_GetSeparation(stereoHandle, &separation);
			NvAPI_Stereo_GetConvergence(stereoHandle, &convergence);
		}
	}

	if (stereo)
		swprintf_s(info, maxstring, L"Sep:%.0f  Conv:%.1f", separation, convergence);
	else
		swprintf_s(info, maxstring, L"Stereo disabled");
}

void Overlay::DrawOverlay(void)
{
	HRESULT hr;

	// Since some games did not like having us change their drawing state from
	// SpriteBatch, we now save and restore all state information for the GPU
	// around our drawing.  
	SaveState();
	{
		hr = InitDrawState();
		if (FAILED(hr))
			goto fail_restore;

		mSpriteBatch->Begin();
		{
			wchar_t osdString[maxstring];
			Vector2 strSize;
			Vector2 textPosition;

			// Top of screen
			CreateShaderCountString(osdString);
			strSize = mFont->MeasureString(osdString);
			textPosition = Vector2(float(mResolution.x - strSize.x) / 2, 10);
			mFont->DrawString(mSpriteBatch.get(), osdString, textPosition, DirectX::Colors::LimeGreen);

			DrawShaderInfoLines();

			// Bottom of screen
			CreateStereoInfoString(mHackerDevice->mStereoHandle, osdString);
			strSize = mFont->MeasureString(osdString);
			textPosition = Vector2(float(mResolution.x - strSize.x) / 2, float(mResolution.y - strSize.y - 10));
			mFont->DrawString(mSpriteBatch.get(), osdString, textPosition, DirectX::Colors::LimeGreen);
		}
		mSpriteBatch->End();
	}
fail_restore:
	RestoreState();
}
