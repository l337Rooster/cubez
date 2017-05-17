#define __ENGINE_DEBUG__
#include "inc/cubez.h"
#include "inc/table.h"
#include "inc/stack_memory.h"
#include "inc/schema.h"
#include "inc/timer.h"

#define GL3_PROTOTYPES 1

#include <GL/glew.h>
#include <stdio.h>
#include <stdlib.h>
#include <X11/X.h>
#include <X11/Xlib.h>
#include <X11/keysymdef.h>
#include <X11/keysym.h>
#include <GL/gl.h>
#include <GL/glx.h>
#include <GL/glu.h>

#include "physics.h"
#include "player.h"

#include <thread>
#include <unordered_map>

Display                 *dpy;
Window                  root;
GLint                   att[] = { GLX_RGBA, GLX_DEPTH_SIZE, 24, GLX_DOUBLEBUFFER, None };
XVisualInfo             *vi;
Colormap                cmap;
XSetWindowAttributes    swa;
Window                  win;
GLXContext              glc;
XWindowAttributes       gwa;
XEvent                  xev;

void init_rendering(int width, int height) {
  dpy = XOpenDisplay(NULL);

  if(dpy == NULL) {
    printf("\n\tcannot connect to X server\n\n");
    exit(0);
  }

  root = DefaultRootWindow(dpy);

  vi = glXChooseVisual(dpy, 0, att);

  if(vi == NULL) {
    printf("\n\tno appropriate visual found\n\n");
    exit(0);
  } 
  else {
    printf("\n\tvisual %p selected\n", (void *)vi->visualid);
  }

  cmap = XCreateColormap(dpy, root, vi->visual, AllocNone);

  swa.colormap = cmap;
  swa.event_mask = ExposureMask | KeyPressMask;

  win = XCreateWindow(dpy, root, 0, 0, width, height, 0, vi->depth, InputOutput,
                      vi->visual, CWColormap | CWEventMask, &swa);

  XMapWindow(dpy, win);
  XStoreName(dpy, win, "Cubez Example");

  glc = glXCreateContext(dpy, vi, NULL, GL_TRUE);
  glXMakeCurrent(dpy, win, glc);
}

int main(int, char* []) {
  init_rendering(600, 600);

  glewExperimental = GL_TRUE;
  GLenum glewError = glewInit();
  if (glewError != 0) {
    std::cout << "Failed to intialize Glew\n"
      << "Error code: " << glewError;
    return 1;
  }

  const char* vertex_shader =
    "#version 130\n"
    "in vec3 vp;"
    "void main() {"
    "  gl_Position = vec4(vp, 1.0);"
    "}";

  const char* fragment_shader =
    "#version 130\n"
    "out vec4 frag_color;"
    "void main() {"
    "  frag_color = vec4(1.0, 0.0, 1.0, 1.0);"
    "}";

  GLuint vs = glCreateShader(GL_VERTEX_SHADER);
  {
    glShaderSource(vs, 1, &vertex_shader, nullptr);
    glCompileShader(vs);
    GLint success = 0;
    glGetShaderiv(vs, GL_COMPILE_STATUS, &success);
    if (success == GL_FALSE) {
      GLint log_size = 0;
      glGetShaderiv(vs, GL_INFO_LOG_LENGTH, &log_size);
      char* error = new char[log_size];

      glGetShaderInfoLog(vs, log_size, &log_size, error);
      std::cout << "Error in vertex shader compilation: " << error << std::endl;
      delete[] error;
    }
  }

  GLuint fs = glCreateShader(GL_FRAGMENT_SHADER);
  {
    glShaderSource(fs, 1, &fragment_shader, nullptr);
    glCompileShader(fs);
    GLint success = 0;
    glGetShaderiv(fs, GL_COMPILE_STATUS, &success);
    if (success == GL_FALSE) {
      GLint log_size = 0;
      glGetShaderiv(vs, GL_INFO_LOG_LENGTH, &log_size);
      char* error = new char[log_size];

      glGetShaderInfoLog(vs, log_size, &log_size, error);
      std::cout << "Error in fragment shader compilation: " << error << std::endl;
      delete[] error;
    }
  }

  GLuint shader_program = glCreateProgram();
  glAttachShader(shader_program, fs);
  glAttachShader(shader_program, vs);
  glLinkProgram(shader_program);
  glUseProgram(shader_program);

  // Create and initialize the game engine.
  cubez::Universe uni;
  cubez::init(&uni);

  // Create the "main".
  cubez::create_program(kMainProgram); 

  // Create a separate program/pipeline to handle stdout.
  char kStdout[] = "stdout";
  cubez::create_program(kStdout);
  cubez::Pipeline* out = cubez::add_pipeline(kStdout, nullptr, nullptr);
  out->transform = +[](cubez::Frame* f) {
    std::cout << (char*)(f->message.data);
  };
  out->select = nullptr;
  out->callback = nullptr;
  cubez::ExecutionPolicy policy;
  policy.priority = cubez::MAX_PRIORITY;
  policy.trigger = cubez::Trigger::EVENT;
  cubez::enable_pipeline(out, policy);
  
  // Create an event to write to stdout with max buffer of 128 chars.
  char kWriteEvent[] = "write";
  cubez::EventPolicy event_policy;
  event_policy.size = 128;
  cubez::create_event(kStdout, kWriteEvent, event_policy);

  // Subcsribe pipeline handler to stdout.
  cubez::subscribe_to(kStdout, kWriteEvent, out);

  // Open channel to start writing.
  cubez::Channel* std_out = cubez::open_channel(kStdout, kWriteEvent);

  // Write a simple "hi".
  std::string msg = "Hello, world!\n";
  cubez::Message* m = new_message(std_out);
  memcpy(m->data, msg.c_str(), msg.length());
  m->size = msg.length();

  cubez::send_message(m);

  const char kStopEvent[] = "stop_event";
  cubez::create_event(kMainProgram, kStopEvent, { sizeof(int) });

  //cubez::detach_program("stdout");

  {
    physics::Settings settings;
    physics::initialize(settings);
  }
  cubez::Collection* c = physics::get_collection();

  {
    player::Settings settings;
    player::initialize(settings);
  }

  GLuint points_buffer;
  glGenBuffers(1, &points_buffer);
  glBindBuffer(GL_ARRAY_BUFFER, points_buffer);
  glBufferData(
      GL_ARRAY_BUFFER,
      c->count(c) * c->values.size,
      c->values.data(c), GL_DYNAMIC_DRAW);

  cubez::start();
  int frame = 0;
  WindowTimer fps_timer(50);
  WindowTimer update_timer(50);
  WindowTimer render_timer(50);


  XGetWindowAttributes(dpy, win, &gwa);
  glViewport(0, 0, gwa.width, gwa.height);


  double t = 0.0;
  const double dt = 0.01;
  double current_time = Timer::now() * 0.000000001;
  double accumulator = 0.0;

  while (1) {
    fps_timer.start();

    double new_time = Timer::now() * 0.000000001;
    double frame_time = new_time - current_time;
    current_time = new_time;

    accumulator += frame_time;

    update_timer.start();
    while (accumulator >= dt) {
      cubez::loop();
      accumulator -= dt;
      t += dt;
    }
    update_timer.stop();
    update_timer.step();

    if (XCheckWindowEvent(dpy, win, KeyPressMask, &xev)) {
      if (xev.type == Expose) {
      } else if (xev.type == KeyPress) {
        // 0x09 is ESC
        if (xev.xkey.keycode == 0x09) {
          glXMakeCurrent(dpy, None, NULL);
          glXDestroyContext(dpy, glc);
          XDestroyWindow(dpy, win);
          XCloseDisplay(dpy);
          cubez::stop();
          exit(0);
        } else if (XLookupKeysym(&xev.xkey, 0) == XK_w) {
           player::move_up(0.001f);
        } else if (XLookupKeysym(&xev.xkey, 0) == XK_a) {
           player::move_left(0.001f);
        } else if (XLookupKeysym(&xev.xkey, 0) == XK_s) {
           player::move_down(0.001f);
        } else if (XLookupKeysym(&xev.xkey, 0) == XK_d) {
           player::move_right(0.001f);
        } else {
          glm::vec3 p{
            2 * (((float)(rand() % 1000) / 1000.0f) - 0.5f),
            2 * (((float)(rand() % 1000) / 1000.0f) - 0.5f),
            0
          };

          glm::vec3 v{
            ((float)(rand() % 1000) / 100000.0f) - 0.005f,
            ((float)(rand() % 1000) / 100000.0f) - 0.005f,
            0
          };
          physics::create(p, v);
        }
      }
    }

    render_timer.start();
    glBufferData(GL_ARRAY_BUFFER, c->count(c) * c->values.size, nullptr, GL_DYNAMIC_DRAW);
    glBufferSubData(
        GL_ARRAY_BUFFER, 0,
        c->count(c) * c->values.size,
        c->values.data(c));

    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, c->values.size, 0);
    glDrawArrays(GL_POINTS, 0, c->count(c));
    glDisableClientState(GL_VERTEX_ARRAY);

    {
      GLenum error = glGetError();
      if (error) {
        const GLubyte* error_str = gluErrorString(error);
        std::cout << "Error(" << error << "): " << error_str << std::endl;
      }
    }
    glXSwapBuffers(dpy, win);
    render_timer.stop();
    render_timer.step();


    ++frame;
    fps_timer.stop();
    fps_timer.step();

    if (frame % 1000 == 0) {
      double total = 15 * 1e6;
      std::string out = "Frame " + std::to_string(frame) + "\n" +
      + "Utili: "  + std::to_string(100.0 * render_timer.get_avg_elapsed_ns() / total) + " : " + std::to_string(100.0 * update_timer.get_avg_elapsed_ns() / total) + "\n"
      + "Update FPS: " + std::to_string(1e9 / update_timer.get_avg_elapsed_ns()) + "\n"
      + "Render FPS: " + std::to_string(1e9 / render_timer.get_avg_elapsed_ns()) + "\n"
      + "Total FPS: " + std::to_string(1e9 / fps_timer.get_elapsed_ns()) + "\n"
      + "Accum: " + std::to_string(accumulator) + "\n";
  cubez::Message* m = new_message(std_out);
  memcpy(m->data, out.c_str(), out.size() > 128 ? 128 : out.size());
  m->size = out.size();

  cubez::send_message(m);

  /*
      std::cout << "Frame " << frame << std::endl;
      std::cout << "Utilization: "  << 100.0 * render_timer.get_avg_elapsed_ns() / total << " : " << 100.0 * update_timer.get_avg_elapsed_ns() / total << "\n";
      std::cout << "Update FPS: " << 1e9 / update_timer.get_avg_elapsed_ns() << "\n";
      std::cout << "Render FPS: " << 1e9 / render_timer.get_avg_elapsed_ns() << "\n";
      std::cout << "Total FPS: " << 1e9 / fps_timer.get_elapsed_ns() << "\n";
      std::cout << "Accumulator: " << accumulator << "\n";
      std::cout << "\n";
      */
    }
  }
}
