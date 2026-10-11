// SPDX-License-Identifier: GPL-2.0-only
/*
 * mt7996-download DEVICE IMAGE --i-understand [--wa | --dsp]
 * mt7996-download DEVICE --reset
 *
 * Download one firmware container (format detected: ROM patch or RAM
 * container; --wa/--dsp name the RAM type, default WM), or reset the WiFi
 * subsystem and release the DMA session. Prints one JSON line. The first
 * download enables bus mastering and pins the module until --reset.
 */
#include "client.h"
#include "hw/mt7996/host.h"

static const char *phase_name(uint32_t phase)
{
	static const char *const names[] = {
		"preflight", "discard-events", "init", "scatter", "start",
		"complete", "wait-download", "patch-sem-get", "patch-finish",
		"patch-sem-release",
	};

	return phase < sizeof(names) / sizeof(names[0]) ? names[phase] : "unknown";
}

static const char *container_name(uint32_t container)
{
	if (container == W1700K_SDR_CONTAINER_RAM)
		return "ram";
	if (container == W1700K_SDR_CONTAINER_PATCH)
		return "patch";
	return "none";
}

static const char *ram_name(uint32_t ram_type)
{
	if (ram_type == W1700K_SDR_RAM_WA)
		return "wa";
	if (ram_type == W1700K_SDR_RAM_DSP)
		return "dsp";
	return "wm";
}

static int reset(const char *device)
{
	struct w1700k_sdr_reset r;
	int fd, rc, err;

	/* No ABI check: RESET_MT and its result are unchanged since ABI 3, and
	 * resetting is how an older driver is unloaded before a newer one loads.
	 */
	fd = open(device, O_RDONLY | O_CLOEXEC);
	if (fd < 0) {
		fprintf(stderr, "%s: %s\n", device, strerror(errno));
		return 1;
	}
	memset(&r, 0, sizeof(r));
	rc = ioctl(fd, W1700K_SDR_RESET_MT, &r);
	err = rc ? errno : 0;
	close(fd);
	printf("{\"op\":\"reset\",\"ioctl_rc\":%d,\"errno_text\":\"%s\","
	       "\"was_dma_exposed\":%" PRIu32 ",\"rings_released\":%" PRIu32
	       ",\"stop_observe\":%" PRId32 ",\"fw_state_after\":\"0x%08" PRIx32
	       "\",\"owner_after\":\"0x%08" PRIx32 "\",\"fw_own_stat_after\":%" PRIu32 "}\n",
	       rc, err ? strerror(err) : "", r.was_dma_exposed, r.rings_released,
	       r.stop_observe, r.fw_state_after, r.owner_after,
	       HW_GET(TOP_LPCR_HOST_FW_OWN_STAT, r.owner_after));
	return fflush(stdout) || rc ? 1 : 0;
}

static void *read_image(const char *path, uint32_t *bytes)
{
	void *image = NULL;
	FILE *f = fopen(path, "rb");
	long size;

	if (!f) {
		client_fail(path);
		return NULL;
	}
	if (fseek(f, 0, SEEK_END) || (size = ftell(f)) <= 0 || size > UINT32_MAX ||
	    fseek(f, 0, SEEK_SET)) {
		fprintf(stderr, "%s: cannot size the image\n", path);
		goto out;
	}
	image = malloc((size_t)size);
	if (!image) {
		client_fail("malloc");
		goto out;
	}
	if (fread(image, 1, (size_t)size, f) != (size_t)size) {
		fprintf(stderr, "%s: short read\n", path);
		free(image);
		image = NULL;
		goto out;
	}
	*bytes = (uint32_t)size;
out:
	fclose(f);
	return image;
}

static int download(const char *device, const char *path, uint32_t ram_type)
{
	struct w1700k_sdr_download req;
	int fd, rc, err;
	void *image;

	memset(&req, 0, sizeof(req));
	image = read_image(path, &req.bytes);
	if (!image)
		return 1;
	fd = client_open(device, O_RDONLY, W1700K_SDR_CAP_DOWNLOAD);
	if (fd < 0) {
		free(image);
		return 1;
	}
	req.image = (uint64_t)(uintptr_t)image;
	req.ram_type = ram_type;
	rc = ioctl(fd, W1700K_SDR_DOWNLOAD, &req);
	err = rc ? errno : 0;
	close(fd);
	free(image);
	/* The result fields are filled whatever the outcome. */
	printf("{\"op\":\"download\",\"image\":");
	client_json_string(stdout, path);
	printf(",\"bytes\":%" PRIu32 ",\"container\":\"%s\",\"ram_type\":\"%s\","
	       "\"start_sent\":%s,\"sem_get\":%" PRId32 ",\"sem_release\":%" PRId32
	       ",\"ioctl_rc\":%d,\"errno\":%d,\"errno_text\":\"%s\",\"phase\":%" PRIu32
	       ",\"phase_name\":\"%s\",\"executor_result\":%" PRId32 ",\"region\":%" PRIu32
	       ",\"offset\":%" PRIu32 ",\"tx_submitted\":%" PRIu32 ",\"tx_completed\":%" PRIu32
	       ",\"replies\":%" PRIu32 ",\"last_fw_state\":\"0x%08" PRIx32 "\"}\n",
	       req.bytes, container_name(req.container), ram_name(ram_type),
	       req.container == W1700K_SDR_CONTAINER_RAM &&
	       (req.phase == W1700K_SDR_PHASE_START ||
		req.phase == W1700K_SDR_PHASE_COMPLETE) ? "true" : "false",
	       req.sem_get_status, req.sem_release_status, rc, err,
	       err ? strerror(err) : "", req.phase, phase_name(req.phase), req.result,
	       req.region, req.offset, req.tx_submitted, req.tx_completed,
	       req.replies, req.last_fw_state);
	return fflush(stdout) || rc ? 1 : 0;
}

static int usage(const char *me)
{
	fprintf(stderr,
		"usage: %s DEVICE IMAGE --i-understand [--wa | --dsp]\n"
		"       %s DEVICE --reset\n", me, me);
	return 2;
}

int main(int argc, char **argv)
{
	uint32_t ram_type = W1700K_SDR_RAM_WM;
	int understood = 0, i;

	if (argc == 3 && !strcmp(argv[2], "--reset"))
		return reset(argv[1]);
	if (argc < 4)
		return usage(argv[0]);
	for (i = 3; i < argc; i++) {
		if (!strcmp(argv[i], "--i-understand"))
			understood = 1;
		else if (!strcmp(argv[i], "--wa"))
			ram_type = W1700K_SDR_RAM_WA;
		else if (!strcmp(argv[i], "--dsp"))
			ram_type = W1700K_SDR_RAM_DSP;
		else
			return usage(argv[0]);
	}
	if (!understood) {
		fprintf(stderr, "%s: --i-understand is required: a download pins the "
			"module until --reset\n", argv[0]);
		return 2;
	}
	return download(argv[1], argv[2], ram_type);
}
