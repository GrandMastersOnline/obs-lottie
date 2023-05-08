#include <obs-module.h>
#include <rlottie.h>

#include "plugin-macros.generated.h"

OBS_DECLARE_MODULE()
OBS_MODULE_USE_DEFAULT_LOCALE(PLUGIN_NAME, "en-US")

struct lottie_source {
	obs_source_t *const source;

	std::string file = "";
	size_t width = 0;
	size_t height = 0;
	size_t frame = 0;
	size_t frames = 0;
	bool keepAspectRatio = true;
	bool is_looping = false;
	bool is_clear_on_media_end = true;
	bool restart_on_activate = true;

	enum obs_media_state state = OBS_MEDIA_STATE_NONE;
	std::vector<uint32_t> buffer;
	std::unique_ptr<rlottie::Animation> animation;
	std::unique_ptr<rlottie::Surface> surface;

	lottie_source(obs_source_t *source) : source(source) {}
};

static const char *lottie_source_get_name(void *data)
{
	UNUSED_PARAMETER(data);

	return obs_module_text("Lottie");
}

static const char *lottie_filter = "Lottie Files (*.json);;"
				   "All Files (*.*)";

static obs_properties_t *lottie_source_properties(void *data)
{
	UNUSED_PARAMETER(data);

	obs_properties_t *properties = obs_properties_create();

	obs_properties_add_path(properties, "file", obs_module_text("File"),
				OBS_PATH_FILE, lottie_filter, NULL);
	obs_properties_add_int(properties, "width", obs_module_text("Width"), 0,
			       4096, 1);
	obs_properties_add_int(properties, "height", obs_module_text("Height"),
			       0, 4096, 1);
	obs_properties_add_bool(properties, "keepAspectRatio",
				obs_module_text("Keep Aspect Ratio"));

	obs_properties_add_bool(properties, "looping",
				obs_module_text("Looping"));

	obs_properties_add_bool(properties, "clear_on_media_end",
				obs_module_text("ClearOnMediaEnd"));

	obs_properties_add_bool(properties, "restart_on_activate",
				obs_module_text("RestartWhenActivated"));

	return properties;
}

static void lottie_source_defaults(obs_data_t *settings)
{
	obs_data_set_default_int(settings, "width", 0);
	obs_data_set_default_int(settings, "height", 0);
	obs_data_set_default_bool(settings, "keepAspectRatio", true);
	obs_data_set_default_bool(settings, "looping", false);
	obs_data_set_default_bool(settings, "clear_on_media_end", true);
	obs_data_set_default_bool(settings, "restart_on_activate", true);
}

static void lottie_source_open(struct lottie_source *s)
{
	if (s->file.empty()) {
		return;
	}

	s->animation = rlottie::Animation::loadFromFile(s->file);
	s->frame = 0;
	s->frames = s->animation->totalFrame();
	s->state = OBS_MEDIA_STATE_NONE;

	if (!s->width && !s->height) {
		s->animation->size(s->width, s->height);
	}

	s->buffer.resize(s->width * s->height * 4);
	s->surface = std::unique_ptr<rlottie::Surface>(new rlottie::Surface(
		s->buffer.data(), s->width, s->height, s->width * 4));
}


static void lottie_source_start(struct lottie_source *s)
{
	if (!s->animation) {
		lottie_source_open(s);
	}

	if (!s->animation) {
		return;
	}

	s->state = OBS_MEDIA_STATE_PLAYING;
	obs_source_media_started(s->source);
}

static void lottie_source_update(void *data, obs_data_t *settings)
{
	lottie_source *ctx = (lottie_source *)data;

	ctx->file = obs_data_get_string(settings, "file");;
	ctx->keepAspectRatio = obs_data_get_bool(settings, "keepAspectRatio");
	ctx->width = obs_data_get_int(settings, "width");;
	ctx->height = obs_data_get_int(settings, "height");;

	ctx->is_looping = obs_data_get_bool(settings, "looping");
	ctx->restart_on_activate =
		obs_data_get_bool(settings, "restart_on_activate");
	ctx->is_clear_on_media_end =
		obs_data_get_bool(settings, "clear_on_media_end");

	bool active = obs_source_active(ctx->source);

	if (!ctx->restart_on_activate || active) {
		lottie_source_start(ctx);
	}
}

static void lottie_source_activate(void *data)
{
	lottie_source *ctx = (lottie_source *)data;

	if (ctx->restart_on_activate) {
		obs_source_media_restart(ctx->source);
	}
}

static void lottie_source_deactivate(void *data)
{
	lottie_source *ctx = (lottie_source *)data;

	if (ctx->restart_on_activate) {
		if (ctx->animation) {
			ctx->state = OBS_MEDIA_STATE_ENDED;
			obs_source_media_ended(ctx->source);
		}
	}
}

static void *lottie_source_create(obs_data_t *settings, obs_source_t *source)
{
	lottie_source *ctx = new lottie_source(source);

	lottie_source_update(ctx, settings);

	return ctx;
}

static void lottie_source_destroy(void *data)
{
	lottie_source *ctx = (lottie_source *)data;

	delete ctx;
}

static uint32_t lottie_source_getwidth(void *data)
{
	lottie_source *ctx = (lottie_source *)data;

	return ctx->width;
}

static uint32_t lottie_source_getheight(void *data)
{
	lottie_source *ctx = (lottie_source *)data;

	return ctx->height;
}

static void lottie_source_frame(lottie_source *s)
{
	s->animation->renderSync(s->frame, *s->surface.get(),
				 s->keepAspectRatio);
}

static void lottie_source_video_tick(void *data, float seconds)
{
	UNUSED_PARAMETER(seconds);

	lottie_source *ctx = (lottie_source *)data;

	if (!ctx->animation) {
		return;
	}

	if (ctx->state == OBS_MEDIA_STATE_STOPPED) {
		ctx->state = OBS_MEDIA_STATE_ENDED;
		obs_source_media_ended(ctx->source);
	}

	if (ctx->state != OBS_MEDIA_STATE_PLAYING) {
		return;
	}

	if (ctx->frame > ctx->frames) {

		ctx->state = OBS_MEDIA_STATE_ENDED;
		obs_source_media_ended(ctx->source);
		return;
	}

	lottie_source_frame(ctx);

	if (ctx->frame++ == ctx->frames) {
		if (ctx->is_looping) {
			ctx->frame = 0;
		}
	}
}

static void lottie_source_render(void *data, gs_effect_t *effect)
{
	UNUSED_PARAMETER(effect);

	lottie_source *ctx = (lottie_source *)data;

	if (!ctx->animation) {
		return;
	}

	if (!obs_source_active(ctx->source)) {
		return;
	}

	if (ctx->state == OBS_MEDIA_STATE_STOPPED) {
		return;
	}

	if (ctx->state == OBS_MEDIA_STATE_ENDED && ctx->is_clear_on_media_end) {
		return;
	}

	const uint8_t *buffer[] = {
		reinterpret_cast<const uint8_t *>(ctx->buffer.data()),
	};

	gs_texture_t *texture = gs_texture_create(ctx->width, ctx->height,
						  GS_BGRA, 1, buffer, 0);

	obs_source_draw(texture, 0, 0, 0, 0, false);

	gs_texture_destroy(texture);
}

static void lottie_source_play_pause(void *data, bool pause)
{
	lottie_source *ctx = (lottie_source *)data;

	if (pause) {
		ctx->state = OBS_MEDIA_STATE_PAUSED;
	} else {
		ctx->state = OBS_MEDIA_STATE_PLAYING;
		obs_source_media_started(ctx->source);
	}
}

static void lottie_source_restart(void *data)
{
	lottie_source *ctx = (lottie_source *)data;

	ctx->frame = 0;

	if (obs_source_showing(ctx->source)) {
		lottie_source_start(ctx);
	}

	ctx->state = OBS_MEDIA_STATE_PLAYING;
}

static void lottie_source_stop(void *data)
{
	lottie_source *ctx = (lottie_source *)data;

	ctx->state = OBS_MEDIA_STATE_STOPPED;
}

static void lottie_source_next(void *data)
{
	lottie_source *ctx = (lottie_source *)data;

	ctx->frame = ctx->frames;

	lottie_source_frame(ctx);
}

static void lottie_source_previous(void *data)
{
	lottie_source *ctx = (lottie_source *)data;

	ctx->frame = 0;

	lottie_source_frame(ctx);
}

static enum obs_media_state lottie_source_get_state(void *data)
{
	lottie_source *ctx = (lottie_source *)data;

	return ctx->state;
}

static int64_t lottie_source_get_duration(void *data)
{
	lottie_source *ctx = (lottie_source *)data;

	if (!ctx->animation) {
		return 0;
	}

	return ctx->animation->duration() * 1000;
}

static int64_t lottie_source_get_time(void *data)
{
	lottie_source *ctx = (lottie_source *)data;

	if (!ctx->animation) {
		return 0;
	}

	return ctx->frame / ctx->animation->frameRate() * 1000;
}

static void lottie_source_set_time(void *data, int64_t ms)
{
	lottie_source *ctx = (lottie_source *)data;

	ctx->frame = ctx->animation->frameAtPos(ms / 1000.0 /
						ctx->animation->duration());

	lottie_source_frame(ctx);
}

static struct obs_source_info lottie_source_info = {
	.id = "lottie_source",
	.type = OBS_SOURCE_TYPE_INPUT,
	.output_flags = OBS_SOURCE_VIDEO | OBS_SOURCE_CONTROLLABLE_MEDIA,
	.get_name = lottie_source_get_name,
	.create = lottie_source_create,
	.destroy = lottie_source_destroy,
	.get_width = lottie_source_getwidth,
	.get_height = lottie_source_getheight,
	.get_defaults = lottie_source_defaults,
	.get_properties = lottie_source_properties,
	.update = lottie_source_update,
	.activate = lottie_source_activate,
	.video_tick = lottie_source_video_tick,
	.video_render = lottie_source_render,
	.icon_type = OBS_ICON_TYPE_MEDIA,
	.media_play_pause = lottie_source_play_pause,
	.media_restart = lottie_source_restart,
	.media_stop = lottie_source_stop,
	.media_next = lottie_source_next,
	.media_previous = lottie_source_previous,
	.media_get_duration = lottie_source_get_duration,
	.media_get_time = lottie_source_get_time,
	.media_set_time = lottie_source_set_time,
	.media_get_state = lottie_source_get_state,
};

bool obs_module_load(void)
{
	obs_register_source(&lottie_source_info);

	return true;
}
