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
	bool keepAspectRatio = true;
	bool is_looping = false;

	std::vector<uint32_t> buffer;
	std::unique_ptr<rlottie::Animation> animation;

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

	return properties;
}

static void lottie_source_defaults(obs_data_t *settings)
{
	obs_data_set_default_int(settings, "width", 0);
	obs_data_set_default_int(settings, "height", 0);
	obs_data_set_default_bool(settings, "keepAspectRatio", true);
	obs_data_set_default_bool(settings, "looping", false);
}

static void lottie_source_update(void *data, obs_data_t *settings)
{
	lottie_source *ctx = (lottie_source *)data;

	const char *file = obs_data_get_string(settings, "file");
	size_t width = obs_data_get_int(settings, "width");
	size_t height = obs_data_get_int(settings, "height");

	if (ctx->file.compare(file)) {
		ctx->file = file;
		ctx->animation = rlottie::Animation::loadFromFile(file);
		ctx->frame = 0;

		width = 0;
		height = 0;
	}

	if (!ctx->animation) {
		return;
	}

	ctx->animation->size(ctx->width, ctx->height);

	if (width) {
		ctx->width = width;
	} else {
		obs_data_set_int(settings, "width", ctx->width);
	}

	if (height) {
		ctx->height = height;
	} else {
		obs_data_set_int(settings, "height", ctx->height);
	}

	ctx->keepAspectRatio = obs_data_get_bool(settings, "keepAspectRatio");

	ctx->buffer.resize(ctx->width * ctx->height * 4);

	if (!width || !height) {
		obs_source_update_properties(ctx->source);
	}

	ctx->is_looping = obs_data_get_bool(settings, "looping");
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

static void lottie_source_video_tick(void *data, float seconds)
{
	UNUSED_PARAMETER(seconds);

	lottie_source *ctx = (lottie_source *)data;

	if (!ctx->animation) {
		return;
	}

	if (obs_source_showing(ctx->source)) {
		rlottie::Surface surface(ctx->buffer.data(), ctx->width,
					 ctx->height, ctx->width * 4);

		ctx->animation->renderSync(ctx->frame, surface,
					   ctx->keepAspectRatio);
	}

	if (ctx->frame == ctx->animation->totalFrame()) {
		if (ctx->is_looping) {
			ctx->frame = 0;
		}
	} else {
		++ctx->frame;
	}
}

static void lottie_source_render(void *data, gs_effect_t *effect)
{
	UNUSED_PARAMETER(effect);

	lottie_source *ctx = (lottie_source *)data;

	if (!ctx->animation) {
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

static struct obs_source_info lottie_source_info = {
	.id = "lottie_source",
	.type = OBS_SOURCE_TYPE_INPUT,
	.output_flags = OBS_SOURCE_VIDEO,
	.get_name = lottie_source_get_name,
	.create = lottie_source_create,
	.destroy = lottie_source_destroy,
	.get_width = lottie_source_getwidth,
	.get_height = lottie_source_getheight,
	.get_defaults = lottie_source_defaults,
	.get_properties = lottie_source_properties,
	.update = lottie_source_update,
	.video_tick = lottie_source_video_tick,
	.video_render = lottie_source_render,
	.icon_type = OBS_ICON_TYPE_MEDIA,
};

bool obs_module_load(void)
{
	obs_register_source(&lottie_source_info);

	return true;
}
