/* TODO
 * This file needs some *serious* love.
 * It was once a scrappy little proof of concept...
 * But it has become a horrible, incohesive, glob of garbage.
 */

/* System Includes */
#if defined(_WIN32) || defined(_WIN64)
  #include <ws2tcpip.h>
#elif !defined(__APPLE__)
  #include <X11/Xlib.h>
#endif
#include <string>
#include <stdexcept>

/* CEF Includes */
#include "include/cef_app.h"
#include "include/cef_browser.h"
#include "include/cef_command_line.h"
#if defined(_WIN32) || defined(_WIN64)
  #include "include/cef_sandbox_win.h"
#endif

/* APR Includes */
#include "apr_file_io.h"
#include "apr_portable.h"
#include "apr_thread_proc.h"
#include "apr_thread_mutex.h"
#include "apr_network_io.h"
#include "apr_env.h"

/* MRuby Includes */
#include "mruby.h"
#include "mruby/string.h"
#include "mruby/compile.h"

/* Lamina Includes */
#include "mruby_lamina.h"
#include "lamina_util.h"
#include "LaminaHandler.h"
#include "LaminaApp.h"
#include "lamina_opt.h"

using namespace std;

#ifdef __cplusplus
extern "C" {
#endif

using namespace std;

// Notes on sandboxing:
//  - On windows, this currently only works if the sub-process exe is the same exe as the main process
//  - With sandboxing enabled, render processes cannot access system resources like networking/files
//  - Currently running without sandboxing to allow the render process to load mruby scripts
//    + This also allows javascript extensions to access resources without going through IPC
//    + This isn't great from a security perspective. Will need to work around this and re-enable sandboxing
//      as soon as a decent story is available for creating JS extensions with mruby scripts in a sandboxed environment.
//#ifdef _WIN32
//#define CEF_USE_SANDBOX
//#endif

/******************************************************************
 * XError Handlers (so app doesn't crash for no *good* reason)
 ******************************************************************/

#if !defined(_WIN32) && !defined(__APPLE__)
namespace {

int XErrorHandlerImpl(Display *display, XErrorEvent *event) {
  LOG(WARNING)
        << "X error received: "
        << "type " << event->type << ", "
        << "serial " << event->serial << ", "
        << "error_code " << static_cast<int>(event->error_code) << ", "
        << "request_code " << static_cast<int>(event->request_code) << ", "
        << "minor_code " << static_cast<int>(event->minor_code);
  return 0;
}

int XIOErrorHandlerImpl(Display *display) {
  return 0;
}

}
#endif

/*********************************
 * MRB Thread Instance Management
 *********************************/

class MrbThreadPair {
public:
  mrb_state * mrb;
  apr_os_thread_t thread;

  MrbThreadPair(apr_os_thread_t t, mrb_state* mrb) {
    this->mrb = mrb;
    this->thread = t;
  }
};

class MrbThreadManager {
public:
  MrbThreadManager() {
    this->mrb_thread_pairs = std::vector<MrbThreadPair>();
  }

  void check_mutex() {
    // LAMINA_LOG("MrbThreadManager: Making sure mutex exists");
    if (MrbThreadManager::mutex == NULL) {
      // LAMINA_LOG("MrbThreadManager: Mutex is NULL, creating it");
      apr_pool_t* mutex_pool;
      apr_pool_create(&mutex_pool, NULL);
      apr_thread_mutex_create(&MrbThreadManager::mutex, APR_THREAD_MUTEX_DEFAULT, mutex_pool);
    }
    // LAMINA_LOG("MrbThreadManager: Mutex check done");
  }

  void set_mrb_for_thread(mrb_state* mrb) {
    // LAMINA_LOG("MrbThreadManager: Setting MRB for current thread");
    check_mutex();
    apr_thread_mutex_lock(MrbThreadManager::mutex);
    set_mrb_for_thread_no_lock(mrb);
    apr_thread_mutex_unlock(MrbThreadManager::mutex);
  }

  mrb_state* get_mrb_for_thread() {
    // LAMINA_LOG("MrbThreadManager: Getting MRB for current thread");
    check_mutex();
    apr_thread_mutex_lock(MrbThreadManager::mutex);
    apr_os_thread_t t = apr_os_thread_current();
    mrb_state* mrb = NULL;

    for (int i = 0; i < mrb_thread_pairs.size(); i++) {
      if (apr_os_thread_equal(t, mrb_thread_pairs[i].thread)) {
        // LAMINA_LOG("Found existing MRB instance for current thread. #" << i);
        mrb = mrb_thread_pairs[i].mrb;
      }
    }

    if (mrb == NULL) {
      // LAMINA_LOG("No MRB instance found for current thread, creating one");
      mrb_state* mrb = mrb_open();
      RClass* lamina_module = mrb_define_module(mrb, "Lamina");
      mrb_funcall(mrb, mrb_obj_value(lamina_module), "read_lamina_options", 0);
      this->set_mrb_for_thread_no_lock(mrb);
    }

    apr_thread_mutex_unlock(MrbThreadManager::mutex);
    return mrb;
  }

private:
  void set_mrb_for_thread_no_lock(mrb_state* mrb) {
    apr_os_thread_t t = apr_os_thread_current();
    mrb_thread_pairs.push_back(MrbThreadPair(t, mrb));
  }

  std::vector<MrbThreadPair> mrb_thread_pairs;
  static apr_thread_mutex_t* mutex;
};

apr_thread_mutex_t* MrbThreadManager::mutex = NULL;

static MrbThreadManager mrb_thread_manager;

mrb_state* mrb_for_thread() {
   return mrb_thread_manager.get_mrb_for_thread();
}

// Only needs to be called for the first thread of the process
// (Since it is launched as lamina.exe, and an mrb_state will already
//  be available)
void set_mrb_for_thread(mrb_state* mrb) {
  // LAMINA_LOG("Explicitly setting MRB for thead (presumably the main thread).");
  mrb_thread_manager.set_mrb_for_thread(mrb);
}

/*********************************
 * Lamina Ruby Module Functions
 *********************************/

int global_argc = 0;
char** global_argv = NULL;

mrb_value
lamina_start_cef_proc(mrb_state* mrb, mrb_value self) {
  // LAMINA_LOG("Lamina.start_cef_proc");

   void* sandbox_info = NULL;

#if defined(CEF_USE_SANDBOX)
   // Manage the life span of the sandbox information object. This is necessary
   // for sandbox support on Windows. See cef_sandbox_win.h for complete details.
   CefScopedSandboxInfo scoped_sandbox;
   sandbox_info = scoped_sandbox.sandbox_info();
#endif

   // Provide CEF with command-line arguments.
#ifdef WINDOWS
   CefMainArgs main_args;
#else
   CefMainArgs main_args(global_argc, global_argv);
#endif

   // SimpleApp implements application-level callbacks. It will create the first
   // browser instance in OnContextInitialized() after CEF has initialized.
  //  LAMINA_LOG("Lamina.start_cef_proc: Creating App");
   CefRefPtr<LaminaApp> app(new LaminaApp);

  //  LAMINA_LOG("Lamina.start_cef_proc: Getting URL");
   app.get()->url = lamina_opt_app_url();
  //  LAMINA_LOG("Lamina.start_cef_proc: Got URL");

   cout << "APP URL: " << app.get()->url << endl;

   // CEF applications have multiple sub-processes (render, plugin, GPU, etc)
   // that share the same executable. This function checks the command-line and,
   // if this is a sub-process, executes the appropriate logic.

  //  LAMINA_LOG("Lamina.start_cef_proc: Executing CEF Process");
   apr_pool_t* pool;
   apr_pool_create(&pool, NULL);
   apr_env_set("CEF_SUBPROC", "true", pool);
   apr_pool_destroy(pool);
   int exit_code = CefExecuteProcess(main_args, app.get(), sandbox_info);
   if (exit_code >= 0) {
      // LAMINA_LOG("Lamina.start_cef_proc: Subprocess exited");
      // The sub-process has completed so return here.
      return mrb_nil_value();
   }

  //  LAMINA_LOG("Lamina.start_cef_proc: This is a CEF browser process");

   // Specify CEF global settings here.
   CefSettings settings;

#ifndef DEBUG /* Suppresses debug.log output when building in release mode */
   //settings.log_severity = LOGSEVERITY_DISABLE;
#endif

  //  LAMINA_LOG("Lamina.start_cef_proc: Getting the cache path");
   string cache_path = lamina_opt_cache_path();
  //  LAMINA_LOG("Lamina.start_cef_proc: Got the cache path");
   if (cache_path.size() > 0) {
      CefString(&settings.cache_path).FromASCII(cache_path.c_str());
   }

  //  LAMINA_LOG("Lamina.start_cef_proc: Getting the remote debugging port");
   int rdp = lamina_opt_remote_debugging_port();
   if (rdp != 0) {
      settings.remote_debugging_port = rdp;
   }

#if !defined(CEF_USE_SANDBOX)
   settings.no_sandbox = true;
#endif

#if !defined(_WIN32) && !defined(__APPLE__)
  // Install xlib error handlers so that the application won't be terminated
  // on non-fatal errors.
  XSetErrorHandler(XErrorHandlerImpl);
  XSetIOErrorHandler(XIOErrorHandlerImpl);
#endif

  //  LAMINA_LOG("Lamina.start_cef_proc: Initializing CEF");
   CefInitialize(main_args, settings, app.get(), sandbox_info);

  //  LAMINA_LOG("Lamina.start_cef_proc: Running the message loop");
   CefRunMessageLoop();

  //  LAMINA_LOG("Lamina.start_cef_proc: Shutting down CEF");
   CefShutdown();

   return mrb_nil_value();
}

void mrb_mruby_lamina_gem_init(mrb_state* mrb) {
   auto lamina_module = mrb_define_module(mrb, "Lamina");
   mrb_define_class_method(mrb, lamina_module, "start_cef_proc", lamina_start_cef_proc, MRB_ARGS_NONE());
}

void mrb_mruby_lamina_gem_final(mrb_state* mrb) {}

/*********************************
 * Platform agnostic start routine
 *********************************/
int lamina_main()
{
   apr_initialize();
   mrb_state* mrb = mrb_open();
   set_mrb_for_thread(mrb);
   RClass* lamina_module = mrb_define_module(mrb, "Lamina");
   mrb_funcall(mrb, mrb_obj_value(lamina_module), "main", 0);
   if (mrb->exc) {
      LAMINA_LOG("!!! Error !!! " << mrb_str_to_cstr(mrb, mrb_funcall(mrb, mrb_obj_value(mrb->exc), "to_s", 0)));
   }
}

#ifdef __cplusplus
}
#endif
