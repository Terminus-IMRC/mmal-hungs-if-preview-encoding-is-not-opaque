/*
 * Copyright (c) 2017 Sugizaki Yukimasa (ysugi@idein.jp)
 * All rights reserved.
 *
 * This software is licensed under a Modified (3-Clause) BSD License.
 * You should have received a copy of this license along with this
 * software. If not, contact the copyright holder above.
 */

#include <interface/mmal/mmal.h>
#include <interface/mmal/util/mmal_connection.h>
#include <interface/mmal/util/mmal_util_params.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>


#define CAMERA_PREVIEW_PORT 0
#define CAMERA_CAPTURE_PORT 2

/* Formats for the preview port of camera. */
//#define ENCODING_PREVIEW MMAL_ENCODING_OPAQUE
#define ENCODING_PREVIEW MMAL_ENCODING_RGB24
#define WIDTH_PREVIEW  1024
#define HEIGHT_PREVIEW 768

/* Formats for the capture port of camera. */
#define ENCODING_CAPTURE MMAL_ENCODING_OPAQUE
#define WIDTH_CAPTURE  1024
#define HEIGHT_CAPTURE 768

#define CAPTURE 1


#define _check_mmal(x) \
    do { \
        MMAL_STATUS_T status = (x); \
        if (status != MMAL_SUCCESS) { \
            fprintf(stderr, "%s:%d: MMAL call failed: 0x%08x\n", __FILE__, __LINE__, status); \
            exit(EXIT_FAILURE); \
        } \
    } while (0)


static MMAL_STATUS_T config_port(MMAL_PORT_T *port, const MMAL_FOURCC_T encoding, const int width, const int height)
{
    port->format->encoding = encoding;
    port->format->es->video.width  = VCOS_ALIGN_UP(width,  32);
    port->format->es->video.height = VCOS_ALIGN_UP(height, 16);
    port->format->es->video.crop.x = 0;
    port->format->es->video.crop.y = 0;
    port->format->es->video.crop.width  = width;
    port->format->es->video.crop.height = height;
    return mmal_port_format_commit(port);
}

static void callback_control(MMAL_PORT_T *port, MMAL_BUFFER_HEADER_T *buffer)
{
    fprintf(stderr, "%s is called by %s\n", __func__, port->name);
    mmal_buffer_header_release(buffer);
}

static double get_time()
{
    struct timeval t;
    gettimeofday(&t, NULL);
    return (double) t.tv_sec + t.tv_usec * 1e-6;
}

int main()
{
    MMAL_CONNECTION_T *conn_preview_render = NULL, *conn_capture_null = NULL;
    MMAL_COMPONENT_T *cp_camera = NULL, *cp_render = NULL, *cp_null = NULL;
    double start;

    /* Setup the camera component. */
    _check_mmal(mmal_component_create("vc.ril.camera", &cp_camera));
    _check_mmal(mmal_port_enable(cp_camera->control, callback_control));
    _check_mmal(config_port(cp_camera->output[CAMERA_PREVIEW_PORT], ENCODING_PREVIEW, WIDTH_PREVIEW, HEIGHT_PREVIEW));
    _check_mmal(config_port(cp_camera->output[CAMERA_CAPTURE_PORT], ENCODING_CAPTURE, WIDTH_CAPTURE, HEIGHT_CAPTURE));
    _check_mmal(mmal_component_enable(cp_camera));

    /* Setup the render component. */
    _check_mmal(mmal_component_create("vc.ril.video_render", &cp_render));
    _check_mmal(mmal_port_enable(cp_render->control, callback_control));
    _check_mmal(config_port(cp_render->input[0], ENCODING_PREVIEW, WIDTH_PREVIEW, HEIGHT_PREVIEW));
    _check_mmal(mmal_component_enable(cp_render));

    /* Setup the null component. */
    _check_mmal(mmal_component_create("vc.null_sink", &cp_null));
    _check_mmal(mmal_port_enable(cp_null->control, callback_control));
    _check_mmal(config_port(cp_null->input[0], ENCODING_CAPTURE, WIDTH_CAPTURE, HEIGHT_CAPTURE));
    _check_mmal(mmal_component_enable(cp_null));

    /* Connect camera[PREVIEW] -- [0]render. */
    _check_mmal(mmal_connection_create(&conn_preview_render, cp_camera->output[CAMERA_PREVIEW_PORT], cp_render->input[0], MMAL_CONNECTION_FLAG_TUNNELLING));
    _check_mmal(mmal_connection_enable(conn_preview_render));

    /* Connect camera[CAPTURE] -- [0]null. */
    _check_mmal(mmal_connection_create(&conn_capture_null,   cp_camera->output[CAMERA_CAPTURE_PORT], cp_null->input[0],   MMAL_CONNECTION_FLAG_TUNNELLING));
    _check_mmal(mmal_connection_enable(conn_capture_null));

    start = get_time();
    fprintf(stderr, "%f: Setting MMAL_PARAMETER_CAPTURE to %d\n", get_time() - start, CAPTURE);
    _check_mmal(mmal_port_parameter_set_boolean(cp_camera->output[CAMERA_CAPTURE_PORT], MMAL_PARAMETER_CAPTURE, CAPTURE));
    for (; ; ) {
        fprintf(stderr, "%f: Still living\n", get_time() - start);
        vcos_sleep(50);
    }

    return 0;
}
