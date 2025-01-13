#pragma once

#ifndef __FOO_LIRC_H__
#define __FOO_LIRC_H__

#include "../SDK/foobar2000.h"
#include "../helpers/helpers.h"

#define WM_REMOTE_KEY		WM_USER + 0xFFFF
#define WM_TIMER_EXECUTOR   WM_USER + 0xFF00

struct lirc_info
{
	unsigned __int64    kc;  // printf("keycode: 0x%I64X\n", kc);
	int                 rc;  // printf("repeat count: 0x%X\n", rc);
	pfc::string         kn;  // printf("keyname: '%s'\n", kn);
	pfc::string         rn;  // printf("remotename: '%s'\n", rn);
};

class lirc_action
{
	public:
		lirc_action()
		{
			kc = 0;			// printf("keycode: 0x%I64X\n", kc);
			rc = 0;			// printf("repeat count: 0x%X\n", rc);
			kn = "";		// printf("keyname: '%s'\n", kn);
			rn = "";		// printf("keyname: '%s'\n", kn);
			naction = -1;	// printf("action: '%d'\n", naction);
		}

        lirc_action(unsigned __int64 _kc, int _rc, pfc::string _kn, pfc::string _rn, int _naction )
		{
			kc = _kc;
			rc = _rc;
			kn = _kn;
			rn = _rn;
			naction = _naction;
		}

		lirc_action( const lirc_action& _la )
		{
			kc = _la.kc;
			rc = _la.rc;
			kn = _la.kn;
			rn = _la.rn;
			naction = _la.naction;
		}
				
		inline void operator=( const lirc_action& _la ) 
		{
			kc = _la.kc;
			rc = _la.rc;
			kn = _la.kn;
			rn = _la.rn;
			naction = _la.naction;
		}

		unsigned __int64    kc;
	    int                 rc;
	    pfc::string         kn;
	    pfc::string         rn;
		int					naction;
};

class cfg_actions : public cfg_var
{
private:
	pfc::ptr_list_t<lirc_action> lactions;
		
public:
	explicit inline cfg_actions(const GUID & p_guid);

	virtual void get_data_raw(stream_writer* p_stream, abort_callback& p_abort);
	virtual void set_data_raw(stream_reader* p_stream, t_size p_sizehint, abort_callback& p_abort);

	lirc_action* get_lactionbyactionindex( int nactionindex );
	lirc_action* get_lactionbyactionname( const char *szaction );
	lirc_action* get_lactionbylactionorder( int nindex );

	lirc_action** get_lactionbykey( const char *szkey, unsigned int* ncount );
	lirc_action* get_lactionbyremotekey( const char *szkey, const char *szremote );

	size_t get_lactionscount();

	bool assignaction_to_remotekey( const char * szaction, const char * szkn, const char * szrn );
	bool assignaction_to_remotekey( int nactionindex, const char * szkn, const char * szrn );

	void delete_lactionbyindex( int naction );
	void delete_all();

	inline const cfg_actions & operator=(const cfg_actions & p_val) { lactions = p_val.lactions; return *this; }
	inline pfc::ptr_list_t<lirc_action> operator=(pfc::ptr_list_t<lirc_action> p_val) { lactions = p_val; return lactions; }

	inline operator pfc::ptr_list_t<lirc_action>() const { return lactions; }

	inline pfc::ptr_list_t<lirc_action> get_value() const { return lactions; }
};

struct fb2kaction
{
	pfc::string	action;
	pfc::string	description;
};

DWORD WINAPI wdevlircthread_callback(LPVOID param);

void update_event(unsigned int _event);
void update_state(unsigned int _state);

// void exec_action( const char* szaction );

#endif // __FOO_LIRC_H__

