/******************************************************************************
    Copyright (C) 2013-2014 by Hugh Bailey <obs.jim@gmail.com>

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
******************************************************************************/

#pragma once

#include "util/c99defs.h"
#include "util/darray.h"
#include "util/circlebuf.h"
#include "util/dstr.h"
#include "util/threading.h"
#include "util/platform.h"
#include "util/profiler.h"
#include "callback/signal.h"
#include "callback/proc.h"

#include "graphics/graphics.h"

#include "media-io/audio-resampler.h"
#include "media-io/video-io.h"
#include "media-io/audio-io.h"

#include "obs.h"

#define NUM_TEXTURES 2
#define MICROSECOND_DEN 1000000

static inline int64_t packet_dts_usec(struct encoder_packet *packet)
{
	return packet->dts * MICROSECOND_DEN / packet->timebase_den;
}

struct draw_callback {
	void (*draw)(void *param, uint32_t cx, uint32_t cy);
	void *param;
};

/* ------------------------------------------------------------------------- */
/* validity checks */

static inline bool obs_object_valid(const void *obj, const char *f,
		const char *t)
{
	if (!obj) {
		blog(LOG_WARNING, "Null %s passed to %s!", t, f);
		return false;
	}

	return true;
}

static inline bool obs_source_valid(const obs_source_t *obj, const char *f)
{
	return obs_object_valid(obj, f, "source");
}

static inline bool obs_output_valid(const obs_output_t *obj, const char *f)
{
	return obs_object_valid(obj, f, "output");
}

static inline bool obs_encoder_valid(const obs_encoder_t *obj, const char *f)
{
	return obs_object_valid(obj, f, "encoder");
}

static inline bool obs_service_valid(const obs_service_t *obj, const char *f)
{
	return obs_object_valid(obj, f, "service");
}

/* ------------------------------------------------------------------------- */
/* modules */

struct obs_module {
	char *mod_name;
	const char *file;
	char *bin_path;
	char *data_path;
	void *module;
	bool loaded;

	bool        (*load)(void);
	void        (*unload)(void);
	void        (*set_locale)(const char *locale);
	void        (*free_locale)(void);
	uint32_t    (*ver)(void);
	void        (*set_pointer)(obs_module_t *module);
	const char *(*name)(void);
	const char *(*description)(void);
	const char *(*author)(void);

	struct obs_module *next;
};

extern void free_module(struct obs_module *mod);

struct obs_module_path {
	char *bin;
	char *data;
};

static inline void free_module_path(struct obs_module_path *omp)
{
	if (omp) {
		bfree(omp->bin);
		bfree(omp->data);
	}
}

static inline bool check_path(const char *data, const char *path,
		struct dstr *output)
{
	dstr_copy(output, path);
	dstr_cat(output, data);

	return os_file_exists(output->array);
}


/* ------------------------------------------------------------------------- */
/* hotkeys */

struct obs_hotkey {
	obs_hotkey_id               id;
	char                        *name;
	char                        *description;

	obs_hotkey_func             func;
	void                        *data;
	int                         pressed;

	obs_hotkey_registerer_t     registerer_type;
	void                        *registerer;

	obs_hotkey_id               pair_partner_id;
};

struct obs_hotkey_pair {
	obs_hotkey_pair_id          pair_id;
	obs_hotkey_id               id[2];
	obs_hotkey_active_func      func[2];
	bool                        pressed0 : 1;
	bool                        pressed1 : 1;
	void                        *data[2];
};

typedef struct obs_hotkey_pair obs_hotkey_pair_t;

typedef struct obs_hotkeys_platform obs_hotkeys_platform_t;

void *obs_hotkey_thread(void *param);

struct obs_core_hotkeys;
bool obs_hotkeys_platform_init(struct obs_core_hotkeys *hotkeys);
void obs_hotkeys_platform_free(struct obs_core_hotkeys *hotkeys);
bool obs_hotkeys_platform_is_pressed(obs_hotkeys_platform_t *context,
		obs_key_t key);

const char *obs_get_hotkey_translation(obs_key_t key, const char *def);

struct obs_context_data;
void obs_hotkeys_context_release(struct obs_context_data *context);

void obs_hotkeys_free(void);

struct obs_hotkey_binding {
	obs_key_combination_t       key;
	bool                        pressed : 1;
	bool                        modifiers_match : 1;

	obs_hotkey_id               hotkey_id;
	obs_hotkey_t                *hotkey;
};

struct obs_hotkey_name_map;
void obs_hotkey_name_map_free(void);


/* ------------------------------------------------------------------------- */
/* views */

struct obs_view {
	pthread_mutex_t                 channels_mutex;
	obs_source_t                    *channels[MAX_CHANNELS];
};

extern bool obs_view_init(struct obs_view *view);
extern void obs_view_free(struct obs_view *view);


/* ------------------------------------------------------------------------- */
/* displays */

struct obs_display {
	bool                            size_changed;
	bool                            enabled;
	uint32_t                        cx, cy;
	uint32_t                        background_color;
	gs_swapchain_t                  *swap;
	pthread_mutex_t                 draw_callbacks_mutex;
	DARRAY(struct draw_callback)    draw_callbacks;

	struct obs_display              *next;
	struct obs_display              **prev_next;
};

extern bool obs_display_init(struct obs_display *display,
		const struct gs_init_data *graphics_data);
extern void obs_display_free(struct obs_display *display);


/* ------------------------------------------------------------------------- */
/* core */

struct obs_vframe_info {
	uint64_t timestamp;
	int count;
};

struct obs_core_video {
	graphics_t                      *graphics;
	gs_stagesurf_t                  *copy_surfaces[NUM_TEXTURES];
	gs_texture_t                    *render_textures[NUM_TEXTURES];
	gs_texture_t                    *output_textures[NUM_TEXTURES];
	gs_texture_t                    *convert_textures[NUM_TEXTURES];
	bool                            textures_rendered[NUM_TEXTURES];
	bool                            textures_output[NUM_TEXTURES];
	bool                            textures_copied[NUM_TEXTURES];
	bool                            textures_converted[NUM_TEXTURES];
	struct circlebuf                vframe_info_buffer;
	gs_effect_t                     *default_effect;
	gs_effect_t                     *default_rect_effect;
	gs_effect_t                     *opaque_effect;
	gs_effect_t                     *solid_effect;
	gs_effect_t                     *conversion_effect;
	gs_effect_t                     *bicubic_effect;
	gs_effect_t                     *lanczos_effect;
	gs_effect_t                     *bilinear_lowres_effect;
	gs_stagesurf_t                  *mapped_surface;
	int                             cur_texture;

	uint64_t                        video_time;
	video_t                         *video;
	pthread_t                       video_thread;
	bool                            thread_initialized;

	bool                            gpu_conversion;
	const char                      *conversion_tech;
	uint32_t                        conversion_height;
	uint32_t                        plane_offsets[3];
	uint32_t                        plane_sizes[3];
	uint32_t                        plane_linewidth[3];

	uint32_t                        output_width;
	uint32_t                        output_height;
	uint32_t                        base_width;
	uint32_t                        base_height;
	float                           color_matrix[16];
	enum obs_scale_type             scale_type;
};

struct obs_core_audio {
	/* TODO: sound output subsystem */
	audio_t                         *audio;

	float                           user_volume;
	float                           present_volume;
};

/* user sources, output channels, and displays */
struct obs_core_data {
	pthread_mutex_t                 user_sources_mutex;
	DARRAY(struct obs_source*)      user_sources;

	struct obs_source               *first_source;
	struct obs_display              *first_display;
	struct obs_output               *first_output;
	struct obs_encoder              *first_encoder;
	struct obs_service              *first_service;

	pthread_mutex_t                 sources_mutex;
	pthread_mutex_t                 displays_mutex;
	pthread_mutex_t                 outputs_mutex;
	pthread_mutex_t                 encoders_mutex;
	pthread_mutex_t                 services_mutex;

	struct obs_view                 main_view;

	volatile long                   active_transitions;

	long long                       unnamed_index;

	volatile bool                   valid;
};

/* user hotkeys */
struct obs_core_hotkeys {
	pthread_mutex_t                 mutex;
	DARRAY(obs_hotkey_t)            hotkeys;
	obs_hotkey_id                   next_id;
	DARRAY(obs_hotkey_pair_t)       hotkey_pairs;
	obs_hotkey_pair_id              next_pair_id;

	pthread_t                       hotkey_thread;
	bool                            hotkey_thread_initialized;
	os_event_t                      *stop_event;
	bool                            thread_disable_press : 1;
	bool                            strict_modifiers : 1;
	bool                            reroute_hotkeys : 1;
	DARRAY(obs_hotkey_binding_t)    bindings;

	obs_hotkey_callback_router_func router_func;
	void                            *router_func_data;

	obs_hotkeys_platform_t          *platform_context;

	pthread_once_t                  name_map_init_token;
	struct obs_hotkey_name_map      *name_map;

	signal_handler_t                *signals;

	char                            *translations[OBS_KEY_LAST_VALUE];
	char                            *mute;
	char                            *unmute;
	char                            *push_to_mute;
	char                            *push_to_talk;
	char                            *sceneitem_show;
	char                            *sceneitem_hide;
};

struct obs_core {
	struct obs_module               *first_module;
	DARRAY(struct obs_module_path)  module_paths;

	DARRAY(struct obs_source_info)  input_types;
	DARRAY(struct obs_source_info)  filter_types;
	DARRAY(struct obs_source_info)  transition_types;
	DARRAY(struct obs_output_info)  output_types;
	DARRAY(struct obs_encoder_info) encoder_types;
	DARRAY(struct obs_service_info) service_types;
	DARRAY(struct obs_modal_ui)     modal_ui_callbacks;
	DARRAY(struct obs_modeless_ui)  modeless_ui_callbacks;

	signal_handler_t                *signals;
	proc_handler_t                  *procs;

	char                            *locale;
	char                            *module_config_path;
	bool                            name_store_owned;
	profiler_name_store_t           *name_store;

	/* segmented into multiple sub-structures to keep things a bit more
	 * clean and organized */
	struct obs_core_video           video;
	struct obs_core_audio           audio;
	struct obs_core_data            data;
	struct obs_core_hotkeys         hotkeys;
};

extern struct obs_core *obs;

extern void *obs_video_thread(void *param);


/* ------------------------------------------------------------------------- */
/* obs shared context data */

#define OCD_DATA_IDLE 0
#define OCD_DATA_INUSE 1
#define OCD_DATA_DESTROY 2

struct obs_context_data {
	char                            *name;
	void                            *data;
	volatile long                   data_usage_state; /* "data" means "data" member. currently only used in do_encode() and remove_connection() */
	obs_data_t                      *settings;
	signal_handler_t                *signals;
	proc_handler_t                  *procs;

	DARRAY(obs_hotkey_id)           hotkeys;
	DARRAY(obs_hotkey_pair_id)      hotkey_pairs;
	obs_data_t                      *hotkey_data;

	DARRAY(char*)                   rename_cache;
	pthread_mutex_t                 rename_cache_mutex;

	pthread_mutex_t                 *mutex;
	struct obs_context_data         *next;
	struct obs_context_data         **prev_next;
};

extern bool obs_context_data_init(
		struct obs_context_data *context,
		obs_data_t              *settings,
		const char              *name,
		obs_data_t              *hotkey_data);
extern void obs_context_data_free(struct obs_context_data *context);

extern void obs_context_data_insert(struct obs_context_data *context,
		pthread_mutex_t *mutex, void *first);
extern void obs_context_data_remove(struct obs_context_data *context);

extern void obs_context_data_setname(struct obs_context_data *context,
		const char *name);


/* ------------------------------------------------------------------------- */
/* ref-counting  */

struct obs_weak_ref {
	volatile long refs;
	volatile long weak_refs;
};

static inline void obs_ref_addref(struct obs_weak_ref *ref)
{
	os_atomic_inc_long(&ref->refs);
}

static inline bool obs_ref_release(struct obs_weak_ref *ref)
{
	return os_atomic_dec_long(&ref->refs) == -1;
}

static inline void obs_weak_ref_addref(struct obs_weak_ref *ref)
{
	os_atomic_inc_long(&ref->weak_refs);
}

static inline bool obs_weak_ref_release(struct obs_weak_ref *ref)
{
	return os_atomic_dec_long(&ref->weak_refs) == -1;
}

static inline bool obs_weak_ref_get_ref(struct obs_weak_ref *ref)
{
	long owners = ref->refs;
	while (owners > -1) {
		if (os_atomic_compare_swap_long(&ref->refs, owners, owners + 1))
			return true;

		owners = ref->refs;
	}

	return false;
}


/* ------------------------------------------------------------------------- */
/* sources  */

struct async_frame {
	struct obs_source_frame *frame;
	long unused_count;
	bool used;
};

struct obs_weak_source {
	struct obs_weak_ref ref;
	struct obs_source *source;
};

struct obs_source {
	struct obs_context_data         context;
	struct obs_source_info          info;
	struct obs_weak_source          *control;

	/* general exposed flags that can be set for the source */
	uint32_t                        flags;
	uint32_t                        default_flags;

	/* indicates ownership of the info.id buffer */
	bool                            owns_info_id;

	/* signals to call the source update in the video thread */
	bool                            defer_update;

	/* ensures show/hide are only called once */
	volatile long                   show_refs;

	/* ensures activate/deactivate are only called once */
	volatile long                   activate_refs;

	/* used to indicate that the source has been removed and all
	 * references to it should be released (not exactly how I would prefer
	 * to handle things but it's the best option) */
	bool                            removed;

	bool                            active;
	bool                            showing;

	/* used to temporarily disable sources if needed */
	bool                            enabled;

	/* timing (if video is present, is based upon video) */
	volatile bool                   timing_set;
	volatile uint64_t               timing_adjust;
	uint64_t                        next_audio_ts_min;
	uint64_t                        last_frame_ts;
	uint64_t                        last_sys_timestamp;
	bool                            async_rendered;

	/* audio */
	bool                            audio_failed;
	bool                            muted;
	struct resample_info            sample_info;
	audio_resampler_t               *resampler;
	audio_line_t                    *audio_line;
	pthread_mutex_t                 audio_mutex;
	struct obs_audio_data           audio_data;
	size_t                          audio_storage_size;
	float                           base_volume;
	float                           user_volume;
	float                           present_volume;
	int64_t                         sync_offset;

	/* async video data */
	gs_texture_t                    *async_texture;
	gs_texrender_t                  *async_convert_texrender;
	struct obs_source_frame         *cur_async_frame;
	bool                            async_gpu_conversion;
	enum video_format               async_format;
	enum video_format               async_cache_format;
	enum gs_color_format            async_texture_format;
	float                           async_color_matrix[16];
	bool                            async_full_range;
	float                           async_color_range_min[3];
	float                           async_color_range_max[3];
	int                             async_plane_offset[2];
	bool                            async_flip;
	bool                            async_active;
	DARRAY(struct async_frame)      async_cache;
	DARRAY(struct obs_source_frame*)async_frames;
	pthread_mutex_t                 async_mutex;
	uint32_t                        async_width;
	uint32_t                        async_height;
	uint32_t                        async_cache_width;
	uint32_t                        async_cache_height;
	uint32_t                        async_convert_width;
	uint32_t                        async_convert_height;

	/* filters */
	struct obs_source               *filter_parent;
	struct obs_source               *filter_target;
	DARRAY(struct obs_source*)      filters;
	pthread_mutex_t                 filter_mutex;
	gs_texrender_t                  *filter_texrender;
	enum obs_allow_direct_render    allow_direct;
	bool                            rendering_filter;

	/* sources specific hotkeys */
	obs_hotkey_pair_id              mute_unmute_key;
	obs_hotkey_id                   push_to_mute_key;
	obs_hotkey_id                   push_to_talk_key;
	bool                            push_to_mute_enabled : 1;
	bool                            push_to_mute_pressed : 1;
	bool                            push_to_talk_enabled : 1;
	bool                            push_to_talk_pressed : 1;
	uint64_t                        push_to_mute_delay;
	uint64_t                        push_to_mute_stop_time;
	uint64_t                        push_to_talk_delay;
	uint64_t                        push_to_talk_stop_time;

	/* referred by output channel flag */
	bool                            ref_by_output_flag;
};

extern const struct obs_source_info *find_source(struct darray *list,
		const char *id);
extern bool obs_source_init_context(struct obs_source *source,
		obs_data_t *settings, const char *name,
		obs_data_t *hotkey_data);
extern bool obs_source_init(struct obs_source *source,
		const struct obs_source_info *info);

extern void obs_source_destroy(struct obs_source *source);

enum view_type {
	MAIN_VIEW,
	AUX_VIEW
};

extern void obs_source_activate(obs_source_t *source, enum view_type type);
extern void obs_source_deactivate(obs_source_t *source, enum view_type type);
extern void obs_source_video_tick(obs_source_t *source, float seconds);
extern float obs_source_get_target_volume(obs_source_t *source,
		obs_source_t *target);

extern void obs_mark_output_sources(bool always_mark_async_source);

/* ------------------------------------------------------------------------- */
/* outputs  */

enum delay_msg {
	DELAY_MSG_PACKET,
	DELAY_MSG_START,
	DELAY_MSG_STOP,
};

struct delay_data {
	enum delay_msg msg;
	uint64_t ts;
	struct encoder_packet packet;
};

typedef void (*encoded_callback_t)(void *data, struct encoder_packet *packet);

struct obs_weak_output {
	struct obs_weak_ref ref;
	struct obs_output *output;
};

struct obs_output {
	struct obs_context_data         context;
	struct obs_output_info          info;
	struct obs_weak_output          *control;

	/* indicates ownership of the info.id buffer */
	bool                            owns_info_id;

	bool                            received_video;
	bool                            received_audio;
	int64_t                         video_offset;
	int64_t                         audio_offsets[MAX_AUDIO_MIXES];
	int64_t                         highest_audio_ts;
	int64_t                         highest_video_ts;
	pthread_mutex_t                 interleaved_mutex;
	DARRAY(struct encoder_packet)   interleaved_packets;

	int                             reconnect_retry_sec;
	int                             reconnect_retry_max;
	int                             reconnect_retries;
	int                             reconnect_retry_cur_sec;
	bool                            reconnecting;
	pthread_t                       reconnect_thread;
	os_event_t                      *reconnect_stop_event;
	volatile bool                   reconnect_thread_active;

	uint32_t                        starting_frame_count;
	uint32_t                        starting_skipped_frame_count;

	int                             total_frames;

	bool                            active;
	volatile bool                   stopped;
	video_t                         *video;
	audio_t                         *audio;
	obs_encoder_t                   *video_encoder;
	obs_encoder_t                   *audio_encoders[MAX_AUDIO_MIXES];
	obs_service_t                   *service;
	size_t                          mixer_idx;

	uint32_t                        scaled_width;
	uint32_t                        scaled_height;

	bool                            video_conversion_set;
	bool                            audio_conversion_set;
	struct video_scale_info         video_conversion;
	struct audio_convert_info       audio_conversion;

	bool                            valid;

	uint64_t                        active_delay_ns;
	encoded_callback_t              delay_callback;
	struct circlebuf                delay_data; /* struct delay_data */
	pthread_mutex_t                 delay_mutex;
	uint32_t                        delay_sec;
	uint32_t                        delay_flags;
	uint32_t                        delay_cur_flags;
	volatile long                   delay_restart_refs;
	bool                            delay_active;
	bool                            delay_capturing;
};

static inline void do_output_signal(struct obs_output *output,
		const char *signal)
{
	struct calldata params = {0};
	calldata_set_ptr(&params, "output", output);
	signal_handler_signal(output->context.signals, signal, &params);
	calldata_free(&params);
}

extern void process_delay(void *data, struct encoder_packet *packet);
extern void obs_output_cleanup_delay(obs_output_t *output);
extern bool obs_output_delay_start(obs_output_t *output);
extern void obs_output_delay_stop(obs_output_t *output);
extern bool obs_output_actual_start(obs_output_t *output);
extern void obs_output_actual_stop(obs_output_t *output, bool force);

extern const struct obs_output_info *find_output(const char *id);

extern void obs_output_remove_encoder(struct obs_output *output,
		struct obs_encoder *encoder);

void obs_output_destroy(obs_output_t *output);


/* ------------------------------------------------------------------------- */
/* encoders  */

struct obs_weak_encoder {
	struct obs_weak_ref ref;
	struct obs_encoder *encoder;
};

struct encoder_callback {
	bool sent_first_packet;
	void (*new_packet)(void *param, struct encoder_packet *packet);
	void *param;
};

struct obs_encoder {
	struct obs_context_data         context;
	struct obs_encoder_info         info;
	struct obs_weak_encoder         *control;

	uint32_t                        samplerate;
	size_t                          planes;
	size_t                          blocksize;
	size_t                          framesize;
	size_t                          framesize_bytes;

	size_t                          mixer_idx;

	uint32_t                        scaled_width;
	uint32_t                        scaled_height;
	enum video_format               preferred_format;

	bool                            active;

	/* indicates ownership of the info.id buffer */
	bool                            owns_info_id;

	uint32_t                        timebase_num;
	uint32_t                        timebase_den;

	int64_t                         cur_pts;

	struct circlebuf                audio_input_buffer[MAX_AV_PLANES];
	uint8_t                         *audio_output_buffer[MAX_AV_PLANES];

	/* if a video encoder is paired with an audio encoder, make it start
	 * up at the specific timestamp.  if this is the audio encoder,
	 * wait_for_video makes it wait until it's ready to sync up with
	 * video */
	bool                            wait_for_video;
	struct obs_encoder              *paired_encoder;
	uint64_t                        start_ts;

	pthread_mutex_t                 outputs_mutex;
	DARRAY(obs_output_t*)            outputs;

	bool                            destroy_on_stop;

	/* stores the video/audio media output pointer.  video_t *or audio_t **/
	void                            *media;

	pthread_mutex_t                 callbacks_mutex;
	DARRAY(struct encoder_callback) callbacks;

	const char                      *profile_encoder_encode_name;
};

extern struct obs_encoder_info *find_encoder(const char *id);

extern bool obs_encoder_initialize(obs_encoder_t *encoder);
extern void obs_encoder_shutdown(obs_encoder_t *encoder);

extern void obs_encoder_start(obs_encoder_t *encoder,
		void (*new_packet)(void *param, struct encoder_packet *packet),
		void *param);
extern void obs_encoder_stop(obs_encoder_t *encoder,
		void (*new_packet)(void *param, struct encoder_packet *packet),
		void *param);

extern void obs_encoder_add_output(struct obs_encoder *encoder,
		struct obs_output *output);
extern void obs_encoder_remove_output(struct obs_encoder *encoder,
		struct obs_output *output);

void obs_encoder_destroy(obs_encoder_t *encoder);

/* ------------------------------------------------------------------------- */
/* services */

struct obs_weak_service {
	struct obs_weak_ref ref;
	struct obs_service *service;
};

struct obs_service {
	struct obs_context_data         context;
	struct obs_service_info         info;
	struct obs_weak_service         *control;

	/* indicates ownership of the info.id buffer */
	bool                            owns_info_id;

	bool                            active;
	bool                            destroy;
	struct obs_output               *output;
};

extern const struct obs_service_info *find_service(const char *id);

extern void obs_service_activate(struct obs_service *service);
extern void obs_service_deactivate(struct obs_service *service, bool remove);
extern bool obs_service_initialize(struct obs_service *service,
		struct obs_output *output);

void obs_service_destroy(obs_service_t *service);

