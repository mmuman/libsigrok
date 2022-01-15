/*
 * This file is part of the libsigrok project.
 *
 * Copyright (C) 2021 François Revol <revol@free.fr>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <config.h>
#include "protocol.h"

static struct sr_dev_driver metrix_ox7520_3_driver_info;

static const char *trigger_sources[] = {
	"CH1", "CH2", "EXT"
};

static const uint32_t scanopts[] = {
	SR_CONF_CONN,
	SR_CONF_SERIALCOMM,
};

static const uint32_t drvopts[] = {
	SR_CONF_OSCILLOSCOPE,
};

static const uint32_t devopts[] = {
	SR_CONF_CONN | SR_CONF_GET,
/*
	SR_CONF_CONTINUOUS,
	SR_CONF_DATA_SOURCE, // 2 memories
	SR_CONF_DATALOG,
	SR_CONF_LIMIT_MSEC,
	
	SR_CONF_CONN | SR_CONF_GET,
	SR_CONF_LIMIT_FRAMES | SR_CONF_GET | SR_CONF_SET,
	SR_CONF_TIMEBASE | SR_CONF_GET | SR_CONF_SET | SR_CONF_LIST,
	SR_CONF_NUM_HDIV | SR_CONF_GET,
	SR_CONF_CAPTURE_RATIO | SR_CONF_GET | SR_CONF_SET,
	SR_CONF_TRIGGER_SOURCE | SR_CONF_GET | SR_CONF_SET | SR_CONF_LIST,
	SR_CONF_TRIGGER_SLOPE | SR_CONF_GET | SR_CONF_SET,
	SR_CONF_BUFFERSIZE | SR_CONF_GET | SR_CONF_SET | SR_CONF_LIST,
*/
	SR_CONF_SAMPLERATE | SR_CONF_GET | SR_CONF_LIST,
	SR_CONF_VDIV | SR_CONF_GET,
/*
	SR_CONF_NUM_VDIV | SR_CONF_GET,
	SR_CONF_TRIGGER_LEVEL | SR_CONF_GET | SR_CONF_SET,
*/
};


/* This is the default setting, but it can go up to 19200. */
#define SERIALCOMM "9600/8n1/flow=2"

static const char *supported_ox[] = {
	/* OX7520 lacks the serial port */
	/* OX7520-2 supports sending HPGL capture and printing but no commands */
	"OX7520-3"
};

static GSList *scan(struct sr_dev_driver *di, GSList *options)
{
	struct sr_dev_inst *sdi;
	struct dev_context *devc;
	struct sr_channel *ch;
	struct sr_channel_group *cg;
	struct sr_config *src;
	struct sr_serial_dev_inst *serial;
	GSList *l, *devices;
	int len, i;
	const char *conn, *serialcomm;
	char *buf, **tokens;

	fprintf(stderr, "ox: %s\n", __FUNCTION__);
	devices = NULL;
	conn = serialcomm = NULL;
	for (l = options; l; l = l->next) {
		src = l->data;
		switch (src->key) {
		case SR_CONF_CONN:
			conn = g_variant_get_string(src->data, NULL);
			break;
		case SR_CONF_SERIALCOMM:
			serialcomm = g_variant_get_string(src->data, NULL);
			break;
		}
	}
	if (!conn)
		return NULL;
	if (!serialcomm)
		serialcomm = SERIALCOMM;

	serial = sr_serial_dev_inst_new(conn, serialcomm);

	if (serial_open(serial, SERIAL_RDWR) != SR_OK) {
		sr_err("Unable open serial port.");
		return NULL;
	}

	if (serial_write_blocking(serial, "IDN?", 4, SERIAL_WRITE_TIMEOUT_MS) < 4) {
		sr_err("Unable to send identification string.");
		return NULL;
	}

	len = 128;
	buf = g_malloc(len);
	serial_readline(serial, &buf, &len, 250);
	if (!len)
		return NULL;

	/* strip trailing comma */
	if (buf[len-1] == ',')
		buf[len-1] = '\0';
	tokens = g_strsplit(buf, "   ", 3);
	for (i = 0; tokens[i] != NULL; i++)
		tokens[i] = g_strstrip(tokens[i]);
	if (tokens[0] && tokens[1] && !strcmp("ITT instruments", tokens[1])
			&& tokens[2]) {
		for (i = 0; supported_ox[i]; i++) {
			if (strcmp(supported_ox[i], tokens[0]))
				continue;
			sdi = g_malloc0(sizeof(struct sr_dev_inst));
			sdi->status = SR_ST_INACTIVE;
			sdi->vendor = g_strdup("Metrix");
			sdi->model = g_strdup(tokens[0]);
			sdi->version = g_strdup(tokens[2]);
			devc = g_malloc0(sizeof(struct dev_context));
			//sr_sw_limits_init(&devc->limits);
			sdi->inst_type = SR_INST_SERIAL;
			sdi->conn = serial;
			sdi->priv = devc;

			for (i = 0; i < NUM_CHANNELS; i++) {
				cg = g_malloc0(sizeof(struct sr_channel_group));
				cg->name = g_strdup(trigger_sources[i]);
				ch = sr_channel_new(sdi, i, SR_CHANNEL_ANALOG, FALSE, trigger_sources[i]);
				cg->channels = g_slist_append(cg->channels, ch);
				sdi->channel_groups = g_slist_append(sdi->channel_groups, cg);
			}

			devices = g_slist_append(devices, sdi);
			break;
		}
	}
	g_strfreev(tokens);
	g_free(buf);

	serial_close(serial);
	if (!devices)
		sr_serial_dev_inst_free(serial);

	return std_scan_complete(di, devices);
}

static int dev_open(struct sr_dev_inst *sdi)
{
	(void)sdi;

	/* TODO: get handle from sdi->conn and open it. */
	return std_serial_dev_open(sdi);
//	return SR_OK;
}

static int dev_close(struct sr_dev_inst *sdi)
{
	(void)sdi;

	/* TODO: get handle from sdi->conn and close it. */

	return std_serial_dev_close(sdi);
//	return SR_OK;
}

static int config_get(uint32_t key, GVariant **data,
	const struct sr_dev_inst *sdi, const struct sr_channel_group *cg)
{
	int ret;

	(void)sdi;
	(void)data;
	(void)cg;

	ret = SR_OK;
	switch (key) {
	/* TODO */
	default:
		return SR_ERR_NA;
	}

	return ret;
}

static int config_set(uint32_t key, GVariant *data,
	const struct sr_dev_inst *sdi, const struct sr_channel_group *cg)
{
	int ret;

	(void)sdi;
	(void)data;
	(void)cg;

	ret = SR_OK;
	switch (key) {
	/* TODO */
	default:
		ret = SR_ERR_NA;
	}

	return ret;
}

static int config_list(uint32_t key, GVariant **data,
	const struct sr_dev_inst *sdi, const struct sr_channel_group *cg)
{
	int ret;

	ret = SR_OK;
	switch (key) {
	case SR_CONF_SCAN_OPTIONS:
	case SR_CONF_DEVICE_OPTIONS:
		return STD_CONFIG_LIST(key, data, sdi, cg, scanopts, drvopts, devopts);
	case SR_CONF_SAMPLERATE:
	case SR_CONF_DATA_SOURCE:
	default:
		return SR_ERR_NA;
	}

	return ret;
}

static int dev_acquisition_start(const struct sr_dev_inst *sdi)
{
	/* TODO: configure hardware, reset acquisition state, set up
	 * callbacks and send header packet. */

	(void)sdi;

	return SR_OK;
}

static int dev_acquisition_stop(struct sr_dev_inst *sdi)
{
	/* TODO: stop acquisition. */

	(void)sdi;

	return SR_OK;
}

static struct sr_dev_driver metrix_ox7520_3_driver_info = {
	.name = "metrix-ox7520-3",
	.longname = "Metrix OX7520-3",
	.api_version = 1,
	.init = std_init,
	.cleanup = std_cleanup,
	.scan = scan,
	.dev_list = std_dev_list,
	.dev_clear = std_dev_clear,
	.config_get = config_get,
	.config_set = config_set,
	.config_list = config_list,
	.dev_open = dev_open,
	.dev_close = dev_close,
	.dev_acquisition_start = dev_acquisition_start,
	.dev_acquisition_stop = dev_acquisition_stop,
	.context = NULL,
};
SR_REGISTER_DEV_DRIVER(metrix_ox7520_3_driver_info);
