#include "stdafx.h"

#include "wdevinitconfig.h"
#include "wdevnetlib.h"
#include "resource.h"

const GUID guid_enabled    = { 0xda7cd0de, 0x01, 0xff, { 0x65, 0x6e, 0x61, 0x62, 0x6c, 0x65, 0x64, 0x30 } };
const GUID guid_port       = { 0xda7cd0de, 0x02, 0xff, { 0x65, 0x6e, 0x61, 0x62, 0x6c, 0x65, 0x64, 0x30 } };
const GUID guid_addr       = { 0xda7cd0de, 0x03, 0xff, { 0x65, 0x6e, 0x61, 0x62, 0x6c, 0x65, 0x64, 0x30 } };
const GUID guid_actions    = { 0xda7cd0de, 0xfe, 0xff, { 0x65, 0x6e, 0x61, 0x62, 0x6c, 0x65, 0x64, 0x30 } };

extern HWND g_hWndActionListBox;
extern HWND g_hWndListView;
extern HWND g_hWndStatus;
extern HWND g_hWndConfig;

HANDLE		thread_handle = 0;
DWORD		thread_id = 0;
HHOOK		message_hook = 0;

static lirc_info lirc;

unsigned int global_state = CONNECT_STATE_DISCONNECTED;
unsigned int global_event = COMPONENT_EVENT_UNKNOWN;

unsigned int global_enabled = COMPONENT_DISABLED;
unsigned int global_port = 8765;
unsigned int global_addr = inet_addr( "127.0.0.1" );

CRITICAL_SECTION            global_cs = { 0 };
extern CRITICAL_SECTION     global_execute_cs;

cfg_int_t<unsigned int>     g_config_enabled(guid_enabled, global_enabled);
cfg_int_t<unsigned int>     g_config_port(guid_port, global_port);
cfg_int_t<unsigned int>     g_config_addr(guid_addr, global_addr);
cfg_actions                 g_config_actions(guid_actions);

extern fb2kaction           act_table[];

void update_state(unsigned int _state)
{
	global_state = _state;
}

void update_event(unsigned int _event)
{
	global_event = _event;
}

DWORD WINAPI wdevlircthread_callback(LPVOID param)
{
	do {
		pfc::string ipsn = "";

		if ( global_enabled != COMPONENT_ENABLED )
			goto out;

		fd_set readfd;
		fd_set allfd;

		FD_ZERO(&readfd);
		FD_ZERO(&allfd);

		update_state(CONNECT_STATE_CONNECTING);

		struct in_addr saddr;
		saddr.s_addr = global_addr;

		ipsn = pfc::string_printf("%s", inet_ntoa( saddr ) );

		// console::printf( "connect to: %s:%d", ipsn.c_str(), global_port );

		int s = wdevnamespace::create_tcp_client(ipsn.get_ptr(), global_port);
		if( !isvalidsock(s) ) {
			update_state(CONNECT_STATE_FAILED);
			goto out;
		}

		::Sleep(1000);
		update_state(CONNECT_STATE_CONNECTED);

		FD_SET(s, &readfd);
		FD_SET(s, &allfd);

		do {
			struct timeval tv;

			::memcpy(&readfd, &allfd, sizeof(fd_set));

			if ( global_enabled != COMPONENT_ENABLED )
				goto out;

			tv.tv_sec = 0;
			tv.tv_usec = 10000L;

			int r = select(s + 1, &readfd, 0, 0, &tv);
			if (r == 0) {
				continue;
			} 
			else if (r < 0) {
				///////////////////////////////////////////////////////////////
				// global_enabled = COMPONENT_DISABLED;
				///////////////////////////////////////////////////////////////

				update_state(CONNECT_STATE_FAILED);
				break;
			} 
			else if (r > 0) {
				if (FD_ISSET(s, &readfd)) {
					char	buf[BUF_SIZ];

					::memset(buf, 0, BUF_SIZ);

					r = wdevnamespace::readn(s, buf, BUF_SIZ);
					if (r == 0) {
						///////////////////////////////////////////////////////////////
						// global_enabled = COMPONENT_DISABLED;
						///////////////////////////////////////////////////////////////

						update_state(CONNECT_STATE_DISCONNECTED);
						break;
					}
					else if (r >= BUF_SIZ) {
						global_enabled = COMPONENT_DISABLED;

						update_state(CONNECT_STATE_FAILED);
						break;
					}
					else if (r > 0) {
						unsigned __int64	kc = 0;
						unsigned int		rc = 0;
						char				kn[BUF_SIZ];
						char				rn[BUF_SIZ];

						::memset(kn, 0, BUF_SIZ);
						::memset(rn, 0, BUF_SIZ);

						sscanf_s(buf, "%I64x %x %s %[^\n]", &kc, &rc, kn, BUF_SIZ, rn, BUF_SIZ);
					
						::EnterCriticalSection(&global_cs);

						lirc.kc = kc;
						lirc.rc = rc;
						lirc.kn = kn;
						lirc.rn = rn;

						::LeaveCriticalSection(&global_cs);

						if ( g_hWndConfig == NULL ) {
							lirc_action* action_ptr = g_config_actions.get_lactionbyremotekey( kn, rn );
							if ( action_ptr ) {
								if ( ::TryEnterCriticalSection( &global_execute_cs ) ) {
									::PostMessage( core_api::get_main_window(), WM_REMOTE_KEY, (WPARAM)&lirc, 0 );
									::LeaveCriticalSection( &global_execute_cs );
								}
							}
						} else {
							::PostMessage( g_hWndConfig, WM_REMOTE_KEY, (WPARAM)&lirc, 0 );
						}
					}
				}
			}
		} while ( ( global_state == CONNECT_STATE_CONNECTED ) && ( global_event != COMPONENT_EVENT_DESTROY ) );

out:
		if ( isvalidsock(s) )
		{
			shutdown( s, SD_BOTH );
			closesock(s);
		}

		::Sleep( 500 );
		update_state(CONNECT_STATE_DISCONNECTED);

	} while ( global_event != COMPONENT_EVENT_DESTROY );
	
	::ExitThread(0);
}

cfg_actions::cfg_actions(const GUID & p_guid) : cfg_var(p_guid)
{

}

void cfg_actions::get_data_raw(stream_writer* p_stream, abort_callback& p_abort)
{
	p_stream->write_lendian_t( lactions.get_size(), p_abort );

	for ( size_t i = 0; i < lactions.get_size(); i++ ) {
		lirc_action* action_ptr = lactions.get_item(i);
		if ( action_ptr ) {
			p_stream->write_lendian_t( action_ptr->kc, p_abort );
			p_stream->write_lendian_t( action_ptr->rc, p_abort );
			p_stream->write_string_nullterm( action_ptr->rn.c_str(), p_abort );
			p_stream->write_string_nullterm( action_ptr->kn.c_str(), p_abort );
			p_stream->write_lendian_t( action_ptr->naction, p_abort );
		}
	}
}

void cfg_actions::set_data_raw(stream_reader* p_stream, t_size p_sizehint, abort_callback& p_abort)
{
	delete_all();

	int i_count = 0;
	p_stream->read_lendian_t( i_count, p_abort ); //alter member data only on success, this will throw an exception when something isn't right

	for ( int i = 0; i < i_count; i++ )
	{
		lirc_action* action_ptr = new lirc_action();
		if ( action_ptr )
		{
			pfc::string8 rn = "";
			pfc::string8 kn = "";

			p_stream->read_lendian_t( action_ptr->kc, p_abort );
			p_stream->read_lendian_t( action_ptr->rc, p_abort );
			p_stream->read_string_nullterm( rn, p_abort );
			p_stream->read_string_nullterm( kn, p_abort );
			p_stream->read_lendian_t( action_ptr->naction, p_abort );

			action_ptr->kn = kn;
			action_ptr->rn = rn;

			lactions.add_item( action_ptr );
		}
	}
}

lirc_action* cfg_actions::get_lactionbyactionindex( int nactionindex )
{
	for ( size_t i = 0; i < lactions.get_count(); i++ ) {
		lirc_action* action_ptr = lactions[ i ];
		if ( action_ptr ) {
			if ( action_ptr->naction == nactionindex ) {
				return action_ptr;
			}
		}
	}
	return nullptr;
}

lirc_action* cfg_actions::get_lactionbyactionname( const char *szaction )
{
	int naction = 0;

	while ( act_table[naction].action != "" ) {
		if ( strcmp( act_table[naction].action.c_str(), szaction ) == 0 ) {
			break;
		}
		++naction;
	}

	for ( size_t i = 0; i < lactions.get_count(); i++ ) {
		lirc_action* action_ptr = lactions[ i ];
		if ( action_ptr ) {
			if ( action_ptr->naction == naction ) {
				return action_ptr;
			}
		}
	}

	return nullptr;
}

lirc_action* cfg_actions::get_lactionbyremotekey( const char *szkn, const char *szrn )
{
	lirc_action* action_ptr = nullptr;

	unsigned int count = 0;

	lirc_action** laction_keypptr = get_lactionbykey( szkn, &count );
	if ( count != 0 ) {
		for ( unsigned int i = 0; i < count; i++ ) {
			if ( strcmp( laction_keypptr[i]->rn.c_str(), szrn ) == 0 ) {
				action_ptr = laction_keypptr[i];
				::free( (void*)laction_keypptr );

				return action_ptr;

				/////////////////////////////////////////////////////////////////////
				// same key, same remote control...
				// return it...
				/////////////////////////////////////////////////////////////////////
			}
		}
		::free( (void*)laction_keypptr );
	}
	return action_ptr;
}


lirc_action** cfg_actions::get_lactionbykey( const char* szkey, unsigned int* ncount )
{
	unsigned int count = 0;
	lirc_action** laction_keypptr = 0;

	for ( int i = 0; i < (int)lactions.get_count(); i++ ) {
		lirc_action* action_ptr = lactions[ i ];
		if ( action_ptr ) {
			if ( strcmp( action_ptr->kn.c_str(), szkey ) == 0 ) {
				count++;
				laction_keypptr = (lirc_action**)::realloc( (void*)laction_keypptr, sizeof( lirc_action* ) * count );
				laction_keypptr[count - 1] = action_ptr;
				(*ncount) = count;
			}
		}							
	}

	return laction_keypptr;
}

lirc_action* cfg_actions::get_lactionbylactionorder( int nindex )
{
	if ( ( nindex >= 0 ) && ( nindex < (int)lactions.get_count() ) )
		return lactions[ nindex ];
	return nullptr;
}

size_t cfg_actions::get_lactionscount()
{
	return lactions.get_count();
}

void cfg_actions::delete_lactionbyindex( int nactionindex )
{
	for ( size_t i = 0; i < lactions.get_count(); i++ ) {
		lirc_action* action_ptr = lactions[ i ];
		if ( action_ptr ) {
			if ( action_ptr->naction == nactionindex ) {
				delete action_ptr;
				lactions.delete_by_idx( i );
				break;
			}
		}
	}
}

bool cfg_actions::assignaction_to_remotekey( const char * szaction, const char * szkn, const char * szrn )
{
	int nactionindex = 0;

	bool brr = false;

	while ( act_table[nactionindex].action != "" ) {
		if ( strcmp( act_table[nactionindex].action.c_str(), szaction ) == 0 ) {
			brr = true;
			break;
		}
		++nactionindex;
	}

	if ( brr == false ) return false;

	return assignaction_to_remotekey(nactionindex, szkn, szrn );
}

bool cfg_actions::assignaction_to_remotekey( int nactionindex, const char * szkn, const char * szrn )
{
	lirc_action* action_ptr = get_lactionbyremotekey( szkn, szrn );

	// possible same key, same remote control with different action
	if ( action_ptr != nullptr ) {
		//////////////////////////////////////////////////////////
		// assingn key on existing action
		//////////////////////////////////////////////////////////
		action_ptr->kn = szkn;
		action_ptr->rn = szrn;
		action_ptr->naction = nactionindex;

		//////////////////////////////////////////////////////////
		// TODO:
		// Check Remote Name...
		// We have no lirc server with this supported...
		//////////////////////////////////////////////////////////
		
	} else {
		//////////////////////////////////////////////////////////
		// assingn key on new action
		//////////////////////////////////////////////////////////
		action_ptr = new lirc_action();
		if ( !action_ptr ) return false;

		action_ptr->kn = szkn;
		action_ptr->rn = szrn;
		action_ptr->naction = nactionindex;

		lactions.add_item( action_ptr );
	}

	return true;
}

void cfg_actions::delete_all()
{
	for ( size_t i = 0; i < lactions.get_count(); i++ ) {
		if ( lactions[i] ) {
			delete lactions[i];
		}
	}
	lactions.delete_all();
}

static const GUID g_mainmenu_group_popup_id = { 0x4edb3e7f, 0x4b2a, 0x4588, { 0xaa, 0x17, 0xa8, 0x64, 0x18, 0xcb, 0x8a, 0xa5 } };
static mainmenu_group_popup_factory g_mainmenu_group_popup(g_mainmenu_group_popup_id, mainmenu_groups::playback, mainmenu_commands::sort_priority_last, "LIRC Daemon client Configure");

class wdevmainmenu_commands : public mainmenu_commands {
public:
	t_uint32 get_command_count() {
		return 1;
	}
	GUID get_command(t_uint32 p_index) {
		static const GUID guid_config = { 0x4edb3e7f, 0x00, 0x4c7c, { 0xad, 0xe8, 0x43, 0xd8, 0x44, 0xbe, 0xce, 0xa8 } };		
		return guid_config;
	}
	void get_name(t_uint32 p_index, pfc::string_base & p_out) {
		p_out = "LIRC Daemon client Configure";
	}
	bool get_description(t_uint32 p_index,pfc::string_base & p_out) {
		p_out = "foobar2000 LIRC Daemon client Configure";
		return true;
	}
	GUID get_parent() {
		return mainmenu_groups::playback;
	}
	void execute(t_uint32 p_index,service_ptr_t<service_base> p_callback) {
		wdevwinlircconfig_dialogshow( p_index );
	}
};

static service_factory_single_t<wdevmainmenu_commands> g_mainmenu_commands_factory;
