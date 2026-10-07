/*
 * head_unit: wind page. Draws the latest wind view on the display; the
 * UI assets (gui.xml, fonts) are read from the SD card.
 */
#include <cstdlib>
#include <cstring>
#include <filesystem>

#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/util.h>

#include <grvl/platform/ZephyrApp.h>

#include <grvl/grvl.h>
#include <grvl/Manager.h>
#include <grvl/component/Label.h>

extern "C" {
#include "wind_state.h"
}

LOG_MODULE_REGISTER(wind_ui, CONFIG_APP_LOG_LEVEL);

namespace fs = std::filesystem;

static constexpr auto UI_THREAD_STACK_SIZE = KB(10);
static constexpr auto UI_THREAD_PRIORITY = 1;
static constexpr auto FRAME_PERIOD_MS = 100;

/* Label ids in gui.xml, indexed by enum wind_field. */
static const char *const field_label_ids[WIND_FIELD_COUNT] = {
	[WIND_AWS] = "aws",
	[WIND_AWA] = "awa",
	[WIND_TWS] = "tws",
	[WIND_TWD] = "twd",
};

K_TIMER_DEFINE(frame_timer, nullptr, nullptr);

static struct wind_state wind;

/*
 * Where the UI assets come from, as in Antmicro's grvl demo: the SD card on
 * the board, the host's romfs/ directory under native_sim.
 */
#if defined(CONFIG_BOARD_STM32H747I_DISCO)
#include <zephyr/fs/ext2.h>
#include <zephyr/fs/fs.h>

static constexpr auto ROMFS_PATH = "/romfs";

static int mount_romfs()
{
	/* Must outlive the mount. */
	static struct fs_mount_t mount;

	mount.type = FS_EXT2;
	mount.mnt_point = ROMFS_PATH;
	mount.storage_dev = (void *)"SD";
	mount.flags = FS_MOUNT_FLAG_NO_FORMAT | FS_MOUNT_FLAG_READ_ONLY;

	int rc = fs_mount(&mount);

	if (rc) {
		LOG_ERR("Failed to mount SD card (err: %d)", rc);
	}
	return rc;
}

static fs::path romfs_path()
{
	return fs::path(ROMFS_PATH);
}
#endif

#if defined(CONFIG_BOARD_NATIVE_SIM)
static int mount_romfs()
{
	return 0;
}

/* $ROMFS_PATH, or romfs/ in the current directory. */
static fs::path romfs_path()
{
	const char *env = std::getenv("ROMFS_PATH");

	return fs::absolute(env ? fs::path(env) : fs::path("romfs"));
}
#endif

static void load_fonts(const fs::path &romfs, grvl::Manager &manager)
{
	auto font = [&](const char *path) {
		return new grvl::GrvlBakedFont((romfs / path).string().c_str());
	};
	auto mona12 = font("fonts/mona12.gbf");

	manager.AddFontToFontContainer("normal", mona12);
	manager.AddFontToFontContainer("mona12", mona12);
	manager.AddFontToFontContainer("mona16", font("fonts/mona16.gbf"));
}

static void show(grvl::AbstractView &page, const struct wind_view &view)
{
	for (int i = 0; i < WIND_FIELD_COUNT; i++) {
		auto label = static_cast<grvl::Label *>(page.GetElement(field_label_ids[i]));

		label->SetText(view.field[i].text);
	}
	page.GetElement("sensorLost")->SetVisible(view.sensor_lost);
}

/* Logged after a changed view is handed to the display; Renode tests read it. */
static void log_if_changed(const struct wind_view &view)
{
	static char shown[64];
	char line[sizeof(shown)];

	wind_view_log_line(&view, line, sizeof(line));
	if (strcmp(line, shown) != 0) {
		strcpy(shown, line);
		LOG_INF("%s", line);
	}
}

static void ui_thread(void *, void *, void *)
{
	const device *display = DEVICE_DT_GET(DT_CHOSEN(zephyr_display));

	wind_state_init(&wind);

	if (mount_romfs()) {
		return;
	}

	grvl::ZephyrApp app{display};
	grvl::Application::Init(&app);

	auto &manager = grvl::Manager::GetInstance();
	const fs::path romfs = romfs_path();

	load_fonts(romfs, manager);
	manager.BuildFromXML((romfs / "gui.xml").string().c_str());
	manager.InitializationFinished();
	manager.SetActiveScreen("wind", 0);

	auto &page = *manager.GetActiveScreen();

	while (true) {
		k_timer_start(&frame_timer, K_MSEC(FRAME_PERIOD_MS), K_NO_WAIT);

		const struct wind_view view = wind_state_view(&wind, k_uptime_get());

		show(page, view);
		app.Render();
		app.Swap();
		log_if_changed(view);
		app.Poll();

		k_timer_status_sync(&frame_timer);
	}
}

K_THREAD_DEFINE(ui_tid, UI_THREAD_STACK_SIZE, ui_thread, nullptr, nullptr, nullptr,
		UI_THREAD_PRIORITY, 0, 0);
