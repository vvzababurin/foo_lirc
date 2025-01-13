#include "stdafx.h"

#include "wdevinitquit.h"
#include "wdevnetlib.h"
#include "wdevinitconfig.h"

extern HWND g_hWndActionListBox;
extern HWND g_hWndListView;
extern HWND g_hWndStatus;
extern HWND g_hWndConfig;

extern const GUID		guid_enabled;
extern const GUID		guid_port;
extern const GUID		guid_addr;
extern const GUID		guid_actions;

extern unsigned int		global_state;
extern unsigned int		global_event;

extern unsigned int		global_enabled;
extern unsigned int		global_port;
extern unsigned int		global_addr;

extern CRITICAL_SECTION				global_cs;
extern CRITICAL_SECTION				global_execute_cs;

extern cfg_int_t<unsigned int>      g_config_enabled;
extern cfg_int_t<unsigned int>      g_config_port;
extern cfg_int_t<unsigned int>      g_config_addr;
extern cfg_actions					g_config_actions;

extern HANDLE	thread_handle;
extern DWORD	thread_id;
extern HHOOK	message_hook;

extern fb2kaction			act_table[];

LRESULT WINAPI wdevlirchook_callback(int nCode, WPARAM wParam, LPARAM lParam)
{
	PMSG msg = (PMSG)lParam;

	if ( nCode < 0 )
		return ::CallNextHookEx(message_hook, nCode, wParam, lParam); 

	if ( nCode == HC_ACTION ) {
		if ( ( core_api::get_main_window() == msg->hwnd ) && ( msg->message == WM_REMOTE_KEY ) ) {
			lirc_info* info = (lirc_info*)msg->wParam;
			if ( info ) {
				::EnterCriticalSection(&global_cs);
				
				pfc::string kn = info->kn;
				pfc::string rn = info->rn;

				::LeaveCriticalSection(&global_cs);

				lirc_action* action_ptr = g_config_actions.get_lactionbyremotekey( kn.c_str(), rn.c_str() );
				if ( action_ptr ) {
					wdevwinlirclauncher_commandexec( act_table[action_ptr->naction].action.c_str() );
				}
			}
		}
	}

	return ::CallNextHookEx(message_hook, nCode, wParam, lParam);
}

BOOL wdevlirc_init()
{
	::InitializeCriticalSection( &global_cs );
	::InitializeCriticalSection( &global_execute_cs );
	
	thread_handle = ::CreateThread( NULL, 0, (LPTHREAD_START_ROUTINE)wdevlircthread_callback, 0, CREATE_SUSPENDED, &thread_id );
	if ( thread_handle ) 
	{
		message_hook = ::SetWindowsHookEx( WH_GETMESSAGE, (HOOKPROC)wdevlirchook_callback, 0, ::GetCurrentThreadId() ); //core_api::get_my_instance(), 0 );
		if ( message_hook )
		{
			global_state = CONNECT_STATE_DISCONNECTED;

			::ResumeThread( thread_handle );
			return TRUE;
		}
	}

	return FALSE;
}

void wdevlirc_quit()
{
	::UnhookWindowsHookEx( message_hook );
	message_hook = nullptr;

	::WaitForSingleObject( thread_handle, 10000 );

	::DeleteCriticalSection( &global_execute_cs );
	::DeleteCriticalSection( &global_cs );

	thread_handle = nullptr;
}

class wdevinitquit : public initquit {
public:
	void on_init() {
		wdevnamespace::init();

		global_enabled = g_config_enabled;
		global_port = g_config_port;
		global_addr = g_config_addr;

		if ( wdevlirc_init() ) {
			update_event(COMPONENT_EVENT_CREATED);
		} else {
			update_event(COMPONENT_EVENT_UNKNOWN);
		}
	}
	void on_quit() {
		update_event(COMPONENT_EVENT_DESTROY);
		wdevlirc_quit();

		wdevnamespace::term();
	}
};

static initquit_factory_t<wdevinitquit> g_wdevinitquit_factory;

