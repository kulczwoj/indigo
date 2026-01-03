// Copyright (c) 2018 CloudMakers, s. r. o.
// All rights reserved.
//
// You can use this software under the terms of 'INDIGO Astronomy
// open-source license' (see LICENSE.md).
//
// THIS SOFTWARE IS PROVIDED BY THE AUTHORS 'AS IS' AND ANY EXPRESS
// OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
// WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
// ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
// DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
// DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
// GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
// INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
// WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
// NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
// SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

// version history
// 2.0 by Peter Polakovic <peter.polakovic@cloudmakers.eu>

/** INDIGO Scheduler agent
 \file indigo_agent_scheduler.c
 */

#define DRIVER_VERSION 0x0001
#define DRIVER_NAME	"indigo_agent_scheduler"

#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <math.h>
#include <assert.h>
#include <pthread.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <netinet/in.h>

#include <indigo/indigo_agent.h>
#include <indigo/indigo_xml.h>
#include <indigo/indigo_filter.h>
#include <indigo/indigo_client.h>

#include "indigo_agent_scheduler.h"

#define DEVICE_PRIVATE_DATA														((scheduler_agent_private_data *)device->private_data)
#define CLIENT_PRIVATE_DATA														((scheduler_agent_private_data *)FILTER_CLIENT_CONTEXT->device->private_data)

typedef struct {
	pthread_mutex_t mutex;
} scheduler_agent_private_data;

// -------------------------------------------------------------------------------- INDIGO agent device implementation

static indigo_result agent_enumerate_properties(indigo_device *device, indigo_client *client, indigo_property *property);

static indigo_result agent_device_attach(indigo_device *device) {
	assert(device != NULL);
	assert(DEVICE_PRIVATE_DATA != NULL);
	if (indigo_filter_device_attach(device, DRIVER_NAME, DRIVER_VERSION, 0) == INDIGO_OK) {
		// -------------------------------------------------------------------------------- Device properties
		// --------------------------------------------------------------------------------
		CONFIG_PROPERTY->hidden = true;
		PROFILE_PROPERTY->hidden = true;
		CONNECTION_PROPERTY->hidden = true;
		ADDITIONAL_INSTANCES_PROPERTY->hidden = DEVICE_CONTEXT->base_device != NULL;
		// --------------------------------------------------------------------------------
		pthread_mutex_init(&DEVICE_PRIVATE_DATA->mutex, NULL);
		indigo_load_properties(device, false);
		INDIGO_DEVICE_ATTACH_LOG(DRIVER_NAME, device->name);
		return agent_enumerate_properties(device, NULL, NULL);
	}
	return INDIGO_FAILED;
}

static indigo_result agent_enumerate_properties(indigo_device *device, indigo_client *client, indigo_property *property) {
	if (client != NULL && client == FILTER_DEVICE_CONTEXT->client)
		return INDIGO_OK;
	return indigo_filter_enumerate_properties(device, client, property);
}

static indigo_result agent_device_detach(indigo_device *device) {
	assert(device != NULL);
	pthread_mutex_destroy(&DEVICE_PRIVATE_DATA->mutex);
	return indigo_filter_device_detach(device);
}

// -------------------------------------------------------------------------------- INDIGO agent client implementation

// -------------------------------------------------------------------------------- Initialization

static scheduler_agent_private_data *private_data = NULL;

static indigo_device *agent_device = NULL;
static indigo_client *agent_client = NULL;

indigo_result indigo_agent_scheduler(indigo_driver_action action, indigo_driver_info *info) {
	static indigo_device agent_device_template = INDIGO_DEVICE_INITIALIZER(
		SCHEDULER_AGENT_NAME,
		agent_device_attach,
		agent_enumerate_properties,
		indigo_filter_change_property,
		NULL,
		agent_device_detach
	);

	static indigo_client agent_client_template = {
		SCHEDULER_AGENT_NAME, false, NULL, INDIGO_OK, INDIGO_VERSION_CURRENT, NULL,
		indigo_filter_client_attach,
		indigo_filter_define_property,
		indigo_filter_update_property,
		indigo_filter_delete_property,
		NULL,
		indigo_filter_client_detach
	};

	static indigo_driver_action last_action = INDIGO_DRIVER_SHUTDOWN;

	SET_DRIVER_INFO(info, SCHEDULER_AGENT_NAME, __FUNCTION__, DRIVER_VERSION, false, last_action);

	if (action == last_action)
		return INDIGO_OK;

	switch(action) {
		case INDIGO_DRIVER_INIT:
			last_action = action;
			private_data = indigo_safe_malloc(sizeof(scheduler_agent_private_data));
			agent_device = indigo_safe_malloc_copy(sizeof(indigo_device), &agent_device_template);
			agent_device->private_data = private_data;
			indigo_attach_device(agent_device);
			agent_client = indigo_safe_malloc_copy(sizeof(indigo_client), &agent_client_template);
			agent_client->client_context = agent_device->device_context;
			indigo_attach_client(agent_client);
			break;

		case INDIGO_DRIVER_SHUTDOWN:
			last_action = action;
			if (agent_client != NULL) {
				indigo_detach_client(agent_client);
				free(agent_client);
				agent_client = NULL;
			}
			if (agent_device != NULL) {
				indigo_detach_device(agent_device);
				free(agent_device);
				agent_device = NULL;
			}
			if (private_data != NULL) {
				free(private_data);
				private_data = NULL;
			}
			break;

		case INDIGO_DRIVER_INFO:
			break;
	}
	return INDIGO_OK;
}
