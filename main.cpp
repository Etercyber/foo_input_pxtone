/*
  Pxtone, a chiptune / lo-tech music decoder for foobar2000
  Copyright (C) 2005 Studio Pixel
  Copyright (C) 2015 Wilbert Lee

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; either version 3 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

#include <cstdio>
#include <windows.h>
#include <Shlwapi.h>
#include "main.h"

pfc::string8 sz_exist_file, sz_target_file;
p_pxtone_decoder m_p_main, m_p_tag;

void initquit_pxtone::on_init() {
	pfc::string8 sz_temp;

	sz_exist_file = core_api::get_my_full_path();
	sz_exist_file.truncate(sz_exist_file.scan_filename());
	sz_exist_file += DLL_NAME ".dll";
	uGetTempPath(sz_temp);
	uGetTempFileName(sz_temp, pfc::string_filename(sz_exist_file), 0, sz_target_file);

	if (!CopyFile(pfc::stringcvt::string_os_from_utf8(sz_exist_file),
		pfc::stringcvt::string_os_from_utf8(sz_target_file), FALSE))
		throw exception_io();

	m_p_tag = create_decoder(sz_target_file);
	m_p_main = create_decoder(sz_exist_file);

	m_p_tag->ready(nullptr, pxtone_channels,
		pxtone_sample_rate, pxtone_bits_per_sample, 0, FALSE, nullptr);
}

void initquit_pxtone::on_quit() {
	m_p_tag->release();
	destroy_decoder(m_p_tag);
	destroy_decoder(m_p_main);
	DeleteFile(pfc::stringcvt::string_os_from_utf8(sz_target_file));
}

p_pxtone_decoder initquit_pxtone::create_decoder(const char *p_filename) {
	p_pxtone_decoder p_pxtone = new pxtone_decoder();

	p_pxtone->instance = LoadLibraryA(p_filename);
	if (!p_pxtone->instance) SAFE_DELETE(p_pxtone);
	GET_PXTONE_ADDR(ready, PXPROC1, "pxtone_Ready");
	GET_PXTONE_ADDR(reset, PXPROC1, "pxtone_Reset");
	GET_PXTONE_ADDR(release, PXPROC5, "pxtone_Release");
	GET_PXTONE_ADDR(get_direct_sound, PXPROC2, "pxtone_GetDirectSound");
	GET_PXTONE_ADDR(get_last_error, PXPROC3, "pxtone_GetLastError");
	GET_PXTONE_ADDR(get_quality, PXPROC4, "pxtone_GetQuality");
	GET_PXTONE_ADDR(tune_load, PXPROC6, "pxtone_Tune_Load");
	GET_PXTONE_ADDR(tune_read, PXPROC7, "pxtone_Tune_Read");
	GET_PXTONE_ADDR(tune_release, PXPROC5, "pxtone_Tune_Release");
	GET_PXTONE_ADDR(tune_start, PXPROC8, "pxtone_Tune_Start");
	GET_PXTONE_ADDR(tune_fade_out, PXPROC9, "pxtone_Tune_Fadeout");
	GET_PXTONE_ADDR(tune_set_vol, PXPROC10, "pxtone_Tune_SetVolume");
	GET_PXTONE_ADDR(tune_stop, PXPROC11, "pxtone_Tune_Stop");
	GET_PXTONE_ADDR(tune_is_streaming, PXPROC5, "pxtone_Tune_IsStreaming");
	GET_PXTONE_ADDR(tune_set_loop, PXPROC12, "pxtone_Tune_SetLoop");
	GET_PXTONE_ADDR(tune_get_inf, PXPROC13, "pxtone_Tune_GetInformation");
	GET_PXTONE_ADDR(tune_get_repeat, PXPROC11, "pxtone_Tune_GetRepeatMeas");
	GET_PXTONE_ADDR(tune_get_play, PXPROC11, "pxtone_Tune_GetPlayMeas");
	GET_PXTONE_ADDR(tune_get_name, PXPROC14, "pxtone_Tune_GetName");
	GET_PXTONE_ADDR(tune_get_com, PXPROC14, "pxtone_Tune_GetComment");
	GET_PXTONE_ADDR(tune_vomit, PXPROC7, "pxtone_Tune_Vomit");

	return p_pxtone;
}

void initquit_pxtone::destroy_decoder(p_pxtone_decoder p_pxtone) {
	if (p_pxtone == nullptr)
		return;
	if (p_pxtone->instance != nullptr)
		FreeLibrary(p_pxtone->instance);
	SAFE_DELETE(p_pxtone);
}

input_pxtone::input_pxtone() {
	m_file_size = 0;
	m_p_data = nullptr;
}

input_pxtone::~input_pxtone() {
	m_p_main->tune_stop();
	m_p_main->tune_release();
	m_p_main->release();
	if (m_p_data == nullptr)
		return;
	delete[] m_p_data;
}

void input_pxtone::read_custom_data(p_pxtone_decoder p_pxtone,
	t_filesize size, abort_callback &p_abort) {
	m_p_data = new byte[size];
	m_file->read(m_p_data, size, p_abort);
	if (p_pxtone->tune_read(m_p_data, size))
		return;
	p_pxtone->tune_release();
	throw exception_io_data();
}

void input_pxtone::get_info(file_info &p_info, abort_callback &p_abort) {
	p_pxtone_decoder p_module;

	if (m_reason == input_open_info_read)
		p_module = m_p_tag;
	else
		p_module = m_p_main;

	p_module->tune_set_loop(false);
	p_module->tune_get_inf(&m_loop_info.beats,
		&m_loop_info.bpm, &m_loop_info.ticks,
		&m_loop_info.measures);

	m_loop_info.loops = 0;
	m_loop_info.meas_loop = p_module->tune_get_play();
	m_loop_info.meas_intro = p_module->tune_get_repeat();

	p_info.meta_set("title", p_module->tune_get_name());
	p_info.info_set("encoding", "lossless");
	p_info.info_set_int("samplerate", pxtone_sample_rate);
	p_info.info_set_int("channels", pxtone_channels);
	p_info.info_set_int("bitspersample", pxtone_bits_per_sample);
	p_info.set_length(meas2msecs(&m_loop_info, m_loop_info.measures) / 1000);
	p_info.info_set_bitrate((pxtone_bits_per_sample *pxtone_channels *pxtone_sample_rate + 500 /* rounding for bps to kbps*/) / 1000 /* bps to kbps */);

	if (m_reason == input_open_info_read)
		m_p_tag->tune_release();
}

void input_pxtone::open(service_ptr_t<file> p_filehint, const char *p_path,
	t_input_open_reason p_reason, abort_callback &p_abort) {
	m_file = p_filehint;
	m_reason = p_reason;
	m_file_size = m_file->get_size(p_abort);
	input_open_file_helper(m_file, p_path, p_reason, p_abort);

	switch (p_reason) {
	case input_open_info_read:
		read_custom_data(m_p_tag, m_file_size, p_abort);
		break;
	case input_open_decode:
		m_p_main->ready(nullptr, pxtone_channels,
			pxtone_sample_rate, pxtone_bits_per_sample, 0, FALSE, nullptr);
		read_custom_data(m_p_main, m_file_size, p_abort);
		break;
	case input_open_info_write:
		throw exception_io_data();
		break;
	default:
		break;
	}
}

void input_pxtone::decode_initialize(unsigned int p_flags, abort_callback &p_abort) {
	m_file->reopen(p_abort);
	m_p_main->tune_start(0, 0);
}

bool input_pxtone::decode_run(audio_chunk &p_chunk, abort_callback &p_abort) {
	static bool finished;
	static byte buffer[512];

	finished = m_p_main->tune_vomit(buffer, 128);
	p_chunk.set_data_fixedpoint(buffer, 512, pxtone_sample_rate, pxtone_channels, pxtone_bits_per_sample,
		audio_chunk::g_guess_channel_config(pxtone_channels));
	return finished;
}

void input_pxtone::decode_seek(double p_seconds, abort_callback &p_abort) {
	int samples_to_do;

	p_seconds *= 1000;
	samples_to_do = p_seconds * pxtone_sample_rate / 1000;

	if (m_p_main->tune_is_streaming()) m_p_main->tune_stop();
	if (m_loop_info.loops != 0 && p_seconds >= meas2msecs(&m_loop_info, m_loop_info.meas_loop))
	{
		int correct_position = p_seconds;
		correct_position -= meas2msecs(&m_loop_info, m_loop_info.meas_intro);
		correct_position %= meas2msecs(&m_loop_info, m_loop_info.meas_loop - m_loop_info.meas_intro);
		correct_position += meas2msecs(&m_loop_info, m_loop_info.meas_intro);
		samples_to_do = correct_position * pxtone_sample_rate / 1000;
	}

	m_p_main->tune_start(samples_to_do, 0);
}

bool input_pxtone::decode_can_seek() {
	return true;
}

bool input_pxtone::decode_get_dynamic_info(file_info &p_out, double &p_timestamp_delta) {
	return false;
}

void input_pxtone::retag(const file_info &p_info, abort_callback &p_abort) {
}

int input_pxtone::meas2msecs(p_loop_info p_loop, int measures) {
	return (float)measures * (float)p_loop->beats / p_loop->bpm * 60000.0f;
}

bool input_pxtone::g_is_our_path(const char *p_path, const char *p_extension) {
	if (stricmp_utf8(p_extension, "pttune") == 0)
		return true;
	else if (stricmp_utf8(p_extension, "ptcop") == 0)
		return true;
	else
		return false;
}

static initquit_factory_t<initquit_pxtone> g_initquit_pxtone_factory;
static input_singletrack_factory_t<input_pxtone> g_input_pxtone_factory;

DECLARE_COMPONENT_VERSION("Pxtone", "0.9.1.8", \
"Program : Pxtone chiptune / lo-tech music decoder\n"\
"Platform : Intel 80386\n"\
"Programmer : Pxtone Collage (Studio Pixel), Wilbert Lee\n"\
"\n"\
"Pxtone Collage is a chiptune / lo-tech music decoder for Windows\n" \
"\n"\
"The great programmer of Pxtone Collage is Pixel, who is celebrated as the programmer of 'Cave Story'\n" \
"\n"\
"Copyright (C) 2005 Studio Pixel\n"\
"Copyright (C) 2015 Wilbert Lee\n"\
);
DECLARE_FILE_TYPE("Pxtone Sound Format", "*.PTTUNE;*.PTCOP");
// This will prevent users from renaming your component around (important for proper troubleshooter behaviors) or loading multiple instances of it.
VALIDATE_COMPONENT_FILENAME("foo_input_pxtone.dll");
