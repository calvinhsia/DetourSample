// dllmain.h : Declaration of module class.

class CATLProject1Module : public ATL::CAtlDllModuleT< CATLProject1Module >
{
public :
	DECLARE_LIBID(LIBID_ATLProject1Lib)
	DECLARE_REGISTRY_APPID_RESOURCEID(IDR_ATLPROJECT1, "{6c707ad5-98d6-4d91-812e-bc2e2890d8ad}")
};

extern class CATLProject1Module _AtlModule;
