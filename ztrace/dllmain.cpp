// dllmain.cpp : 定义 DLL 应用程序的入口点。


#include "myTrace.h"

int plugin_hd;
HWND main_wnd;

PLUG_EXPORT bool pluginit(PLUG_INITSTRUCT* initStruct)
{
	initStruct->pluginVersion = PLUGIN_VERSION;
	initStruct->sdkVersion = PLUG_SDKVERSION;
	strncpy_s(initStruct->pluginName, PLUGIN_NAME, _TRUNCATE);

	plugin_hd = initStruct->pluginHandle;
	_plugin_registercallback(plugin_hd, CB_PAUSEDEBUG, (CBPLUGIN)CB_mypaused);
	_plugin_registercommand(plugin_hd, PLUGIN_NAME, cb_cmd_start_myTrace, true);

	return true;
}

PLUG_EXPORT bool plugstop()
{
	_plugin_unregistercommand(plugin_hd, PLUGIN_NAME);
	_plugin_unregistercallback(plugin_hd, CB_PAUSEDEBUG);
	return true;
}

PLUG_EXPORT void plugsetup(PLUG_SETUPSTRUCT* setupStruct)
{
	main_wnd = setupStruct->hwndDlg;
	_plugin_menuaddentry(setupStruct->hMenu, myMenu::Menu_MAIN_start_trace, "start myTrace");
	_plugin_menuaddentry(setupStruct->hMenu, myMenu::Menu_MAIN_stop_trace, "stop myTrace");
	//_plugin_menuaddentry(setupStruct->hMenuDisasm, myMenu::Menu_DISASM_mytrace, "myTrace");
}

// PLUG_EXPORT void CBINITDEBUG(CBTYPE cbType, PLUG_CB_INITDEBUG* info)
// {
//     dprintf("Debugging of %s started!\n", info->szFileName);
// }

// PLUG_EXPORT void CBSTOPDEBUG(CBTYPE cbType, PLUG_CB_STOPDEBUG* info)
// {
//     dputs("Debugging stopped!");
// }

// PLUG_EXPORT void CBEXCEPTION(CBTYPE cbType, PLUG_CB_EXCEPTION* info)
// {
//     dprintf("ExceptionRecord.ExceptionCode: %08X\n", info->Exception->ExceptionRecord.ExceptionCode);
// }

// PLUG_EXPORT void CBDEBUGEVENT(CBTYPE cbType, PLUG_CB_DEBUGEVENT* info)
// {
//     if(info->DebugEvent->dwDebugEventCode == EXCEPTION_DEBUG_EVENT)
//     {
//         dprintf("DebugEvent->EXCEPTION_DEBUG_EVENT->%.8X\n", info->DebugEvent->u.Exception.ExceptionRecord.ExceptionCode);
//     }
// }

PLUG_EXPORT void CBMENUENTRY(CBTYPE cbType, PLUG_CB_MENUENTRY* info)
{
	switch (info->hEntry) {
	case myMenu::Menu_MAIN_start_trace:
		cb_menu_start_myTrace();
		break;

	case myMenu::Menu_MAIN_stop_trace:
		stop_mytrace();
		break;

	default:
		break;
	}
}

// 保存每次x64dbg暂停时的 寄存器, 线程ID
// 为了防止<读-写不同步>, 特意封装成这样
class PausedInfo
{
	CRITICAL_SECTION m_lock;
	REGDUMP m_regs;
	DWORD m_threadid;			//便于判断paused发生时, 是不是切换了thread
							//当我们手动step_in/over时, 是一定不会切换thread的
	DWORD m_target_threadid;
	HANDLE m_event_paused_on_target_thread;

public:
	PausedInfo()
	{
		InitializeCriticalSection(&m_lock);
		m_event_paused_on_target_thread = CreateEvent(NULL, FALSE, FALSE, L"myTrace_event_paused_on_target_thread");
	}

	~PausedInfo()
	{
		DeleteCriticalSection(&m_lock);
	}

	void lock()
	{
		EnterCriticalSection(&m_lock);
	}

	void unlock()
	{
		LeaveCriticalSection(&m_lock);
	}

	void update()
	{
		lock();
		DbgGetRegDumpEx(&m_regs, sizeof(REGDUMP));
		m_threadid = DbgGetThreadId();
		//dprintf("paused, thread_id: %d, cip: %p\n", m_threadid, m_regs.regcontext.cip);
		if (m_threadid == m_target_threadid) {
			//dprintf("set event for thread: %d\n", m_threadid);
			SetEvent(m_event_paused_on_target_thread);
		}
		unlock();
	}

	void get(REGDUMP &_regs, DWORD &_id)
	{
		lock();
		_regs = m_regs;
		_id = m_threadid;
		unlock();
	}

	void set_target_threadid(DWORD threadid)
	{
		lock();
		m_target_threadid = threadid;
		//dprintf("set target thread: %d\n", m_target_threadid);
		ResetEvent(m_event_paused_on_target_thread);
		unlock();
	}

	void waitfor_thread_paused()
	{
		if (m_event_paused_on_target_thread != INVALID_HANDLE_VALUE) {
			WaitForSingleObject(m_event_paused_on_target_thread, INFINITE);
			ResetEvent(m_event_paused_on_target_thread);
		}
	}
};

static PausedInfo g_paused_info;
void CB_mypaused(CBTYPE cbType, PLUG_CB_PAUSEDEBUG * callbackInfo)
{
	//每次发生了暂停事件, 我们就更新一下寄存器
	g_paused_info.update();
}


/*****************************************************************************/

//给其他部分代码提供的访问接口
void get_paused_info(REGDUMP &regs, DWORD &threadid)
{
	g_paused_info.get(regs, threadid);
}

void set_target_thread(DWORD threadid)
{
	g_paused_info.set_target_threadid(threadid);
}

void my_step_in()
{
	static char cmd[] = "sti";
	//g_paused_info.set_target_threadid(threadid);
	//DbgCmdExecDirect(cmd);
	DbgCmdExec(cmd);
	g_paused_info.waitfor_thread_paused();
}

void my_step_over()
{
	static char cmd[] = "sto";
	//g_paused_info.set_target_threadid(threadid);
	//DbgCmdExecDirect(cmd);
	DbgCmdExec(cmd);
	g_paused_info.waitfor_thread_paused();
}

void my_run_to(duint va)
{
	char cmd[64];
	sprintf(cmd, "run %p", va);
	DbgCmdExec(cmd);
	g_paused_info.waitfor_thread_paused();
}


