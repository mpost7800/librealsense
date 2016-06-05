// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2015 Intel Corporation. All Rights Reserved.

//////////////////////////////////////////////////////////////////////////////////////
// librealsense Multi-threading Demo 1 - dipatching processing to a seperate thread //
//////////////////////////////////////////////////////////////////////////////////////

#include <librealsense/rs.hpp>
#include <cstdio>
#include <GLFW/glfw3.h>
#include "concurrency.hpp"
#include <thread>



int main() try
{
    // In this demo the workload of rendering the frames is split equally between N consumer

    rs::context ctx;
    printf("There are %d connected RealSense devices.\n", ctx.get_device_count());
    if(ctx.get_device_count() == 0) return EXIT_FAILURE;

    rs::device * dev = ctx.get_device(0);
    printf("\nUsing device 0, an %s\n", dev->get_name());
    printf("    Serial number: %s\n", dev->get_serial());
    printf("    Firmware version: %s\n", dev->get_firmware_version());

    // create N queues for the N consumers
    const auto consumers = 3;
	const auto  streams = 3;

    single_consumer_queue<rs::frameset> frames_queue[consumers];
    std::vector<bool> running(consumers, true);
    std::vector<std::pair<int, int>> resolutions(static_cast<int>(rs::stream::depth_aligned_to_infrared2), { 0, 0 });

    glfwInit();
   

    for (auto i = 0; i < consumers; i++)
    {
        // Each consumer will deque 1/N-th of the total frames and present them in its own window
        std::thread consumer([dev, &frames_queue, &running, &resolutions, i]()
        {
            try
            {
                GLFWwindow * win = glfwCreateWindow(1280, 960, "librealsense - multi-threading demo-1", nullptr, nullptr);
                glfwMakeContextCurrent(win);
                while (!glfwWindowShouldClose(win))
                {
                    glfwPollEvents();
                    auto frames = frames_queue[i].dequeue();

                    glClear(GL_COLOR_BUFFER_BIT);
                    glPixelZoom(1, -1);

                    glRasterPos2f(-1, 1);
                    glPixelTransferf(GL_RED_SCALE, 0xFFFF * dev->get_depth_scale() / 2.0f);
                    glDrawPixels(resolutions[static_cast<int>(rs::stream::depth)].first, resolutions[static_cast<int>(rs::stream::depth)].second, GL_RED, GL_UNSIGNED_SHORT, frames.get_frame_data(rs::stream::depth));
                    glPixelTransferf(GL_RED_SCALE, 1.0f);

                    glRasterPos2f(0, 1);
                    glDrawPixels(resolutions[static_cast<int>(rs::stream::color)].first, resolutions[static_cast<int>(rs::stream::color)].second, GL_RGB, GL_UNSIGNED_BYTE, frames.get_frame_data(rs::stream::color));

                    glRasterPos2f(-1, 0);
                    glDrawPixels(resolutions[static_cast<int>(rs::stream::infrared)].first, resolutions[static_cast<int>(rs::stream::infrared)].second, GL_LUMINANCE, GL_UNSIGNED_BYTE, frames.get_frame_data(rs::stream::infrared));

                    if (dev->is_stream_enabled(rs::stream::infrared2))
                    {
                        glRasterPos2f(0, 0);
                        glDrawPixels(resolutions[static_cast<int>(rs::stream::infrared2)].first, resolutions[static_cast<int>(rs::stream::infrared2)].second, GL_LUMINANCE, GL_UNSIGNED_BYTE, frames.get_frame_data(rs::stream::infrared2));
                    }

                    glfwSwapBuffers(win);
                }
            }
            catch (const rs::error & e)
            {
                printf("rs::error was thrown when calling %s(%s):\n", e.get_failed_function().c_str(), e.get_failed_args().c_str());
                printf("    %s\n", e.what());
            }
            running[i] = false;
        });
        consumer.detach();
    }

    dev->enable_stream(rs::stream::depth, 0, 0, rs::format::z16, 60);
    dev->enable_stream(rs::stream::color, 640, 480, rs::format::rgb8, 60);
    dev->enable_stream(rs::stream::infrared, 0, 0, rs::format::y8, 60);
    if (dev->supports(rs::capabilities::infrared2))
    {
        dev->enable_stream(rs::stream::infrared2, 0, 0, rs::format::y8, 60);
    }
    
    dev->start();
    resolutions[static_cast<int>(rs::stream::depth)] = { dev->get_stream_width(rs::stream::depth), dev->get_stream_height(rs::stream::depth) };
    resolutions[static_cast<int>(rs::stream::color)] = { dev->get_stream_width(rs::stream::color), dev->get_stream_height(rs::stream::color) };
    resolutions[static_cast<int>(rs::stream::infrared)] = { dev->get_stream_width(rs::stream::infrared), dev->get_stream_height(rs::stream::infrared) };

    if (dev->is_stream_enabled(rs::stream::infrared2))
        resolutions[static_cast<int>(rs::stream::infrared2)] = { dev->get_stream_width(rs::stream::infrared2), dev->get_stream_height(rs::stream::infrared2) };

    auto counter = 0;
    while (any_costumers_alive(running))
    {
        auto frames = dev->wait_for_frames_safe();
        auto queue_id = counter++ % consumers; // enforce fairness
        if (running[queue_id])
           frames_queue[queue_id].enqueue(std::move(frames)); // pass the frameset for processing to thread queue_id
    }
    for (auto i = 0; i < consumers; i++)
        frames_queue[i].clear();

    dev->stop();

    for (auto i = 0; i < streams; i++)
    {
        if (dev->is_stream_enabled((rs::stream)i))
            dev->disable_stream((rs::stream)i);
    }

    return EXIT_SUCCESS;
}
catch(const rs::error & e)
{
    printf("rs::error was thrown when calling %s(%s):\n", e.get_failed_function().c_str(), e.get_failed_args().c_str());
    printf("    %s\n", e.what());
    return EXIT_FAILURE;
}
