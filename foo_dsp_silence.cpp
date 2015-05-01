#include "../SDK/foobar2000.h"
#include "../helpers/helpers.h"
#include "resource.h"

#include <vector>

#include <boost/lexical_cast.hpp>

using namespace std;

struct t_dsp_silence_params {
	t_uint32 m_ms_post;
	t_uint32 m_ms_pre;

	bool operator == (t_dsp_silence_params const& rhs) {
		return m_ms_post == rhs.m_ms_post && m_ms_pre == rhs.m_ms_pre;
	}

	bool operator != (t_dsp_silence_params const& rhs) {
		return !(*this == rhs);
	}

	t_dsp_silence_params(t_uint32 p_ms_post = 2000, t_uint32 p_ms_pre = 0)
		: m_ms_post(p_ms_post), m_ms_pre(p_ms_pre)
	{}

	bool set_data(dsp_preset const& p_data) {
		// invalid data
		if (p_data.get_data_size() == 0) return false;

		// <= 0.0.3 data format, just post-track silence
		if (p_data.get_data_size() == sizeof(t_uint32)) {
			std::memcpy(&m_ms_post, p_data.get_data(), sizeof(t_uint32));
			m_ms_pre = 0;
			return true;
		}

		// 0.0.4 data format, post-track and pre-track silence
		if (p_data.get_data_size() == 2*sizeof(t_uint32)) {
			auto* p = (char const*)p_data.get_data();
			std::memcpy(&m_ms_post, p, sizeof(t_uint32));
			p += sizeof(t_uint32);
			std::memcpy(&m_ms_pre, p, sizeof(t_uint32));
			return true;
		}
		return false;
	}

	bool get_data(dsp_preset& p_data) {
		// 0.0.4 data format, post-track and pre-track silence
		t_uint32 d[] = { m_ms_post, m_ms_pre };
		p_data.set_data(d, sizeof(t_uint32)*2);
		return true;
	}
};

class dialog_dsp_silence :
	public dialog_helper::dialog_modal
{
public:
	dialog_dsp_silence(t_dsp_silence_params & p_params) : m_params(p_params) {}

	virtual BOOL on_message(UINT msg, WPARAM wp, LPARAM lp) {
		switch(msg) {
		case WM_INITDIALOG: {
				uSetDlgItemInt(get_wnd(), IDC_MS_PRE, m_params.m_ms_pre, FALSE);
				uSetDlgItemInt(get_wnd(), IDC_MS_POST, m_params.m_ms_post, FALSE);
				update_display();
			}
			break;

		case WM_CLOSE: {
				end_dialog(0);
			}
			break;

		case WM_COMMAND:
			switch(wp) {
			case IDOK: {
					auto get_text_value = [&](int id, t_int32 def) -> t_int32 {
						BOOL translated = FALSE;
						t_int32 ret = uGetDlgItemInt(get_wnd(), id, &translated, FALSE);
						if (translated)
							return ret;
						return def;
					};
					auto restore_if_negative = [&](t_int32 amount, int id, t_int32 old_value) -> t_int32 {
						if (amount < 0) {
							uSetDlgItemInt(get_wnd(), id, old_value, FALSE);
							return old_value;
						}
						return amount;
					};
					auto pre_ms  = get_text_value(IDC_MS_PRE,  -1);
					auto post_ms = get_text_value(IDC_MS_POST, -1);
					m_params.m_ms_pre = restore_if_negative(pre_ms, IDC_MS_PRE, m_params.m_ms_pre);
					m_params.m_ms_post = restore_if_negative(post_ms, IDC_MS_POST, m_params.m_ms_post);
					end_dialog(1);
				}
				break;
			case IDCANCEL: {
					end_dialog(0);
				}
				break;
			}
			break;
		}
		return 0;
	}

private:
	void update_display() {
	}

	t_dsp_silence_params & m_params;
};

class dsp_silence :
	public dsp_impl_base
{
	t_uint32 m_ms_pre;
	t_uint32 m_ms_post;
	t_uint32 m_nch;
	t_uint32 m_chmask;
	t_uint32 m_srate;

	bool m_first_chunk;

public:
	dsp_silence(dsp_preset const& p_preset) {
		t_dsp_silence_params params;
		params.set_data(p_preset);
		m_first_chunk = true;
		m_ms_pre  = params.m_ms_pre;
		m_ms_post = params.m_ms_post;
		m_nch = 0;
		m_chmask = 0;
		m_srate = 0;
	}

	static GUID g_get_guid() {
		// {750FBC09-5337-42d5-9A0F-9E76611D7EC2}
		static const GUID guid = 
		{ 0x750fbc09, 0x5337, 0x42d5, { 0x9a, 0xf, 0x9e, 0x76, 0x61, 0x1d, 0x7e, 0xc2 } };
		return guid;
	}

	static void g_get_name(pfc::string_base & p_out) {
		p_out = "Affix silence";
	}

	static bool g_have_config_popup() {
		return true;
	}

#pragma warning(push)
#pragma warning(disable: 4996)
	static bool g_show_config_popup(dsp_preset const& p_data, HWND p_parent, dsp_preset_edit_callback& cb) {
		t_dsp_silence_params params;
		if(!params.set_data(p_data)) return false;

		t_dsp_silence_params before_change = params;
		dialog_dsp_silence dlg(params);
		if(!dlg.run(IDD_CONFIG, p_parent)) return false;
		if (before_change != params) {
			dsp_preset_impl out;
			out.copy(p_data);
			params.get_data(out);
			cb.on_preset_changed(out);
		}
		return true;
	}
#pragma warning(pop)

	static bool g_get_default_preset(dsp_preset & p_out) {
		return g_get_data(p_out, 2000, 0);
	}

	bool set_data(const dsp_preset & p_data) {
		t_int32 post, pre;
		if(!g_set_data(p_data, post, pre)) return false;
		m_ms_post = post;
		m_ms_pre = pre;
		return true;
	}

	t_uint32 sample_count(float seconds) {
		return (t_uint32)(m_nch * m_srate * seconds);
	}

	void insert_silence_chunk(float seconds) {
		t_uint32 amount = sample_count(seconds);
		if (amount > 0) {
			auto* ac = insert_chunk();
			ac->set_data_size(amount);
			pfc::array_t<audio_sample> silence;
			silence.set_size(amount);
			silence.fill(0.0f);
			ac->set_data(silence.get_ptr(), amount/m_nch, m_nch, m_srate, m_chmask);
		}
	}

	virtual bool on_chunk(audio_chunk * chunk, abort_callback&) {
		if (m_first_chunk) {
			m_nch = chunk->get_channels();
			m_chmask = chunk->get_channel_config();
			m_srate = chunk->get_srate();
			insert_silence_chunk(m_ms_pre/1000.0f);
			m_first_chunk = false;
		}
		return true;
	}

	virtual void on_endoftrack(abort_callback&) {
		insert_silence_chunk(m_ms_post/1000.0f);
		m_first_chunk = true;
	}

	virtual void on_endofplayback(abort_callback&) {}

	virtual void flush() {
	}

	virtual double get_latency() { return 0.0; }

	virtual bool need_track_change_mark() { return true; }

private:
	static bool g_set_data(const dsp_preset & p_data, t_int32 & p_post, t_int32 & p_pre) {
		if(p_data.get_owner() == g_get_guid()) {
			t_dsp_silence_params params;
			if(!params.set_data(p_data)) return false;
			p_post = params.m_ms_post;
			p_pre = params.m_ms_pre;
			return true;
		}
		return false;
	}

	static bool g_get_data(dsp_preset & p_data, t_int32 p_post, t_int32 p_pre) {
		p_data.set_owner(g_get_guid());
		t_dsp_silence_params params(p_post, p_pre);
		return params.get_data(p_data);
	}
};

DECLARE_COMPONENT_VERSION("Post-track silence", "0.0.5", "A DSP for inserting a configurable amount of silence before/after each track.\n" "Zao")
VALIDATE_COMPONENT_FILENAME("foo_dsp_silence.dll")
static dsp_factory_t<dsp_silence> foo_dsp_silence;