/*
Copyright (C) 2015 by Bernd Buschinski <b.buschinski@gmail.com>

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
*/

#include "jack-wrapper.h"

#include <obs-module.h>

#define JACK_INPUT_MAX_PORTS 8

static const char* const channel_ports_property_names[JACK_INPUT_MAX_PORTS] = {
   "ports connected to channel 1",
   "ports connected to channel 2",
   "ports connected to channel 3",
   "ports connected to channel 4",
   "ports connected to channel 5",
   "ports connected to channel 6",
   "ports connected to channel 7",
   "ports connected to channel 8"
};

/**
 * Returns the name of the plugin
 */
static const char *jack_input_getname(void *unused)
{
	UNUSED_PARAMETER(unused);
	return obs_module_text("JACKInput");
}

/**
 * Destroy the plugin object and free all memory
 */
static void jack_destroy(void *vptr)
{
	struct jack_data *data = (struct jack_data *)vptr;

	if (!data)
		return;

	deactivate_jack(data);

	if (data->device)
		bfree(data->device);
	pthread_mutex_destroy(&data->jack_mutex);
	bfree(data);
}

/**
 * Update the input settings
 */
static void jack_update(void *vptr, obs_data_t *settings)
{
	struct jack_data *data = (struct jack_data *)vptr;
	if (!data)
		return;

	const char *new_device;
	bool settings_changed = false;
	bool new_jack_start_server = obs_data_get_bool(settings, "startjack");
	int new_channel_count = obs_data_get_int(settings, "channels");

	if (new_jack_start_server != data->start_jack_server) {
		data->start_jack_server = new_jack_start_server;
		settings_changed = true;
	}

	if (new_channel_count != data->channels)
		/*
		 * keep "old" channel count  for now,
		 * we need to destroy the correct number of channels
		 */
		settings_changed = true;

	new_device = obs_source_get_name(data->source);
	if (!data->device || strcmp(data->device, new_device) != 0) {
		if (data->device)
			bfree(data->device);
		data->device = bstrdup(new_device);
		settings_changed = true;
	}

	if (settings_changed) {
		deactivate_jack(data);

		data->channels = new_channel_count;

		if (jack_init(data) != 0) {
			deactivate_jack(data);
		}
	}
}

/**
 * Create the plugin object
 */
static void *jack_create(obs_data_t *settings, obs_source_t *source)
{
	struct jack_data *data = bzalloc(sizeof(struct jack_data));

	pthread_mutex_init(&data->jack_mutex, NULL);
	data->source = source;
	data->channels = -1;

	jack_update(data, settings);

	if (data->jack_client == NULL) {
		jack_destroy(data);
		return NULL;
	}
	return data;
}

/**
 * Get plugin defaults
 */
static void jack_input_defaults(obs_data_t *settings)
{
	obs_data_set_default_int(settings, "channels", 2);
	obs_data_set_default_bool(settings, "startjack", false);
}

/**
 * Toggle visibility of channel port connections when channel count changes
 */
static bool jack_input_channel_count_changed(obs_properties_t *props,
      obs_property_t *p, obs_data_t *settings)
{
	UNUSED_PARAMETER(p);
	const int channels     = obs_data_get_int(settings, "channels");

	// display port properties for all existing channels
	for (int i = 0; i < channels; ++i) {
		obs_property_t *channel_ports_property = obs_properties_get(props, channel_ports_property_names[i]);
		obs_property_set_visible(channel_ports_property, true);
		obs_property_modified(channel_ports_property, settings);
	}

	// hide port properties for the reamining non-existing channels
	for (int i = channels; i < JACK_INPUT_MAX_PORTS; ++i) {
		obs_property_t *channel_ports_property = obs_properties_get(props, channel_ports_property_names[i]);
		obs_property_set_visible(channel_ports_property, false);
		obs_property_modified(channel_ports_property, settings);
	}

	return true;
}

/**
 * Get plugin properties
 */
static obs_properties_t *jack_input_properties(void *vptr)
{
	struct jack_data* data = (struct jack_data*)vptr;

	obs_properties_t *props = obs_properties_create();

	obs_properties_add_int(props, "channels", obs_module_text("Channels"),
			       1, 8, 1);
	obs_property_t *channels = obs_properties_add_int(props, "channels",
		obs_module_text("Channels"), 1, JACK_INPUT_MAX_PORTS, 1);

	obs_property_set_modified_callback(channels, jack_input_channel_count_changed);
//	obs_property_set_modified_callback(server, xshm_server_changed);

	obs_properties_add_bool(props, "startjack",
				obs_module_text("StartJACKServer"));

	pthread_mutex_lock(&data->jack_mutex);

//	if (data->channels > 0 && data->jack_ports != NULL) {
//		for (int i = 0; i < data->channels; ++i) {

	for (int i = 0; i < JACK_INPUT_MAX_PORTS; ++i) {
			obs_properties_add_editable_list(props, channel_ports_property_names[i], channel_ports_property_names[i], OBS_EDITABLE_LIST_TYPE_STRINGS, NULL, NULL);

	}

	pthread_mutex_unlock(&data->jack_mutex);

	return props;
}

struct obs_source_info jack_output_capture = {
	.id = "jack_output_capture",
	.type = OBS_SOURCE_TYPE_INPUT,
	.output_flags = OBS_SOURCE_AUDIO,
	.get_name = jack_input_getname,
	.create = jack_create,
	.destroy = jack_destroy,
	.update = jack_update,
	.get_defaults = jack_input_defaults,
	.get_properties = jack_input_properties,
};
