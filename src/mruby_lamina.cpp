/* System Includes */
#if defined(_WIN32) || defined(_WIN64)
#include <ws2tcpip.h>
#endif
#include <string>
#include <thread>
#include <chrono>
#include <mutex>

/* CEF Includes */
#include "include/cef_app.h"
#include "include/cef_browser.h"
#include "include/cef_command_line.h"
#if defined(_WIN32) || defined(_WIN64)
  #include "include/cef_sandbox_win.h"
#endif
/* APR Includes */
#include "apr_file_io.h"
#include "apr_thread_proc.h"
#include "apr_network_io.h"
#include "apr_env.h"

/* MRuby Includes */
#include "mruby.h"
#include "mruby/compile.h"

/* Lamina Includes */
#include "mruby_lamina.h"
#include "lamina_util.h"
#include "LaminaHandler.h"
#include "LaminaApp.h"
#include "lamina_opt.h"
#include "BrowserMessageServer.h"
#include "BrowserMessageClient.h"

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

std::mutex mrbs_mutex;
map<thread::id, mrb_state*> thread_mrbs;

mrb_state* mrb_for_thread() {
   LAMINA_LOG("mrb_for_thread");
   mrbs_mutex.lock();
   thread::id thread_id = this_thread::get_id();
   mrb_state* mrb;
   try {
      mrb = thread_mrbs.at(thread_id);
      LAMINA_LOG("Found existing mrb for this thread");
   }
   catch (out_of_range ex) {
      LAMINA_LOG("No mrb found for this thread, creating a new one.");
      mrb = mrb_open();
      thread_mrbs[thread_id] = mrb;
   }
   mrbs_mutex.unlock();
   return mrb;
}

// Only needs to be called for the first thread of the process
// (Since it is launched as lamina.exe, and an mrb_state will already
//  be available)
void set_mrb_for_thread(mrb_state* mrb) {
   LAMINA_LOG("set_mrb_for_thread");
   mrbs_mutex.lock();
   thread::id thread_id = this_thread::get_id();
   thread_mrbs[thread_id] = mrb;
   mrbs_mutex.unlock();
}

int global_argc = 0;
char** global_argv = NULL;

mrb_value
lamina_start_cef_proc(mrb_state* mrb, mrb_value self) {
#ifdef DEBUG
   cout << "New Main Process" << endl;
#endif

   void* sandbox_info = NULL;

#if defined(CEF_USE_SANDBOX)
   // Manage the life span of the sandbox information object. This is necessary
   // for sandbox support on Windows. See cef_sandbox_win.h for complete details.
   CefScopedSandboxInfo scoped_sandbox;
   sandbox_info = scoped_sandbox.sandbox_info();
#endif

   // Provide CEF with command-line arguments.
   CefMainArgs main_args(global_argc, global_argv);
   // SimpleApp implements application-level callbacks. It will create the first
   // browser instance in OnContextInitialized() after CEF has initialized.
   CefRefPtr<LaminaApp> app(new LaminaApp);

   app.get()->url = lamina_opt_app_url();

#ifdef DEBUG
   cout << "APP URL: " << app.get()->url << endl;
#endif

   // CEF applications have multiple sub-processes (render, plugin, GPU, etc)
   // that share the same executable. This function checks the command-line and,
   // if this is a sub-process, executes the appropriate logic.
   LAMINA_LOG("Executing CEF Process");
   apr_pool_t* pool;
   apr_pool_create(&pool, NULL);
   apr_env_set("CEF_SUBPROC", "true", pool);
   apr_pool_destroy(pool);
   int exit_code = CefExecuteProcess(main_args, app.get(), sandbox_info);
   if (exit_code >= 0) {
      LAMINA_LOG("Subprocess exited");
      // The sub-process has completed so return here.
      return mrb_nil_value();
   }
   
   LAMINA_LOG("This is a CEF browser process");

   // Specify CEF global settings here.
   CefSettings settings;

#ifndef DEBUG /* Suppresses debug.log output when building in release mode */
   //settings.log_severity = LOGSEVERITY_DISABLE;
#endif

   auto cache_path = lamina_opt_cache_path();
   if (cache_path.size() > 0) {
      CefString(&settings.cache_path).FromASCII(cache_path.c_str());
   }

   int rdp = lamina_opt_remote_debugging_port();
   if (rdp != 0) {
      settings.remote_debugging_port = rdp;
   }
      

#if !defined(CEF_USE_SANDBOX)
   settings.no_sandbox = true;
#endif

   // Initialize CEF.
   CefInitialize(main_args, settings, app.get(), sandbox_info);

   // Run the CEF message loop. This will block until CefQuitMessageLoop() is
   // called.
   CefRunMessageLoop();

   // Shut down CEF.
   CefShutdown();

   return mrb_nil_value();
}

mrb_value
lamina_start_browser_message_server(mrb_state* mrb, mrb_value self) {
   LAMINA_LOG("Starting browser message server");
   // Create new, and do not destroy. Should be running as long as the process is running
   auto browserMessageServer = new BrowserMessageServer();
   browserMessageServer->set_url(lamina_opt_browser_ipc_path());
   browserMessageServer->start();
   return self;
}

// md-doc is already written in mrblib/lamina.rb (just to keep it all in one file)
mrb_value
lamina_open_new_window(mrb_state* mrb, mrb_value self) {
   LAMINA_LOG("Starting browser message client");
   BrowserMessageClient client;
   auto browser_ipc_path = lamina_opt_browser_ipc_path();
   LAMINA_LOG("Sending new_window message");
   client.set_server_url(browser_ipc_path);
   client.send("new_window");
   // Sleep long enough for the message to be delivered
   // just in case the app exists after this.
   // (TODO: Should be a way to flush this without a sleep)
   this_thread::sleep_for(chrono::seconds(1));
   return self;
}

void mrb_mruby_lamina_gem_init(mrb_state* mrb) {
   auto lamina_module = mrb_define_module(mrb, "Lamina");
   mrb_define_class_method(mrb, lamina_module, "start_browser_message_server", lamina_start_browser_message_server, MRB_ARGS_NONE());
   mrb_define_class_method(mrb, lamina_module, "start_cef_proc", lamina_start_cef_proc, MRB_ARGS_NONE());
   mrb_define_class_method(mrb, lamina_module, "open_new_window", lamina_open_new_window, MRB_ARGS_NONE());
}

void mrb_mruby_lamina_gem_final(mrb_state* mrb) {}

#ifdef __cplusplus
}
#endif
