#ifndef PASSTHROUGHAPP_PROTOCOLIMPL_INL
#define PASSTHROUGHAPP_PROTOCOLIMPL_INL

#if _MSC_VER > 1000
	#pragma once
#endif // _MSC_VER > 1000

#ifndef PASSTHROUGHAPP_PROTOCOLIMPL_H
	#error ProtocolImpl.inl requires ProtocolImpl.h to be included first
#endif

namespace PassthroughAPP
{

namespace Detail
{

template <class T>
inline HRESULT WINAPI QIPassthrough<T>::
	QueryInterfacePassthroughT(void* pv, REFIID riid, LPVOID* ppv, DWORD_PTR dw)
{
	ATLASSERT(pv != 0);
	T* pT = static_cast<T*>(pv);

	IUnknown* punkTarget = pT->GetTargetUnknown();
	ATLASSERT(punkTarget != 0);
	if (!punkTarget)
	{
		ATLTRACE(_T("Interface queried before target unknown is set"));
		return E_UNEXPECTED;
	}

	IUnknown* punkWrapper = pT->GetUnknown();

	typename T::ObjectLock lock(pT);
	return QueryInterfacePassthrough(
		pv, riid, ppv, dw, punkTarget, punkWrapper);
}

template <class T>
inline HRESULT WINAPI QIPassthrough<T>::
	QueryInterfaceDebugT(void* pv, REFIID riid, LPVOID* ppv, DWORD_PTR dw)
{
	ATLASSERT(pv != 0);
	T* pT = static_cast<T*>(pv);

	IUnknown* punkTarget = pT->GetTargetUnknown();
	ATLASSERT(punkTarget != 0);
	if (!punkTarget)
	{
		ATLTRACE(_T("Interface queried before target unknown is set"));
		return E_UNEXPECTED;
	}

	typename T::ObjectLock lock(pT);
	return QueryInterfaceDebug(pv, riid, ppv, dw, punkTarget);
}

inline HRESULT WINAPI QueryInterfacePassthrough(void* pv, REFIID riid,
	LPVOID* ppv, DWORD_PTR dw, IUnknown* punkTarget, IUnknown* punkWrapper)
{
	ATLASSERT(pv != 0);
	ATLASSERT(ppv != 0);
	ATLASSERT(dw != 0);
	ATLASSERT(punkTarget != 0);

	const PassthroughItfData& data =
		*reinterpret_cast<const PassthroughItfData*>(dw);

	IUnknown** ppUnk = reinterpret_cast<IUnknown**>(
		static_cast<char*>(pv) + data.offsetUnk);

	HRESULT hr = S_OK;
	if (!*ppUnk)
	{
		CComPtr<IUnknown> spUnk;
		hr = punkTarget->QueryInterface(riid,
			reinterpret_cast<void**>(&spUnk));
		ATLASSERT(FAILED(hr) || spUnk != 0);
		if (SUCCEEDED(hr))
		{
			*ppUnk = spUnk.Detach();

			// Need to QI for base interface to fill in base target pointer
			if (data.piidBase)
			{
				ATLASSERT(punkWrapper != 0);
				hr = punkWrapper->QueryInterface(*data.piidBase,
					reinterpret_cast<void**>(&spUnk));
				// since QI for derived interface succeeded,
				// QI for base interface must succeed, too
				ATLASSERT(SUCCEEDED(hr));
			}
		}
	}
	if (SUCCEEDED(hr))
	{
		CComPtr<IUnknown> spItf = reinterpret_cast<IUnknown*>(
			static_cast<char*>(pv) + data.offsetItf);
		*ppv = spItf.Detach();
	}
	else
	{
		ATLASSERT(_T("Interface not supported by target unknown"));
	}
	return hr;
}

inline HRESULT WINAPI QueryInterfaceDebug(void* pv, REFIID riid,
	LPVOID* ppv, DWORD_PTR dw, IUnknown* punkTarget)
{
	ATLASSERT(pv != 0);
	ATLASSERT(ppv != 0);
	ATLASSERT(punkTarget != 0);

	CComPtr<IUnknown> spUnk;
	HRESULT hr = punkTarget->QueryInterface(riid,
		reinterpret_cast<void**>(&spUnk));
	ATLASSERT(FAILED(hr) || spUnk != 0);
	if (SUCCEEDED(hr))
	{
		ATLTRACE(_T("Unrecognized interface supported by target unknown"));
		ATLASSERT(false);
	}

	// We don't support this interface, so return an error.
	// The operations above are for debugging purposes only,
	// this function is not supposed to ever return success
	return E_NOINTERFACE;
}

inline HRESULT QueryServicePassthrough(REFGUID guidService,
	IUnknown* punkThis, REFIID riid, void** ppv,
	IServiceProvider* pClientProvider)
{
	ATLASSERT(punkThis != 0);
	CComPtr<IUnknown> spDummy;
	HRESULT hr = pClientProvider ?
		pClientProvider->QueryService(guidService, riid,
			reinterpret_cast<void**>(&spDummy)) :
		E_NOINTERFACE;
	if (SUCCEEDED(hr))
	{
		hr = punkThis->QueryInterface(riid, ppv);
	}
	return hr;
}

} // end namespace PassthroughAPP::Detail

// ===== IInternetProtocolImpl =====

inline STDMETHODIMP IInternetProtocolImpl::SetTargetUnknown(
	IUnknown* punkTarget)
{
	ATLASSERT(punkTarget != 0);
	if (!punkTarget)
	{
		return E_POINTER;
	}

	// This method should only be called once, and be the only source
	// of target interface pointers.
	ATLASSERT(m_spInternetProtocolUnk == 0);
	ATLASSERT(m_spInternetProtocol == 0);
	if (m_spInternetProtocolUnk || m_spInternetProtocol)
	{
		return E_UNEXPECTED;
	}

	// We expect the target unknown to implement at least IInternetProtocol
	// Otherwise we reject it
	HRESULT hr = punkTarget->QueryInterface(&m_spInternetProtocol);
	ATLASSERT(FAILED(hr) || m_spInternetProtocol != 0);
	if (FAILED(hr))
	{
		return hr;
	}

	hr = m_spInternetProtocol->QueryInterface(&m_spInternetProtocolEx);
	ATLASSERT(FAILED(hr) || m_spInternetProtocolEx != 0);
	if (FAILED(hr))
	{
		return hr;
	}

	ATLASSERT(m_spInternetProtocolInfo == 0);
	ATLASSERT(m_spInternetPriority == 0);
	ATLASSERT(m_spInternetThreadSwitch == 0);
	ATLASSERT(m_spWinInetInfo == 0);
	ATLASSERT(m_spWinInetHttpInfo == 0);

	m_spInternetProtocolUnk = punkTarget;
	return S_OK;
}

inline void IInternetProtocolImpl::ReleaseAll()
{
	TRACE(_T("[Thread: %d] *%p->IInternetProtocolImpl::ReleaseAll()\n"), GetCurrentThreadId(), this);
	m_spInternetProtocolUnk.Release();
	m_spInternetProtocol.Release();
	m_spInternetProtocolEx.Release();
	m_spInternetProtocolInfo.Release();
	m_spInternetPriority.Release();
	m_spInternetThreadSwitch.Release();
	m_spWinInetInfo.Release();
	m_spWinInetHttpInfo.Release();
	m_spWinInetCacheHints.Release();
	m_spWinInetCacheHints2.Release();
}

// IInternetProtocolRoot
inline STDMETHODIMP IInternetProtocolImpl::Start(
	/* [in] */ LPCWSTR szUrl,
	/* [in] */ IInternetProtocolSink *pOIProtSink,
	/* [in] */ IInternetBindInfo *pOIBindInfo,
	/* [in] */ DWORD grfPI,
	/* [in] */ HANDLE_PTR dwReserved)
{
	TRACE(_T("[Thread: %d] *%p->IInternetProtocolImpl::Start()\n"), GetCurrentThreadId(), this);
	ATLASSERT(m_spInternetProtocol != 0);
	return m_spInternetProtocol ?
		m_spInternetProtocol->Start(szUrl, pOIProtSink, pOIBindInfo, grfPI,
			dwReserved) :
		E_UNEXPECTED;
}

inline STDMETHODIMP IInternetProtocolImpl::Continue(
	/* [in] */ PROTOCOLDATA *pProtocolData)
{
	TRACE(_T("[Thread: %d] *%p->IInternetProtocolImpl::Continue()\n"), GetCurrentThreadId(), this);
	ATLASSERT(m_spInternetProtocol != 0);
	return m_spInternetProtocol ?
		m_spInternetProtocol->Continue(pProtocolData) :
		E_UNEXPECTED;
}

inline STDMETHODIMP IInternetProtocolImpl::Abort(
	/* [in] */ HRESULT hrReason,
	/* [in] */ DWORD dwOptions)
{
	TRACE(_T("[Thread: %d] *%p->IInternetProtocolImpl::Abort()\n"), GetCurrentThreadId(), this);
	ATLASSERT(m_spInternetProtocol != 0);
	return m_spInternetProtocol ?
		m_spInternetProtocol->Abort(hrReason, dwOptions) :
		E_UNEXPECTED;
}

inline STDMETHODIMP IInternetProtocolImpl::Terminate(
	/* [in] */ DWORD dwOptions)
{
	TRACE(_T("[Thread: %d] *%p->IInternetProtocolImpl::Terminate()\n"), GetCurrentThreadId(), this);
	ATLASSERT(m_spInternetProtocol != 0);
	return m_spInternetProtocol ?
		m_spInternetProtocol->Terminate(dwOptions) :
		E_UNEXPECTED;
}

inline STDMETHODIMP IInternetProtocolImpl::Suspend()
{
	TRACE(_T("[Thread: %d] *%p->IInternetProtocolImpl::Suspend()\n"), GetCurrentThreadId(), this);
	ATLASSERT(m_spInternetProtocol != 0);
	return m_spInternetProtocol ?
		m_spInternetProtocol->Suspend() :
		E_UNEXPECTED;
}

inline STDMETHODIMP IInternetProtocolImpl::Resume()
{
	TRACE(_T("[Thread: %d] *%p->IInternetProtocolImpl::Resume()\n"), GetCurrentThreadId(), this);
	ATLASSERT(m_spInternetProtocol != 0);
	return m_spInternetProtocol ?
		m_spInternetProtocol->Resume() :
		E_UNEXPECTED;
}

// IInternetProtocol
inline STDMETHODIMP IInternetProtocolImpl::Read(
	/* [in, out] */ void *pv,
	/* [in] */ ULONG cb,
	/* [out] */ ULONG *pcbRead)
{
	TRACE(_T("[Thread: %d] *%p->IInternetProtocolImpl::Read()\n"), GetCurrentThreadId(), this);
	ATLASSERT(m_spInternetProtocol != 0);
	return m_spInternetProtocol ?
		m_spInternetProtocol->Read(pv, cb, pcbRead) :
		E_UNEXPECTED;
}

inline STDMETHODIMP IInternetProtocolImpl::Seek(
	/* [in] */ LARGE_INTEGER dlibMove,
	/* [in] */ DWORD dwOrigin,
	/* [out] */ ULARGE_INTEGER *plibNewPosition)
{
	TRACE(_T("[Thread: %d] *%p->IInternetProtocolImpl::Seek()\n"), GetCurrentThreadId(), this);
	ATLASSERT(m_spInternetProtocol != 0);
	return m_spInternetProtocol ?
		m_spInternetProtocol->Seek(dlibMove, dwOrigin, plibNewPosition) :
		E_UNEXPECTED;
}

inline STDMETHODIMP IInternetProtocolImpl::LockRequest(
	/* [in] */ DWORD dwOptions)
{
	TRACE(_T("[Thread: %d] *%p->IInternetProtocolImpl::LockRequest()\n"), GetCurrentThreadId(), this);
	ATLASSERT(m_spInternetProtocol != 0);
	return m_spInternetProtocol ?
		m_spInternetProtocol->LockRequest(dwOptions) :
		E_UNEXPECTED;
}

inline STDMETHODIMP IInternetProtocolImpl::UnlockRequest()
{
	TRACE(_T("[Thread: %d] *%p->IInternetProtocolImpl::UnlockRequest()\n"), GetCurrentThreadId(), this);
	ATLASSERT(m_spInternetProtocol != 0);
	return m_spInternetProtocol ?
		m_spInternetProtocol->UnlockRequest() :
		E_UNEXPECTED;
}

// IInternetProtocolEx

inline STDMETHODIMP IInternetProtocolImpl::StartEx(
	IUri *pUri,
	IInternetProtocolSink *pOIProtSink,
	IInternetBindInfo *pOIBindInfo,
	DWORD grfPI,
	HANDLE_PTR dwReserved)
{
	TRACE(_T("[Thread: %d] *%p->IInternetProtocolImpl::StartEx()\n"), GetCurrentThreadId(), this);
	ATLASSERT(m_spInternetProtocolEx != 0);
	return m_spInternetProtocolEx ?
		m_spInternetProtocolEx->StartEx(pUri, pOIProtSink, pOIBindInfo, grfPI, dwReserved) :
		E_UNEXPECTED;
}

// IInternetProtocolInfo
inline STDMETHODIMP IInternetProtocolImpl::ParseUrl(
	/* [in] */ LPCWSTR pwzUrl,
	/* [in] */ PARSEACTION ParseAction,
	/* [in] */ DWORD dwParseFlags,
	/* [out] */ LPWSTR pwzResult,
	/* [in] */ DWORD cchResult,
	/* [out] */ DWORD *pcchResult,
	/* [in] */ DWORD dwReserved)
{
	TRACE(_T("[Thread: %d] *%p->IInternetProtocolImpl::ParseUrl()\n"), GetCurrentThreadId(), this);
	ATLASSERT(m_spInternetProtocolInfo != 0);
	return m_spInternetProtocolInfo ?
		m_spInternetProtocolInfo->ParseUrl(pwzUrl, ParseAction, dwParseFlags,
			pwzResult, cchResult, pcchResult, dwReserved) :
		E_UNEXPECTED;
}

inline STDMETHODIMP IInternetProtocolImpl::CombineUrl(
	/* [in] */ LPCWSTR pwzBaseUrl,
	/* [in] */ LPCWSTR pwzRelativeUrl,
	/* [in] */ DWORD dwCombineFlags,
	/* [out] */ LPWSTR pwzResult,
	/* [in] */ DWORD cchResult,
	/* [out] */ DWORD *pcchResult,
	/* [in] */ DWORD dwReserved)
{
	TRACE(_T("[Thread: %d] *%p->IInternetProtocolImpl::CombineUrl()\n"), GetCurrentThreadId(), this);
	ATLASSERT(m_spInternetProtocolInfo != 0);
	return m_spInternetProtocolInfo ?
		m_spInternetProtocolInfo->CombineUrl(pwzBaseUrl, pwzRelativeUrl,
			dwCombineFlags, pwzResult, cchResult, pcchResult, dwReserved) :
		E_UNEXPECTED;
}

inline STDMETHODIMP IInternetProtocolImpl::CompareUrl(
	/* [in] */ LPCWSTR pwzUrl1,
	/* [in] */ LPCWSTR pwzUrl2,
	/* [in] */ DWORD dwCompareFlags)
{
	TRACE(_T("[Thread: %d] *%p->IInternetProtocolImpl::CompareUrl()\n"), GetCurrentThreadId(), this);
	ATLASSERT(m_spInternetProtocolInfo != 0);
	return m_spInternetProtocolInfo ?
		m_spInternetProtocolInfo->CompareUrl(pwzUrl1,pwzUrl2, dwCompareFlags) :
		E_UNEXPECTED;
}

inline STDMETHODIMP IInternetProtocolImpl::QueryInfo(
	/* [in] */ LPCWSTR pwzUrl,
	/* [in] */ QUERYOPTION QueryOption,
	/* [in] */ DWORD dwQueryFlags,
	/* [in, out] */ LPVOID pBuffer,
	/* [in] */ DWORD cbBuffer,
	/* [in, out] */ DWORD *pcbBuf,
	/* [in] */ DWORD dwReserved)
{
	TRACE(_T("[Thread: %d] *%p->IInternetProtocolImpl::QueryInfo(LPCWSTR, ...)\n"), GetCurrentThreadId(), this);
	ATLASSERT(m_spInternetProtocolInfo != 0);
	return m_spInternetProtocolInfo ?
		m_spInternetProtocolInfo->QueryInfo(pwzUrl, QueryOption, dwQueryFlags,
			pBuffer, cbBuffer, pcbBuf, dwReserved) :
		E_UNEXPECTED;
}

// IInternetPriority
inline STDMETHODIMP IInternetProtocolImpl::SetPriority(
	/* [in] */ LONG nPriority)
{
	TRACE(_T("[Thread: %d] *%p->IInternetProtocolImpl::SetPriority()\n"), GetCurrentThreadId(), this);
	ATLASSERT(m_spInternetPriority != 0);
	return m_spInternetPriority ?
		m_spInternetPriority->SetPriority(nPriority) :
		E_UNEXPECTED;
}

inline STDMETHODIMP IInternetProtocolImpl::GetPriority(
	/* [out] */ LONG *pnPriority)
{
	TRACE(_T("[Thread: %d] *%p->IInternetProtocolImpl::GetPriority()\n"), GetCurrentThreadId(), this);
	ATLASSERT(m_spInternetPriority != 0);
	return m_spInternetPriority ?
		m_spInternetPriority->GetPriority(pnPriority) :
	E_UNEXPECTED;
}

// IInternetThreadSwitch
inline STDMETHODIMP IInternetProtocolImpl::Prepare()
{
	TRACE(_T("[Thread: %d] *%p->IInternetProtocolImpl::Prepare()\n"), GetCurrentThreadId(), this);
	ATLASSERT(m_spInternetThreadSwitch != 0);
	return m_spInternetThreadSwitch ?
		m_spInternetThreadSwitch->Prepare() :
	E_UNEXPECTED;
}

inline STDMETHODIMP IInternetProtocolImpl::Continue()
{
	TRACE(_T("[Thread: %d] *%p->IInternetProtocolImpl::Continue()\n"), GetCurrentThreadId(), this);
	ATLASSERT(m_spInternetThreadSwitch != 0);
	return m_spInternetThreadSwitch ?
		m_spInternetThreadSwitch->Continue() :
	E_UNEXPECTED;
}

// IWinInetInfo
inline STDMETHODIMP IInternetProtocolImpl::QueryOption(
	/* [in] */ DWORD dwOption,
	/* [in, out] */ LPVOID pBuffer,
	/* [in, out] */ DWORD *pcbBuf)
{
	TRACE(_T("[Thread: %d] *%p->IInternetProtocolImpl::QueryOption()\n"), GetCurrentThreadId(), this);
	ATLASSERT(m_spWinInetInfo != 0);
	return m_spWinInetInfo ?
		m_spWinInetInfo->QueryOption(dwOption, pBuffer, pcbBuf) :
		E_UNEXPECTED;
}

// IWinInetHttpInfo
inline STDMETHODIMP IInternetProtocolImpl::QueryInfo(
	/* [in] */ DWORD dwOption,
	/* [in, out] */ LPVOID pBuffer,
	/* [in, out] */ DWORD *pcbBuf,
	/* [in, out] */ DWORD *pdwFlags,
	/* [in, out] */ DWORD *pdwReserved)
{
	TRACE(_T("[Thread: %d] *%p->IInternetProtocolImpl::QueryInfo(DWORD, ...)\n"), GetCurrentThreadId(), this);
	ATLASSERT(m_spWinInetHttpInfo != 0);
	return m_spWinInetHttpInfo ?
		m_spWinInetHttpInfo->QueryInfo(dwOption, pBuffer, pcbBuf, pdwFlags,
			pdwReserved) :
		E_UNEXPECTED;
}

// IWinInetCacheHints

inline STDMETHODIMP IInternetProtocolImpl::SetCacheExtension(
	/* [in] */			LPCWSTR pwzExt,
	/* [in, out] */ LPVOID pszCacheFile,
	/* [in, out] */ DWORD *pcbCacheFile,
	/* [in, out] */ DWORD *pdwWinInetError,
	/* [in, out] */	DWORD *pdwReserved)
{
	TRACE(_T("[Thread: %d] *%p->IInternetProtocolImpl::SetCacheExtension()\n"), GetCurrentThreadId(), this);
	 ATLASSERT(m_spWinInetCacheHints2 != NULL);
	 return m_spWinInetCacheHints2 ?
		 m_spWinInetCacheHints2->SetCacheExtension(pwzExt, pszCacheFile, pcbCacheFile,
			pdwWinInetError, pdwReserved) : E_UNEXPECTED;
}

inline STDMETHODIMP IInternetProtocolImpl::SetCacheExtension2(
	/* [in] */			LPCWSTR pwzExt,
	/* [out] */		 WCHAR *pwzCacheFile,
	/* [in, out] */ DWORD *pcchCacheFile,
	/* [out] */		 DWORD *pdwWinInetError,
	/* [out] */		 DWORD *pdwReserved)
{
	TRACE(_T("[Thread: %d] *%p->IInternetProtocolImpl::SetCacheExtension2()\n"), GetCurrentThreadId(), this);
	 ATLASSERT(m_spWinInetCacheHints2 != NULL);
	 return m_spWinInetCacheHints2 ?
		 m_spWinInetCacheHints2->SetCacheExtension2(pwzExt, pwzCacheFile, pcchCacheFile,
			pdwWinInetError, pdwReserved) : E_UNEXPECTED;
}

// ===== IInternetProtocolSinkImpl =====

inline HRESULT IInternetProtocolSinkImpl::InitMembers(IInternetProtocolSink *pOIProtSink, IInternetBindInfo *pOIBindInfo,
	IInternetProtocol* pTargetProtocol)
{
	TRACE(_T("[Thread: %d] *%p->IInternetProtocolSinkImpl::InitMembers()\n"), GetCurrentThreadId(), this);
	ATLASSERT(pOIProtSink != 0);
	ATLASSERT(pOIBindInfo != 0);
	ATLASSERT(pTargetProtocol != 0);
	if (!pOIProtSink || !pOIBindInfo || !pTargetProtocol)
	{
		return E_POINTER;
	}

	// This method should only be called once, and be the only source
	// of target interface pointers.
	ATLASSERT(m_spInternetProtocolSink == 0);
	ATLASSERT(m_spInternetBindInfo == 0);
	ATLASSERT(m_spTargetProtocol == 0);
	if (m_spInternetProtocolSink || m_spInternetBindInfo || m_spTargetProtocol)
	{
		return E_UNEXPECTED;
	}

	ATLASSERT(m_spServiceProvider == 0);

	m_spInternetProtocolSink = pOIProtSink;
	m_spInternetBindInfo = pOIBindInfo;
	if (FAILED(m_spInternetBindInfo->QueryInterface(&m_spInternetBindInfoEx)))
		m_spInternetBindInfoEx = NULL;
	m_spTargetProtocol = pTargetProtocol;
	return S_OK;
}

inline HRESULT IInternetProtocolSinkImpl::OnStart(LPCWSTR szUrl,
	IInternetProtocolSink *pOIProtSink, IInternetBindInfo *pOIBindInfo,
	DWORD grfPI, HANDLE_PTR dwReserved, IInternetProtocol* pTargetProtocol)
{
	TRACE(_T("[Thread: %d] *%p->IInternetProtocolSinkImpl::OnStart()\n"), GetCurrentThreadId(), this);
	return InitMembers(pOIProtSink, pOIBindInfo, pTargetProtocol);
}

inline HRESULT IInternetProtocolSinkImpl::OnStartEx(IUri* pUri,
	IInternetProtocolSink *pOIProtSink, IInternetBindInfo *pOIBindInfo,
	DWORD grfPI, HANDLE_PTR dwReserved, IInternetProtocol* pTargetProtocol)
{
	TRACE(_T("[Thread: %d] *%p->IInternetProtocolSinkImpl::OnStartEx()\n"), GetCurrentThreadId(), this);
	return InitMembers(pOIProtSink, pOIBindInfo, pTargetProtocol);
}


inline void IInternetProtocolSinkImpl::ReleaseAll()
{
	TRACE(_T("[Thread: %d] *%p->IInternetProtocolSinkImpl::ReleaseAll()\n"), GetCurrentThreadId(), this);
	m_spInternetProtocolSink.Release();
	m_spServiceProvider.Release();
	m_spInternetBindInfo.Release();
	m_spInternetBindInfoEx.Release();
	m_spUriContainer.Release();
	m_spTargetProtocol.Release();
}

inline IServiceProvider* IInternetProtocolSinkImpl::GetClientServiceProvider()
{
	TRACE(_T("[Thread: %d] *%p->IInternetProtocolSinkImpl::GetClientServiceProvider()\n"), GetCurrentThreadId(), this);
	return m_spServiceProvider;
}

inline HRESULT IInternetProtocolSinkImpl::QueryServiceFromClient(
	REFGUID guidService, REFIID riid, void** ppvObject)
{
	TRACE(_T("[Thread: %d] *%p->IInternetProtocolSinkImpl::QueryServiceFromClient()\n"), GetCurrentThreadId(), this);
	HRESULT hr = S_OK;
	CComPtr<IServiceProvider> spClientProvider = m_spServiceProvider;
	if (!spClientProvider)
	{
		hr = m_spInternetProtocolSink->QueryInterface(&spClientProvider);
		ATLASSERT(SUCCEEDED(hr) && spClientProvider != 0);
	}
	if (SUCCEEDED(hr))
	{
		hr = spClientProvider->QueryService(guidService, riid, ppvObject);
	}
	return hr;
}

// IInternetProtocolSink
inline STDMETHODIMP IInternetProtocolSinkImpl::Switch(
	/* [in] */ PROTOCOLDATA *pProtocolData)
{
	TRACE(_T("[Thread: %d] *%p->IInternetProtocolSinkImpl::Switch()\n"), GetCurrentThreadId(), this);
	ATLASSERT(m_spInternetProtocolSink != 0);
	return m_spInternetProtocolSink ?
		m_spInternetProtocolSink->Switch(pProtocolData) :
		E_UNEXPECTED;
}

inline STDMETHODIMP IInternetProtocolSinkImpl::ReportProgress(
	/* [in] */ ULONG ulStatusCode,
	/* [in] */ LPCWSTR szStatusText)
{
	TRACE(_T("[Thread: %d] *%p->IInternetProtocolSinkImpl::ReportProgress()\n"), GetCurrentThreadId(), this);
	ATLASSERT(m_spInternetProtocolSink != 0);
	return m_spInternetProtocolSink ?
		m_spInternetProtocolSink->ReportProgress(ulStatusCode, szStatusText) :
		E_UNEXPECTED;
}

inline STDMETHODIMP IInternetProtocolSinkImpl::ReportData(
	/* [in] */ DWORD grfBSCF,
	/* [in] */ ULONG ulProgress,
	/* [in] */ ULONG ulProgressMax)
{
	TRACE(_T("[Thread: %d] *%p->IInternetProtocolSinkImpl::ReportData()\n"), GetCurrentThreadId(), this);
	ATLASSERT(m_spInternetProtocolSink != 0);
	return m_spInternetProtocolSink ?
		m_spInternetProtocolSink->ReportData(grfBSCF, ulProgress,
			ulProgressMax) :
		E_UNEXPECTED;
}

inline STDMETHODIMP IInternetProtocolSinkImpl::ReportResult(
	/* [in] */ HRESULT hrResult,
	/* [in] */ DWORD dwError,
	/* [in] */ LPCWSTR szResult)
{
	TRACE(_T("[Thread: %d] *%p->IInternetProtocolSinkImpl::ReportResult()\n"), GetCurrentThreadId(), this);
	ATLASSERT(m_spInternetProtocolSink != 0);
	return m_spInternetProtocolSink ?
		m_spInternetProtocolSink->ReportResult(hrResult, dwError, szResult) :
		E_UNEXPECTED;
}

// IServiceProvider
inline STDMETHODIMP IInternetProtocolSinkImpl::QueryService(
	/* [in] */ REFGUID guidService,
	/* [in] */ REFIID riid,
	/* [out] */ void** ppvObject)
{
	TRACE(_T("[Thread: %d] *%p->IInternetProtocolSinkImpl::QueryService()\n"), GetCurrentThreadId(), this);
	ATLASSERT(m_spServiceProvider != 0);
	return m_spServiceProvider ?
		m_spServiceProvider->QueryService(guidService, riid, ppvObject) :
		E_UNEXPECTED;
}

// IInternetBindInfo
inline STDMETHODIMP IInternetProtocolSinkImpl::GetBindInfo(
	/* [out] */ DWORD *grfBINDF,
	/* [in, out] */ BINDINFO *pbindinfo)
{
	TRACE(_T("[Thread: %d] *%p->IInternetProtocolSinkImpl::GetBindInfo()\n"), GetCurrentThreadId(), this);
	ATLASSERT(m_spInternetBindInfo != 0);
	return m_spInternetBindInfo ?
		m_spInternetBindInfo->GetBindInfo(grfBINDF, pbindinfo) :
		E_UNEXPECTED;
}

inline STDMETHODIMP IInternetProtocolSinkImpl::GetBindString(
	/* [in] */ ULONG ulStringType,
	/* [in, out] */ LPOLESTR *ppwzStr,
	/* [in] */ ULONG cEl,
	/* [in, out] */ ULONG *pcElFetched)
{
	TRACE(_T("[Thread: %d] *%p->IInternetProtocolSinkImpl::GetBindString()\n"), GetCurrentThreadId(), this);
	ATLASSERT(m_spInternetBindInfo != 0);
	return m_spInternetBindInfo ?
		m_spInternetBindInfo->GetBindString(ulStringType, ppwzStr, cEl,
			pcElFetched) :
		E_UNEXPECTED;
}

// IInternetBindInfoEx
inline STDMETHODIMP IInternetProtocolSinkImpl::GetBindInfoEx(
	DWORD *grfBINDF,
	BINDINFO *pbindinfo,
	DWORD *grfBINDF2,
	DWORD *pdwReserved)
{
	TRACE(_T("[Thread: %d] *%p->IInternetProtocolSinkImpl::GetBindInfoEx()\n"), GetCurrentThreadId(), this);
	ATLASSERT(m_spInternetBindInfoEx != 0);
	return m_spInternetBindInfoEx ?
		m_spInternetBindInfoEx->GetBindInfoEx(grfBINDF, pbindinfo,
			grfBINDF2, pdwReserved) : E_UNEXPECTED;
}

inline STDMETHODIMP IInternetProtocolSinkImpl::GetIUri(
	IUri **ppIUri
)
{
	TRACE(_T("[Thread: %d] *%p->IInternetProtocolSinkImpl::GetIUri()\n"), GetCurrentThreadId(), this);
	ATLASSERT(m_spUriContainer != 0);
	return m_spUriContainer ?
		m_spUriContainer->GetIUri(ppIUri) : E_UNEXPECTED;
}

// ===== CInternetProtocolSinkWithSP =====

template <class T, class ThreadModel>
inline HRESULT CInternetProtocolSinkWithSP<T, ThreadModel>::OnStart(
	LPCWSTR szUrl, IInternetProtocolSink *pOIProtSink,
	IInternetBindInfo *pOIBindInfo,	DWORD grfPI, HANDLE_PTR dwReserved,
	IInternetProtocol* pTargetProtocol)
{
	ATLASSERT(m_spServiceProvider == 0);
	if (m_spServiceProvider)
	{
		return E_UNEXPECTED;
	}
	HRESULT hr = BaseClass::OnStart(szUrl, pOIProtSink, pOIBindInfo, grfPI,
		dwReserved, pTargetProtocol);
	if (SUCCEEDED(hr))
	{
		pOIProtSink->QueryInterface(&m_spServiceProvider);
	}
	return hr;
}

template <class T, class ThreadModel>
inline HRESULT CInternetProtocolSinkWithSP<T, ThreadModel>::OnStartEx(
	IUri* pUri, IInternetProtocolSink *pOIProtSink,
	IInternetBindInfo *pOIBindInfo,	DWORD grfPI, HANDLE_PTR dwReserved,
	IInternetProtocol* pTargetProtocol)
{
	ATLASSERT(m_spServiceProvider == 0);
	if (m_spServiceProvider)
	{
		return E_UNEXPECTED;
	}
	HRESULT hr = BaseClass::OnStartEx(pUri, pOIProtSink, pOIBindInfo, grfPI,
		dwReserved, pTargetProtocol);
	if (SUCCEEDED(hr))
	{
		pOIProtSink->QueryInterface(&m_spServiceProvider);
	}
	return hr;
}

template <class T, class ThreadModel>
inline HRESULT CInternetProtocolSinkWithSP<T, ThreadModel>::
	_InternalQueryService(REFGUID guidService, REFIID riid, void** ppvObject)
{
	return E_NOINTERFACE;
}

template <class T, class ThreadModel>
inline STDMETHODIMP CInternetProtocolSinkWithSP<T, ThreadModel>::QueryService(
	REFGUID guidService, REFIID riid, void** ppv)
{
	T* pT = static_cast<T*>(this);
	HRESULT hr = pT->_InternalQueryService(guidService, riid, ppv);
	if (FAILED(hr) && m_spServiceProvider)
	{
		hr = m_spServiceProvider->QueryService(guidService, riid, ppv);
	}
	return hr;
}

// ===== CInternetProtocol =====

// IInternetProtocolRoot
template <class StartPolicy, class ThreadModel>
inline STDMETHODIMP CInternetProtocol<StartPolicy, ThreadModel>::Start(
	LPCWSTR szUrl, IInternetProtocolSink *pOIProtSink,
	IInternetBindInfo *pOIBindInfo, DWORD grfPI, HANDLE_PTR dwReserved)
{
	ATLASSERT(m_spInternetProtocol != 0);
	if (!m_spInternetProtocol)
	{
		return E_UNEXPECTED;
	}

	return StartPolicy::OnStart(szUrl, pOIProtSink, pOIBindInfo, grfPI,
		dwReserved, m_spInternetProtocol);
}

// IInternetProtocolEx
template <class StartPolicy, class ThreadModel>
inline STDMETHODIMP CInternetProtocol<StartPolicy, ThreadModel>::StartEx(
	IUri* pUri, IInternetProtocolSink *pOIProtSink,
	IInternetBindInfo *pOIBindInfo, DWORD grfPI, HANDLE_PTR dwReserved)
{
	ATLASSERT(m_spInternetProtocolEx != 0);
	if (!m_spInternetProtocolEx)
	{
		return E_UNEXPECTED;
	}

	return StartPolicy::OnStartEx(pUri, pOIProtSink, pOIBindInfo, grfPI,
		dwReserved, m_spInternetProtocolEx);
}

} // end namespace PassthroughAPP

#endif // PASSTHROUGHAPP_PROTOCOLIMPL_INL
