#include "stdafx.h"

#include "wdevinitconfig.h"
#include "resource.h"

HWND g_hWndActionListBox = NULL;
HWND g_hWndListView = NULL;
HWND g_hWndStatus = NULL;
HWND g_hWndConfig = NULL;

extern const GUID       guid_enabled;
extern const GUID       guid_port;
extern const GUID       guid_addr;
extern const GUID       guid_actions;

extern unsigned int global_state;
extern unsigned int global_event;

extern unsigned int global_enabled;
extern unsigned int global_port;
extern unsigned int global_addr;

extern CRITICAL_SECTION             global_cs;
CRITICAL_SECTION                    global_execute_cs = { 0 };

extern cfg_int_t<unsigned int>      g_config_enabled;
extern cfg_int_t<unsigned int>      g_config_port;
extern cfg_int_t<unsigned int>      g_config_addr;
extern cfg_actions                  g_config_actions;

fb2kaction act_table[] = {
	{ "action_play",        "[component] Start playback" },
	{ "action_stop",        "[component] Stop playback" },
	{ "action_nexttrack",   "[component] Play next track" },
	{ "action_prevtrack",   "[component] Play previous track" },
	{ "action_rndtrack",    "[component] Play random track" },
	{ "action_pause",       "[component] Pause playback" },
	{ "action_fforward",    "[component] Fast Forward" },
	{ "action_frewind",     "[component] Fast Rewind" },
	{ "action_volumeup",    "[component] Increase volume" },
	{ "action_volumedown",  "[component] Decrease volume" },
	{ "action_volumemute",  "[component] Mute volume" },
	{ "action_appmaximaze", "[app] Show application maximazed" },
	{ "action_appminimaze", "[app] Show application minimazed" },
	{ "action_appnormal",   "[app] Show application normal" },
	{ "action_apphide",     "[app] Hide application" },
	{ "action_apprestore",  "[app] Restore application" },
	{ "action_appclose",    "[app] Close application" },
	{ "", "" }
};

class wdevwinlircconfig : public CDialogImpl<wdevwinlircconfig> {
public:
	enum { IDD = IDD_DIALOGCONFIG };

	BEGIN_MSG_MAP(wdevwinlircconfig)
		MESSAGE_HANDLER(WM_CLOSE, OnCloseClicked)
		MESSAGE_HANDLER(WM_TIMER, OnTimer)
		MESSAGE_HANDLER(WM_INITDIALOG, OnInitDialog)
		MESSAGE_HANDLER(WM_REMOTE_KEY, OnRemoteKey)
		COMMAND_HANDLER(IDC_ENABLED, BN_CLICKED, OnEnabledClicked)
		COMMAND_HANDLER(IDCANCEL, BN_CLICKED, OnCancelClicked)
		COMMAND_HANDLER(IDC_PORT, EN_CHANGE, OnEnChangePort)
		COMMAND_HANDLER(IDC_B_ADD, BN_CLICKED, OnBnClickedBAdd)
		COMMAND_HANDLER(IDC_B_REMOVE, BN_CLICKED, OnBnClickedBRemove)
		COMMAND_HANDLER(IDC_B_RESET, BN_CLICKED, OnBnClickedBReset)
		COMMAND_HANDLER(IDC_LB_ACTIONS, LBN_SELCHANGE, OnLbnSelchangeLbActions)
		NOTIFY_HANDLER(IDC_IPADDRESS, IPN_FIELDCHANGED, OnIpnFieldchangedIpaddress)
		COMMAND_HANDLER(IDAPPLY, BN_CLICKED, OnBnClickedApply)
	END_MSG_MAP()

	void update_state();
	void update_action_list();
	void update_bindings( int nActionIndex );

	LRESULT OnTimer(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled);

	LRESULT OnInitDialog(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled);
	LRESULT OnRemoteKey(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled);

	LRESULT OnCloseClicked(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled);

	LRESULT OnEnabledClicked(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL& bHandled);
	LRESULT OnCancelClicked(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL& bHandled);
	LRESULT OnBnClickedApply(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL& bHandled);

	LRESULT OnBnClickedBAdd(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL& bHandled);
	LRESULT OnBnClickedBRemove(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL& bHandled);
	LRESULT OnBnClickedBReset(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL& bHandled);

	LRESULT OnEnChangePort(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL& bHandled);
	LRESULT OnIpnFieldchangedIpaddress(int idCtrl, LPNMHDR pNMHDR, BOOL& bHandled);

	LRESULT OnLbnSelchangeLbActions(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL& bHandled);
	
	void OnClose();
};

LRESULT wdevwinlircconfig::OnTimer(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
{
	update_state();
	return TRUE;
}

void wdevwinlircconfig::OnClose()
{
	g_hWndListView = NULL;
	g_hWndStatus = NULL;
	g_hWndConfig = NULL;
	g_hWndActionListBox = NULL;

	::KillTimer(m_hWnd, 0);
	EndDialog(0);
}

LRESULT wdevwinlircconfig::OnEnabledClicked(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL& bHandled)
{
	::EnableWindow( ::GetDlgItem( m_hWnd, IDAPPLY ), TRUE );
	return 0;
}

LRESULT wdevwinlircconfig::OnCancelClicked(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL& bHandled)
{
	OnClose();
	return 0;
}

LRESULT wdevwinlircconfig::OnCloseClicked(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled) 
{ 
	OnClose();
	return 0;
}

LRESULT wdevwinlircconfig::OnRemoteKey(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
{
	BOOL bbr = FALSE;

	::EnterCriticalSection(&global_cs);

	lirc_info* info = (lirc_info*)wParam;
	if ( info )
	{
		HWND hWndKey = ::GetDlgItem( m_hWnd, IDC_E_KEY );
		HWND hWndRemote = ::GetDlgItem( m_hWnd, IDC_E_REMOTE );

		if ( hWndKey && hWndRemote ) {
			::SetWindowTextA(hWndKey, info->kn.c_str());
			::SetWindowTextA(hWndRemote, info->rn.c_str());
		}

		bbr = TRUE;
		goto out;
	}

out:
	::LeaveCriticalSection(&global_cs);
	return bbr;
}


LRESULT wdevwinlircconfig::OnIpnFieldchangedIpaddress(int /*idCtrl*/, LPNMHDR pNMHDR, BOOL& /*bHandled*/)
{
	LPNMIPADDRESS pIPAddr = reinterpret_cast<LPNMIPADDRESS>(pNMHDR);
	
/*
	int nCheck = (int)::SendMessage( ::GetDlgItem( m_hWnd, IDC_ENABLED ), BM_GETCHECK, 0, 0 );
	if ( nCheck == 0 ) {
		global_enabled = COMPONENT_DISABLED;
	} else if ( nCheck == 1 ) {
		::SendMessage( ::GetDlgItem( m_hWnd, IDC_ENABLED ), BM_SETCHECK, 0, 0L );
		global_enabled = COMPONENT_DISABLED;
	}

	g_config_enabled = global_enabled;
*/

	CIPAddressCtrl wnd_ipaddress( ::GetDlgItem( m_hWnd, IDC_IPADDRESS ) );

	DWORD dwAddress = 0;

	if ( wnd_ipaddress.GetAddress( &dwAddress ) == 4 ) {
		g_config_addr = htonl( dwAddress );
	}

	wnd_ipaddress.Detach();

	::EnableWindow( ::GetDlgItem( m_hWnd, IDAPPLY ), TRUE );

	return 0;
}

LRESULT wdevwinlircconfig::OnBnClickedApply(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL& bHandled)
{
	int nCheck = (int)::SendMessage( ::GetDlgItem( m_hWnd, IDC_ENABLED ), BM_GETCHECK, 0, 0 );
	if ( nCheck == 0 ) {
		global_enabled = COMPONENT_DISABLED;
	} else if ( nCheck == 1 ) {
		global_enabled = COMPONENT_ENABLED;
	}
	
	g_config_enabled = global_enabled;
	global_addr = g_config_addr.get_value();

	::EnableWindow( ::GetDlgItem( m_hWnd, IDAPPLY ), FALSE );
	// ::SetFocus( ::GetDlgItem( m_hWnd, IDCLOSE ) );

	return 0;
}


void wdevwinlircconfig::update_state()
{
	if ( g_hWndStatus != NULL )
	{
		pfc::string state_txt = "";
		pfc::string ipsn = "";

		struct in_addr saddr;
		saddr.s_addr = global_addr;

		ipsn = pfc::string_printf("%s", inet_ntoa( saddr ) );

		switch (global_state)
		{
		case CONNECT_STATE_FAILED:
			state_txt = "Unable to connect";
			break;
		case CONNECT_STATE_DISCONNECTED:
			state_txt = "Not connected";
			break;
		case CONNECT_STATE_CONNECTING:
			state_txt = "Connecting...";
			break;
		case CONNECT_STATE_CONNECTED:
			state_txt = pfc::string_printf( "Connected to LIRC Daemon server on %s:%d", ipsn.get_ptr(), global_port );
			break;
		default:
			state_txt = "Not implemented...";
			break;
		}

		::SetWindowTextA(g_hWndStatus, state_txt.get_ptr());
	}

	// HWND hWndCtl = ::GetDlgItem( m_hWnd, IDC_ENABLED);
	// ::SendMessage( hWndCtl, BM_SETCHECK, (WPARAM)global_enabled, 0 );
}

void wdevwinlircconfig::update_action_list()
{
	if ( g_hWndActionListBox != NULL )
	{
		::SendMessage( g_hWndActionListBox, LB_RESETCONTENT, 0, 0 );

		size_t nActionIndex = 0;
		while ( act_table[ nActionIndex ].action != "" )
		{
			int nItemIndex = uSendMessageText( g_hWndActionListBox, LB_ADDSTRING, 0, act_table[ nActionIndex ].description.c_str() );
			::SendMessage( g_hWndActionListBox, LB_SETITEMDATA, nItemIndex, nActionIndex );

			nActionIndex++;
		}
	}
}

void wdevwinlircconfig::update_bindings( int nActionIndex = 0 )
{
	if ( g_hWndListView != NULL )
	{
		LVITEM item;
		ListView_DeleteAllItems( g_hWndListView );

		int nActiveItem = 0;

		size_t i_count = g_config_actions.get_lactionscount();
		for ( size_t i = 0; i < i_count; i++ )
		{
			lirc_action* action_ptr = g_config_actions.get_lactionbylactionorder( i );
			if ( action_ptr )
			{
				ZeroMemory(&item, sizeof(item));

				item.mask = LVIF_TEXT | LVIF_PARAM;

				size_t string_size = 0;
				wchar_t* wText = nullptr;

				size_t ch_c;

				string_size = strlen( action_ptr->kn.c_str() ) + 1;
				wText = new wchar_t[ string_size ];

				ch_c = 0;
				mbstowcs_s( &ch_c, wText, string_size, action_ptr->kn.c_str(), string_size);

				item.pszText = wText;
				item.iItem = i;
				item.iSubItem = 0;
				item.lParam = action_ptr->naction;

				if ( action_ptr->naction == nActionIndex ) {
					nActiveItem = i;
				}

				ListView_InsertItem(g_hWndListView, &item);
				delete wText;

				item.mask = LVIF_TEXT;
				item.iItem = i;
				item.iSubItem = 1;

				string_size = strlen( act_table[action_ptr->naction].description.c_str() ) + 1;
				wText = new wchar_t[ string_size ];
				
				ch_c = 0;
				mbstowcs_s( &ch_c, wText, string_size, act_table[action_ptr->naction].description.c_str(), string_size);

				item.pszText = wText;

				ListView_SetItem(g_hWndListView, &item);
				delete wText;
			}
		}

		ZeroMemory(&item, sizeof(item));

		item.state = LVIS_SELECTED | LVIS_FOCUSED;
		item.stateMask = LVIS_SELECTED | LVIS_FOCUSED;
		
		if ( ::SendMessage( g_hWndListView, LVM_SETITEMSTATE, nActiveItem, (LPARAM)&item) ) {
			::SendMessage( g_hWndListView, LVM_SETSELECTIONMARK, 0, nActiveItem );
			::SendMessage( g_hWndListView, LVM_ENSUREVISIBLE, nActiveItem, MAKELPARAM( FALSE, 0 ) );
		}
	}
}

LRESULT wdevwinlircconfig::OnInitDialog(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
{
	g_hWndConfig = m_hWnd;

	CenterWindow();

	g_hWndActionListBox = ::GetDlgItem( m_hWnd, IDC_LB_ACTIONS );
	g_hWndListView = ::GetDlgItem( m_hWnd, IDC_LISTVIEW );
	g_hWndStatus = ::GetDlgItem( m_hWnd, IDC_STAT );

	if ( g_hWndListView != NULL )
	{
		LVCOLUMN col;
		ListView_SetExtendedListViewStyle(g_hWndListView, LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES);
	
		::ZeroMemory(&col, sizeof(col));
	
		col.mask = LVCF_TEXT;

		col.pszText = L"Button";
		ListView_InsertColumn(g_hWndListView, 0, &col);
		col.pszText = L"Action";
		ListView_InsertColumn(g_hWndListView, 1, &col);
		
		ListView_SetColumnWidth(g_hWndListView, 0, 100);
		ListView_SetColumnWidth(g_hWndListView, 1, 300);
	}

	HICON hIcon = AtlLoadIconImage(IDI_COMPONENTICON, LR_DEFAULTCOLOR, ::GetSystemMetrics(SM_CXICON), ::GetSystemMetrics(SM_CYICON));
	SetIcon(hIcon, TRUE);

	HICON hIconSmall = AtlLoadIconImage(IDI_COMPONENTICON, LR_DEFAULTCOLOR, ::GetSystemMetrics(SM_CXSMICON), ::GetSystemMetrics(SM_CYSMICON));
	SetIcon(hIconSmall, FALSE);

	::SendMessage( ::GetDlgItem( m_hWnd, IDC_IPADDRESS ), IPM_SETADDRESS, 0, ntohl(global_addr) );
	::SetDlgItemInt( m_hWnd, IDC_PORT, global_port, false );

	HWND hWndCtl = ::GetDlgItem( m_hWnd, IDC_ENABLED);
	::SendMessage( hWndCtl, BM_SETCHECK, global_enabled, 0 );

	update_action_list();
	update_bindings();

	::SetTimer( m_hWnd, 0, 500, 0); 

	return TRUE;
}

LRESULT wdevwinlircconfig::OnEnChangePort(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	BOOL bTranslated = TRUE;

	unsigned int nResPort = ::GetDlgItemInt( m_hWnd, IDC_PORT, &bTranslated, FALSE );
	if ( bTranslated > 0 ) {
		g_config_port = nResPort;
		global_port = g_config_port.get_value();
	}

	::EnableWindow( ::GetDlgItem( m_hWnd, IDAPPLY ), TRUE );

	return 0;
}

LRESULT wdevwinlircconfig::OnBnClickedBAdd(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	CListBox wnd_listbox( ::GetDlgItem( m_hWnd, IDC_LB_ACTIONS ) );

	pfc::string kn = uGetDlgItemText( m_hWnd, IDC_E_KEY );
	pfc::string rn = uGetDlgItemText( m_hWnd, IDC_E_REMOTE );

	int nItemIndex = wnd_listbox.GetCurSel();
	if ( ( nItemIndex >= 0 ) && ( kn.length() > 0 ) && ( rn.length() > 0 ) ) {
		int nActionIndex = wnd_listbox.GetItemData(nItemIndex);
		if ( nActionIndex >= 0 ) {
			bool br = g_config_actions.assignaction_to_remotekey( nActionIndex, kn.c_str(), rn.c_str() );
			if ( br == true ) {
				update_bindings( nActionIndex );
			} else {
				::MessageBoxA( g_hWndConfig, "This key from this remote control already assigned...", "Assign key", 0);
			}
		}
	}

	wnd_listbox.Detach();

	::EnableWindow( ::GetDlgItem( m_hWnd, IDAPPLY ), TRUE );

	return 0;
}


LRESULT wdevwinlircconfig::OnBnClickedBRemove(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	CListViewCtrl wnd_listview( ::GetDlgItem( m_hWnd, IDC_LISTVIEW ) );

	int nItemIndex = wnd_listview.GetSelectedIndex();
	if ( nItemIndex >= 0 )
	{
		int nActionIndex = wnd_listview.GetItemData( nItemIndex );
		
		if ( nActionIndex >= 0 ) {
			g_config_actions.delete_lactionbyindex( nActionIndex );
			update_bindings();
		}
	}

	wnd_listview.Detach();

	::EnableWindow( ::GetDlgItem( m_hWnd, IDAPPLY ), TRUE );

	return 0;
}


LRESULT wdevwinlircconfig::OnBnClickedBReset(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	g_config_actions.delete_all();
	update_bindings();

	::EnableWindow( ::GetDlgItem( m_hWnd, IDAPPLY ), TRUE );

	return 0;
}


LRESULT wdevwinlircconfig::OnLbnSelchangeLbActions(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL& bHandled)
{
	CListBox wnd_listbox( ::GetDlgItem( m_hWnd, IDC_LB_ACTIONS ) );

	int nItemIndex = wnd_listbox.GetCurSel();
	int nActionIndex = wnd_listbox.GetItemData(nItemIndex);

	wnd_listbox.Detach();

	CStatic wnd_static( ::GetDlgItem( m_hWnd, IDC_DESC ) );

	size_t string_size = 0;
	wchar_t* wText = nullptr;

	size_t ch_c;

	pfc::string desc = "Description: ";
	desc += act_table[nActionIndex].description;
	desc += " ( ";
	desc += act_table[nActionIndex].action;
	desc += " )";

	string_size = strlen(desc.c_str()) + 1;
	wText = new wchar_t[ string_size ];

	ch_c = 0;
	mbstowcs_s( &ch_c, wText, string_size, desc.c_str(), string_size);

	wnd_static.SetWindowTextW( wText );
	wnd_static.Detach();

	nItemIndex = -1;

	CListViewCtrl wnd_listview( ::GetDlgItem( m_hWnd, IDC_LISTVIEW ) );

	int nCount = wnd_listview.GetItemCount();
	if ( nCount > 0 ) {
		for ( int i = 0; i < nCount; i++ ) {
			int nItemData = wnd_listview.GetItemData( i );
			if ( nItemData == nActionIndex ) {
				nItemIndex = i;
				break;
			}
		}
		if ( nItemIndex != -1 ) {
			wnd_listview.SelectItem( nItemIndex );
		}
	}

	wnd_listview.Detach();

	return 0;
}

void wdevwinlircconfig_dialogshow(t_uint32 p_index) 
{
	try {
		wdevwinlircconfig dlg;
		dlg.DoModal(core_api::get_main_window());

		//////////////////////////////////////////////
		//  g_hWndListView = NULL;
		//  g_hWndStatus = NULL;
		//  g_hWndConfig = NULL;
		//  g_hWndActionListBox = NULL;
		//////////////////////////////////////////////

	} catch( std::exception const& e ) {
		popup_message::g_complain("dialog creation failure", e);
	}
}

// class wdevwinlircexecutor_window : public CWindowImpl<wdevwinlircexecutor_window>, private play_callback_impl_base
class wdevwinlircexecutor : private play_callback_impl_base
{
public:
	wdevwinlircexecutor( const char* szaction = 0 ) {
		if ( szaction )
			setlirc_action( szaction );
	}

	virtual ~wdevwinlircexecutor() {
	
	}

	bool DoCommand( const char* szaction = 0 )
	{
		if ( szaction )
			setlirc_action( szaction );

		return execute();
	}

	bool execute()
	{
		bool brr = true;

		if ( strcmp( getlirc_action(), "action_play" ) == 0 ) {
			m_pbctl->start();
		} else if ( strcmp( getlirc_action(), "action_stop" ) == 0 ) {
			m_pbctl->stop();
		} else if ( strcmp( getlirc_action(), "action_nexttrack" ) == 0 ) {
			m_pbctl->start(playback_control::track_command_next);
		} else if ( strcmp( getlirc_action(), "action_prevtrack" ) == 0 ) {
			m_pbctl->start(playback_control::track_command_prev);
		} else if ( strcmp( getlirc_action(), "action_rndtrack" ) == 0 ) {
			m_pbctl->start(playback_control::track_command_rand);
		} else if ( strcmp( getlirc_action(), "action_pause" ) == 0 ) {
			m_pbctl->toggle_pause();
		} else if ( strcmp( getlirc_action(), "action_fforward" ) == 0 ) {
			if ( m_pbctl->playback_can_seek() ) {
				double time = m_pbctl->playback_get_position();

				time += 0.25;
				m_pbctl->playback_seek(time);

				brr = false;
			}
		} else if ( strcmp( getlirc_action(), "action_frewind" ) == 0 ) {
			if ( m_pbctl->playback_can_seek() ) {
				double time = m_pbctl->playback_get_position();

				time -= 0.25;
				if ( time < 0.0 ) time = 0.0;

				m_pbctl->playback_seek(time);

				brr = false;
			}
		} else if ( strcmp( getlirc_action(), "action_volumeup" ) == 0 ) {
			float vol = m_pbctl->get_volume();
			
			vol += 0.25f;
			if ( vol > 0.0f ) vol = 0.0f;

			m_pbctl->set_volume(vol);

			brr = false;
		} else if ( strcmp( getlirc_action(), "action_volumedown" ) == 0 ) {
			float vol = m_pbctl->get_volume();

			vol -= 0.25f;
			if ( vol < -100.0f ) vol = -100.0f;

			m_pbctl->set_volume(vol);

			brr = false;
		} else if ( strcmp( getlirc_action(), "action_volumemute" ) == 0 ) {
			m_pbctl->volume_mute_toggle();
		} else if ( strcmp( getlirc_action(), "action_appmaximaze" ) == 0 ) {
			::ShowWindow(core_api::get_main_window(), SW_SHOWMAXIMIZED );
			::UpdateWindow(core_api::get_main_window());
		} else if ( strcmp( getlirc_action(), "action_appminimaze" ) == 0 ) {
			::ShowWindow(core_api::get_main_window(), SW_SHOWMINIMIZED );
			::UpdateWindow(core_api::get_main_window());
		} else if ( strcmp( getlirc_action(), "action_appnormal" ) == 0 ) {
			::ShowWindow(core_api::get_main_window(), SW_SHOWNORMAL );
			::UpdateWindow(core_api::get_main_window());
		} else if ( strcmp( getlirc_action(), "action_apprestore" ) == 0 ) {
			::ShowWindow(core_api::get_main_window(), SW_RESTORE );
			::UpdateWindow(core_api::get_main_window());
		} else if ( strcmp( getlirc_action(), "action_apphide" ) == 0 ) {
			::ShowWindow(core_api::get_main_window(), SW_HIDE );
		} else if ( strcmp( getlirc_action(), "action_appclose" ) == 0 ) {
			::SendMessage( core_api::get_main_window(), WM_SYSCOMMAND, SC_CLOSE, 0 );
		} 
		
		return brr;
	}

	void setlirc_action( const char* szaction )
	{
		m_action = szaction;
	}

	const char* getlirc_action()
	{
		return m_action.c_str();
	}

private:

	//  Playback callback methods.
	void on_playback_starting(play_control::t_track_command p_command,bool p_paused) {update();}
	void on_playback_new_track(metadb_handle_ptr p_track) {update();}
	void on_playback_stop(play_control::t_stop_reason p_reason) {update();}
	void on_playback_seek(double p_time) {update();}
	void on_playback_pause(bool p_state) {update();}
	void on_playback_edited(metadb_handle_ptr p_track) {update();}
	void on_playback_dynamic_info(const file_info & p_info) {update();}
	void on_playback_dynamic_info_track(const file_info & p_info) {update();}
	void on_playback_time(double p_time) {update();}
	void on_volume_change(float p_new_val) {}

	void update() {}

	static_api_ptr_t<playback_control> m_pbctl;

	pfc::string m_action;
};

void wdevwinlirclauncher_commandexec( const char* szaction ) 
{
	try {
		if ( ::TryEnterCriticalSection( &global_execute_cs ) ) {
			wdevwinlircexecutor dlg;
			if ( dlg.DoCommand( szaction ) )
				::Sleep( 1200 );
			::LeaveCriticalSection( &global_execute_cs );
		}

		//////////////////////////////////////////////
		//  g_hWndListView = NULL;
		//  g_hWndStatus = NULL;
		//  g_hWndConfig = NULL;
		//  g_hWndActionListBox = NULL;
		//////////////////////////////////////////////

	} catch( std::exception const& e ) {
		popup_message::g_complain("excutor creation failure", e);
	}
}
